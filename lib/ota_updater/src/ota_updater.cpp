#include "ota_updater.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <ArduinoJson.h>

// Firmware.json publicado por GitHub Actions en la release "latest" del repo.
// Los assets de una GitHub Release se sirven con redirects (302 → CDN).
// HTTPClient necesita setFollowRedirects para no cortar en el 302.
namespace {
constexpr const char* URL_MANIFEST =
  "https://github.com/Adelpozor1/Esp32Dashboard/releases/download/latest/firmware.json";
}

const char* OtaUpdater::versionActual() {
#ifdef FIRMWARE_VERSION
  return FIRMWARE_VERSION;
#else
  return "dev-local";
#endif
}

bool OtaUpdater::comprobarVersionRemota(OtaVersionInfo& out) {
  HTTPClient http;
  http.setTimeout(20000);
  http.setUserAgent("ESP32-Radar-Vuelos-OTA");
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  WiFiClientSecure secureClient;
  secureClient.setInsecure();
  if (!http.begin(secureClient, URL_MANIFEST)) return false;
  http.addHeader("Accept", "application/json");
  http.addHeader("Accept-Encoding", "identity");
  int code = http.GET();
  if (code != 200) {
    ::Serial.printf("[ota] manifest HTTP=%d\n", code);
    http.end();
    return false;
  }
  String payload = http.getString();
  http.end();
  JsonDocument doc;
  if (deserializeJson(doc, payload)) return false;
  const char* v = doc["version"] | (const char*)nullptr;
  const char* s = doc["sha"]     | (const char*)nullptr;
  const char* u = doc["url"]     | (const char*)nullptr;
  if (!v || !u) return false;
  out.version = v;
  out.sha     = s ? s : "";
  out.size    = doc["size"] | 0;
  out.url     = u;
  return true;
}

bool OtaUpdater::aplicarActualizacion(const OtaVersionInfo& info) {
  ::Serial.printf("[ota] descargando %s (%u bytes)\n",
                  info.version.c_str(), (unsigned)info.size);
  HTTPClient http;
  http.setTimeout(60000);
  http.setUserAgent("ESP32-Radar-Vuelos-OTA");
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  WiFiClientSecure secureClient;
  secureClient.setInsecure();
  if (!http.begin(secureClient, info.url.c_str())) {
    ::Serial.println("[ota] http.begin FAIL");
    return false;
  }
  int code = http.GET();
  if (code != 200) {
    ::Serial.printf("[ota] firmware.bin HTTP=%d\n", code);
    http.end();
    return false;
  }
  int total = http.getSize();
  if (total <= 0 && info.size > 0) total = static_cast<int>(info.size);
  if (total <= 0) {
    ::Serial.println("[ota] tamano desconocido");
    http.end();
    return false;
  }
  if (!Update.begin(total)) {
    ::Serial.printf("[ota] Update.begin FAIL: %s\n", Update.errorString());
    http.end();
    return false;
  }
  WiFiClient* stream = http.getStreamPtr();
  if (!stream) { http.end(); return false; }
  size_t written = Update.writeStream(*stream);
  if (written != (size_t)total) {
    ::Serial.printf("[ota] escritos %u / %d\n", (unsigned)written, total);
    Update.abort();
    http.end();
    return false;
  }
  if (!Update.end(true)) {
    ::Serial.printf("[ota] Update.end FAIL: %s\n", Update.errorString());
    http.end();
    return false;
  }
  http.end();
  ::Serial.println("[ota] actualizacion OK, reiniciando");
  delay(500);
  ESP.restart();
  return true;   // no llega
}

#else  // !ARDUINO — stubs para el entorno native (no se usa allí)

const char* OtaUpdater::versionActual() { return "native-test"; }
bool OtaUpdater::comprobarVersionRemota(OtaVersionInfo&) { return false; }
bool OtaUpdater::aplicarActualizacion(const OtaVersionInfo&) { return false; }

#endif
