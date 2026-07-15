#include "wifi_portal.h"
#include "geocoder.h"
#include "status_led.h"
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

namespace {

String macSufijo() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char buf[5];
  snprintf(buf, sizeof(buf), "%02X%02X", mac[4], mac[5]);
  return String(buf);
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

  static AsyncWebServer server(80);
  server.serveStatic("/", LittleFS, "/").setDefaultFile("portal.html");

  server.on("/api/scan", HTTP_GET, [](AsyncWebServerRequest* req) {
    int n = WiFi.scanNetworks(/*async=*/false, /*show_hidden=*/false);
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < n; ++i) {
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
    req->send(200, "application/json", out);
  });

  server.on("/api/save", HTTP_POST,
    [](AsyncWebServerRequest*) {},
    nullptr,
    [&http](AsyncWebServerRequest* req, uint8_t* data, size_t len,
            size_t /*index*/, size_t /*total*/) {
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
      // Antes de geocoding, conectar a la WiFi indicada — Nominatim
      // requiere internet y en Modo Portal solo teníamos el AP levantado.
      Serial.printf("[portal] conectando a %s para geocoding...\n", ssid.c_str());
      WiFi.begin(ssid.c_str(), pass.c_str());
      uint32_t inicio = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - inicio < 20000) {
        delay(200);
      }
      if (WiFi.status() != WL_CONNECTED) {
        req->send(400, "text/plain",
                  "No se puede conectar a la WiFi indicada. Comprueba SSID y password.");
        return;
      }
      Serial.printf("[portal] WiFi conectada, IP: %s\n",
                    WiFi.localIP().toString().c_str());

      Geocoder g(http);
      double lat = 0, lon = 0;
      if (!g.resolver(dir, lat, lon)) {
        req->send(400, "text/plain",
                  "Ubicación no encontrada. Prueba con código postal + país (ej: 28013 España) o dirección completa.");
        return;
      }
      Config cfg;
      cfg.ssid = ssid;
      cfg.password = pass;
      cfg.direccion = dir;
      cfg.lat = lat;
      cfg.lon = lon;
      cfg.radio_km = radio;
      if (!ConfigStore::guardar(cfg)) {
        req->send(500, "text/plain", "Error guardando en NVS");
        return;
      }
      req->send(200, "text/plain", "OK, reiniciando");
      delay(500);
      ESP.restart();
    });

  server.begin();
  Serial.println("[portal] esperando configuración...");
  for (;;) {
    delay(1000);
  }
}
