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
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssidAp.c_str());
  Serial.printf("[portal] AP levantado: %s  IP: %s\n",
                ssidAp.c_str(), WiFi.softAPIP().toString().c_str());

  static AsyncWebServer server(80);
  server.serveStatic("/", LittleFS, "/").setDefaultFile("portal.html");

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
      Geocoder g(http);
      double lat = 0, lon = 0;
      if (!g.resolver(dir, lat, lon)) {
        req->send(400, "text/plain",
                  "Dirección no encontrada. Prueba a añadir ciudad y país.");
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
