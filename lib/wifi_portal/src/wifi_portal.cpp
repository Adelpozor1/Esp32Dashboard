#include "wifi_portal.h"
#include "geocoder.h"
#include "status_led.h"
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

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

void tareaGuardar(void* param) {
  auto* d = static_cast<DatosGuardar*>(param);
  Serial.printf("[save] conectando a %s...\n", d->ssid.c_str());
  WiFi.begin(d->ssid.c_str(), d->password.c_str());
  uint32_t inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 20000) {
    vTaskDelay(pdMS_TO_TICKS(200));
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[save] WiFi no conecta");
    s_ultimoError = "No se puede conectar a la WiFi. Comprueba SSID y password.";
    s_estadoGuardar = EstadoGuardar::ERROR;
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
      int n = WiFi.scanNetworks(false, false);
      Serial.printf("[scan] devolvió %d redes\n", n);
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
      Serial.printf("[scan] cache actualizado (%u bytes)\n", (unsigned)out.length());
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

}  // namespace

void WifiPortal::ejecutar(IHttpClient& http) {
  StatusLed::setEstado(EstadoLed::PORTAL);

  String ssidAp = String("RadarVuelos-") + macSufijo();
  // AP+STA para poder levantar el portal y a la vez escanear redes cercanas.
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ssidAp.c_str());
  Serial.printf("[portal] AP levantado: %s  IP: %s\n",
                ssidAp.c_str(), WiFi.softAPIP().toString().c_str());

  // Task dedicada de scan en core 1 (fuera del thread async_tcp).
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
  for (;;) {
    delay(1000);
  }
}
