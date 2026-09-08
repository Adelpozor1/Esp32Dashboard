#include "wifi_portal.h"
#include "geocoder.h"
#include "status_led.h"
#include "qr_view.h"
#include "tft_driver.h"
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <vector>

namespace {

String macSufijo() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char buf[5];
  snprintf(buf, sizeof(buf), "%02X%02X", mac[4], mac[5]);
  return String(buf);
}

// Cache del último scan y flag para pedir rescan desde otro thread.
// WiFi.scanNetworks(sync) NO puede llamarse desde el handler de AsyncWebServer
// porque bloquea el thread async_tcp > 5 s y dispara el Task Watchdog.
String s_scanCache = "[]";
volatile bool s_pedirScan = true;

// Estado del proceso de guardado (que también se saca del thread async_tcp).
enum class EstadoGuardar { OCIOSO, PROCESANDO, ERROR };
volatile EstadoGuardar s_estadoGuardar = EstadoGuardar::OCIOSO;
String s_ultimoError = "";

struct DatosGuardar {
  std::string ssid;
  std::string password;
  std::string direccion;
  int radio_km;
  IHttpClient* http;
};

// Recupera el AP tras un intento fallido de conexión para que el usuario pueda
// volver a llenar el formulario sin reflashear ni power-cycle.
static String s_ssidApGuardado;
void relevantarAP() {
  Serial.println("[save] relevantando AP para reintento");
  WiFi.disconnect(true, true);
  vTaskDelay(pdMS_TO_TICKS(200));
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(s_ssidApGuardado.c_str());
}

const char* nombreWlStatus(wl_status_t s) {
  switch (s) {
    case WL_NO_SHIELD:       return "NO_SHIELD";
    case WL_IDLE_STATUS:     return "IDLE";
    case WL_NO_SSID_AVAIL:   return "NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED:  return "SCAN_COMPLETED";
    case WL_CONNECTED:       return "CONNECTED";
    case WL_CONNECT_FAILED:  return "CONNECT_FAILED";
    case WL_CONNECTION_LOST: return "CONNECTION_LOST";
    case WL_DISCONNECTED:    return "DISCONNECTED";
    default:                 return "?";
  }
}

void tareaGuardar(void* param) {
  auto* d = static_cast<DatosGuardar*>(param);
  Serial.printf("[save] preparando STA-only para conectar a '%s'\n", d->ssid.c_str());
  // Bajamos el AP y limpiamos la STA para que el radio tenga solo un canal que
  // gestionar. Con AP+STA activos, WiFi.begin a veces falla porque el AP fija
  // canal e interfiere con el hopping del STA.
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_STA);
  vTaskDelay(pdMS_TO_TICKS(300));

  Serial.printf("[save] WiFi.begin('%s')\n", d->ssid.c_str());
  WiFi.begin(d->ssid.c_str(), d->password.c_str());
  uint32_t inicio = millis();
  wl_status_t status = WL_IDLE_STATUS;
  while (millis() - inicio < 30000) {
    status = WiFi.status();
    if (status == WL_CONNECTED) break;
    // WL_NO_SSID_AVAIL sale rápido si el SSID no aparece — no esperamos 30s.
    if (status == WL_NO_SSID_AVAIL && millis() - inicio > 8000) break;
    if (status == WL_CONNECT_FAILED) break;
    vTaskDelay(pdMS_TO_TICKS(200));
  }
  status = WiFi.status();
  Serial.printf("[save] tras begin: status=%s (%d)\n",
                nombreWlStatus(status), (int)status);
  if (status != WL_CONNECTED) {
    if (status == WL_NO_SSID_AVAIL) {
      s_ultimoError = "SSID no encontrado. Comprueba el nombre exacto (mayusculas/minusculas y espacios). Recuerda: el ESP32 solo soporta 2.4 GHz.";
    } else if (status == WL_CONNECT_FAILED) {
      s_ultimoError = "Password incorrecta o red rechazando la conexion.";
    } else {
      s_ultimoError = "No se puede conectar a la WiFi. Comprueba SSID, password y que la red sea 2.4 GHz (WPA2).";
    }
    s_estadoGuardar = EstadoGuardar::ERROR;
    relevantarAP();
    delete d;
    vTaskDelete(nullptr);
    return;
  }
  Serial.printf("[save] WiFi OK, IP: %s\n", WiFi.localIP().toString().c_str());

  Serial.println("[save] geocoding...");
  Geocoder g(*d->http);
  double lat = 0, lon = 0;
  if (!g.resolver(d->direccion, lat, lon)) {
    Serial.println("[save] geocoding falló");
    s_ultimoError = "Ubicación no encontrada. Prueba con código postal + país (ej: 28013 España).";
    s_estadoGuardar = EstadoGuardar::ERROR;
    relevantarAP();
    delete d;
    vTaskDelete(nullptr);
    return;
  }
  Serial.printf("[save] geocoding OK: %.4f,%.4f\n", lat, lon);

  Config cfg;
  cfg.ssid = d->ssid;
  cfg.password = d->password;
  cfg.direccion = d->direccion;
  cfg.lat = lat;
  cfg.lon = lon;
  cfg.radio_km = d->radio_km;
  if (!ConfigStore::guardar(cfg)) {
    Serial.println("[save] error NVS");
    s_ultimoError = "Error guardando en NVS.";
    s_estadoGuardar = EstadoGuardar::ERROR;
    relevantarAP();
    delete d;
    vTaskDelete(nullptr);
    return;
  }
  Serial.println("[save] guardado OK, reiniciando en 500ms");
  delete d;
  delay(500);
  ESP.restart();
  // no vuelve
}

