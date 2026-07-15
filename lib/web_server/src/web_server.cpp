#include "web_server.h"
#include "geocoder.h"
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

AsyncWebServer server(80);
Config          s_cfg;
RadarState*     s_estado = nullptr;
IHttpClient*    s_http = nullptr;

// Cache del scan de redes vecinas para /config. Igual que en el portal:
// scanNetworks(sync) no puede correr en el thread async_tcp del AsyncWebServer.
String s_scanCache = "[]";
volatile bool s_pedirScan = false;

void tareaScanWeb(void*) {
  for (;;) {
    if (s_pedirScan) {
      s_pedirScan = false;
      Serial.println("[web-scan] ejecutando WiFi.scanNetworks()...");
      int n = WiFi.scanNetworks(false, false);
      Serial.printf("[web-scan] devolvió %d redes\n", n);
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
      Serial.printf("[web-scan] cache actualizado (%u bytes)\n", (unsigned)out.length());
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void handleAircraft(AsyncWebServerRequest* req) {
  Snapshot snap = s_estado->snapshot();
  JsonDocument doc;
  auto c = doc["center"].to<JsonObject>();
  c["lat"] = snap.lat;
  c["lon"] = snap.lon;
  doc["radio_km"] = snap.radio_km;
  doc["ts"]       = snap.ts;
  doc["stale"]    = snap.stale;
  auto arr = doc["aircraft"].to<JsonArray>();
  for (const auto& a : snap.aeronaves) {
    auto o = arr.add<JsonObject>();
    o["hex"]     = a.hex;
    o["cs"]      = a.callsign;
    o["lat"]     = a.lat;
    o["lon"]     = a.lon;
    o["alt_ft"] = a.alt_ft;
    o["gs"]     = a.gs_kt;
    o["trk"]    = a.track_deg;
    o["dist_km"] = a.dist_km;
    o["bearing"] = a.bearing;
  }
  String out;
  serializeJson(doc, out);
  req->send(200, "application/json", out);
}

void handleConfigGet(AsyncWebServerRequest* req) {
  JsonDocument doc;
  doc["ssid"]      = s_cfg.ssid;
  doc["password"]  = s_cfg.password;
  doc["direccion"] = s_cfg.direccion;
  doc["radio_km"]  = s_cfg.radio_km;
  String out;
  serializeJson(doc, out);
  req->send(200, "application/json", out);
}

void handleConfigPost(AsyncWebServerRequest* req, uint8_t* data, size_t len,
                      size_t /*index*/, size_t /*total*/) {
  JsonDocument doc;
  if (deserializeJson(doc, data, len)) {
    req->send(400, "text/plain", "JSON inválido");
    return;
  }
  std::string ssid = doc["ssid"]      | "";
  std::string pass = doc["password"]  | "";
  std::string dir  = doc["direccion"] | "";
  int radio        = doc["radio_km"]  | 25;
  if (ssid.empty() || pass.empty() || dir.empty()) {
    req->send(400, "text/plain", "Campos obligatorios vacíos");
    return;
  }
  double lat = s_cfg.lat, lon = s_cfg.lon;
  if (dir != s_cfg.direccion) {
    Geocoder g(*s_http);
    if (!g.resolver(dir, lat, lon)) {
      req->send(400, "text/plain",
                "Ubicación no encontrada. Prueba con código postal + país (ej: 28013 España) o dirección completa.");
      return;
    }
  }
  Config nueva;
  nueva.ssid = ssid;
  nueva.password = pass;
  nueva.direccion = dir;
  nueva.lat = lat;
  nueva.lon = lon;
  nueva.radio_km = radio;
  if (!ConfigStore::guardar(nueva)) {
    req->send(500, "text/plain", "Error guardando en NVS");
    return;
  }
  req->send(200, "text/plain", "OK, reiniciando");
  delay(500);
  ESP.restart();
}

void handleReset(AsyncWebServerRequest* req) {
  ConfigStore::borrar();
  req->send(200, "text/plain", "Config borrada, reiniciando");
  delay(500);
  ESP.restart();
}

void handleScan(AsyncWebServerRequest* req) {
  req->send(200, "application/json", s_scanCache);
}

void handleRescan(AsyncWebServerRequest* req) {
  s_pedirScan = true;
  req->send(200, "text/plain", "OK");
}

}  // namespace

void RadarWebServer::iniciar(const Config& cfg, RadarState& estado, IHttpClient& http) {
  s_cfg = cfg;
  s_estado = &estado;
  s_http = &http;

  // Task de scan en core 1. Se dispara con /api/rescan; scanNetworks(sync)
  // no puede correr en el thread async_tcp del AsyncWebServer sin romper WDT.
  xTaskCreatePinnedToCore(tareaScanWeb, "webscan", 4096, nullptr, 1, nullptr, 1);

  // IMPORTANTE: rutas específicas primero, serveStatic al final.
  server.on("/config", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(LittleFS, "/config.html", "text/html");
  });
  server.on("/api/aircraft", HTTP_GET, handleAircraft);
  server.on("/api/config",   HTTP_GET, handleConfigGet);
  server.on("/api/config",   HTTP_POST,
            [](AsyncWebServerRequest*) {}, nullptr, handleConfigPost);
  server.on("/api/reset",    HTTP_POST, handleReset);
  server.on("/api/scan",     HTTP_GET, handleScan);
  server.on("/api/rescan",   HTTP_POST, handleRescan);
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  server.begin();
  Serial.println("[web] servidor iniciado");
}
