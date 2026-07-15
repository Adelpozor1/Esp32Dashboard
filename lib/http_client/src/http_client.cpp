#include "http_client.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

bool WifiHttpClient::get(const std::string& url,
                         std::string& bodyOut,
                         int& statusOut,
                         int timeoutMs) {
  bodyOut.clear();
  statusOut = 0;
  HTTPClient http;
  http.setTimeout(timeoutMs);
  // Nominatim exige User-Agent identificable.
  http.setUserAgent("radar-vuelos-esp32/1.0 (albertodelpozo)");
  bool ok;
  if (url.rfind("https://", 0) == 0) {
    WiFiClientSecure client;
    client.setInsecure();  // Sin validación de certificado (aceptable para APIs públicas de solo lectura).
    ok = http.begin(client, url.c_str());
  } else {
    ok = http.begin(url.c_str());
  }
  if (!ok) return false;
  int code = http.GET();
  statusOut = code;
  if (code <= 0) { http.end(); return false; }
  String payload = http.getString();
  bodyOut.assign(payload.c_str(), payload.length());
  http.end();
  return true;
}
#endif