void tareaScanPortal(void*) {
  for (;;) {
    if (s_pedirScan) {
      s_pedirScan = false;
      Serial.println("[scan] ejecutando WiFi.scanNetworks()...");
      int n = WiFi.scanNetworks(false, true);
      Serial.printf("[scan] devolvió %d redes\n", n);
      // Si el rescan devuelve 0 (típico en AP+STA), preservamos la cache
      // anterior — así el usuario no pierde la lista inicial cuando pulsa 🔄.
      if (n > 0) {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (int i = 0; i < n && i < 32; ++i) {
          String ssid = WiFi.SSID(i);
          if (ssid.length() == 0) continue;
          JsonObject o = arr.add<JsonObject>();
          o["ssid"] = ssid;
          o["rssi"] = WiFi.RSSI(i);
          o["open"] = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
        }
        String out;
        serializeJson(doc, out);
        s_scanCache = out;
        Serial.printf("[scan] cache actualizado (%u bytes)\n", (unsigned)out.length());
      } else {
        Serial.println("[scan] devolvió 0 — conservamos cache anterior");
      }
      WiFi.scanDelete();
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

}  // namespace

void WifiPortal::ejecutar(IHttpClient& http, const Config* cfgPrevia) {
  StatusLed::setEstado(EstadoLed::PORTAL);

  String ssidAp = String("RadarVuelos-") + macSufijo();
  s_ssidApGuardado = ssidAp;

  // 1) Scan INICIAL en modo STA-only. Con AP+STA el scan a veces devuelve 0
  //    redes (el radio wifi solo tiene un canal a la vez y el AP fija el canal),
  //    así que hacemos el primer scan aquí sin el AP para llenar la cache.
  Serial.println("[portal] scan inicial en STA-only...");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  vTaskDelay(pdMS_TO_TICKS(200));
  {
    int n = WiFi.scanNetworks(false, true);
    Serial.printf("[portal] scan inicial devolvio %d redes\n", n);
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < n && i < 32; ++i) {
      String ssid = WiFi.SSID(i);
      if (ssid.length() == 0) continue;
      JsonObject o = arr.add<JsonObject>();
      o["ssid"] = ssid;
      o["rssi"] = WiFi.RSSI(i);
      o["open"] = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
    }
    WiFi.scanDelete();
    String out;
    serializeJson(doc, out);
    s_scanCache = out;
    s_pedirScan = false;   // ya tenemos algo, no rescan-cortoplacista
  }

  // 2) Ahora sí subimos AP+STA para el portal.
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ssidAp.c_str());
  Serial.printf("[portal] AP levantado: %s  IP: %s\n",
                ssidAp.c_str(), WiFi.softAPIP().toString().c_str());

  // Task dedicada de scan en core 1 (fuera del thread async_tcp). Este scan
  // corre en AP+STA y puede fallar a veces; la cache inicial del paso 1 nos
  // asegura que el usuario ve algo en la lista aunque el rescan salga vacío.
  xTaskCreatePinnedToCore(tareaScanPortal, "scan", 4096, nullptr, 1, nullptr, 1);

  static AsyncWebServer server(80);

  // IMPORTANTE: registrar las rutas específicas ANTES del serveStatic catch-all,
  // si no /api/scan se resuelve como fichero estático y falla con 404.
  server.on("/api/scan", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "application/json", s_scanCache);
  });
  server.on("/api/rescan", HTTP_POST, [](AsyncWebServerRequest* req) {
    s_pedirScan = true;
    req->send(200, "text/plain", "OK");
  });

  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* req) {
    JsonDocument doc;
    switch (s_estadoGuardar) {
      case EstadoGuardar::OCIOSO:      doc["estado"] = "ocioso";     break;
      case EstadoGuardar::PROCESANDO:  doc["estado"] = "procesando"; break;
      case EstadoGuardar::ERROR:       doc["estado"] = "error";
                                       doc["error"] = s_ultimoError; break;
    }
    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  server.on("/api/save", HTTP_POST,
    [](AsyncWebServerRequest*) {},
    nullptr,
    [&http](AsyncWebServerRequest* req, uint8_t* data, size_t len,
            size_t /*index*/, size_t /*total*/) {
      if (s_estadoGuardar == EstadoGuardar::PROCESANDO) {
        req->send(409, "text/plain", "Ya hay un guardado en curso");
        return;
      }
      JsonDocument doc;
      if (deserializeJson(doc, data, len)) {
        req->send(400, "text/plain", "JSON inválido");
        return;
      }
      std::string ssid  = doc["ssid"]      | "";
      std::string pass  = doc["password"]  | "";
      std::string dir   = doc["direccion"] | "";
      int radio         = doc["radio_km"]  | 25;
      if (ssid.empty() || pass.empty() || dir.empty()) {
        req->send(400, "text/plain", "Campos obligatorios vacíos");
        return;
      }
      // El trabajo pesado (WiFi.begin + geocoding + guardar + reboot) se
      // saca del thread async_tcp para evitar Task Watchdog. La task se
      // auto-elimina con vTaskDelete al terminar.
      auto* d = new DatosGuardar{ssid, pass, dir, radio, &http};
      s_ultimoError = "";
      s_estadoGuardar = EstadoGuardar::PROCESANDO;
      xTaskCreatePinnedToCore(tareaGuardar, "guardar", 8192, d, 1, nullptr, 1);
      req->send(202, "text/plain", "Procesando");
    });

  // serveStatic va DESPUÉS de las rutas específicas para no eclipsarlas.
  server.serveStatic("/", LittleFS, "/").setDefaultFile("portal.html");

  server.begin();
  Serial.println("[portal] esperando configuración...");

  // Pintar QR del AP en pantalla. Panel derecho: font 2 ~8 chars por línea,
  // hasta ~10 líneas. Trunca strings largos.
  {
    auto trunc = [](const std::string& s, size_t n) {
      if (s.size() <= n) return s;
      return s.substr(0, n - 1) + ".";
    };
    std::vector<std::string> lineas = {
      "AP:",
      trunc(std::string(ssidAp.c_str()), 8),
      "",
      "IP:",
      "192.168.",
      "4.1",
    };
    if (cfgPrevia) {
      char radioBuf[16];
      std::snprintf(radioBuf, sizeof(radioBuf), "Radio %dkm", cfgPrevia->radio_km);
      lineas.push_back("");
      lineas.push_back("Anterior:");
      lineas.push_back(trunc(cfgPrevia->ssid, 8));
    } else {
      lineas.push_back("");
      lineas.push_back("Escanea");
      lineas.push_back("el QR");
    }
    qr_view::pintarPortalConQR(tft_driver::obtenerTft(), "Modo Portal",
                               "http://192.168.4.1/", lineas);
  }

  for (;;) {
    delay(1000);
  }
}
