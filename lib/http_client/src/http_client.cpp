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
  // Cloudflare (TheSportsDB, Jolpica) bloquea user-agents "bot-like".
  http.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:120.0) Gecko/20100101 Firefox/120.0");

  // El WiFiClientSecure debe seguir vivo hasta que http.end() termine.
  // Si se declara dentro del `if`, sale de scope antes de http.GET() y
  // HTTPClient acaba mandando plain HTTP al puerto 443 (bug reportado por
  // el propio nginx: "plain HTTP request was sent to HTTPS port").
  WiFiClientSecure secureClient;
  bool ok;
  if (url.rfind("https://", 0) == 0) {
    secureClient.setInsecure();  // Sin validación de cert (aceptable para APIs de solo lectura).
    ok = http.begin(secureClient, url.c_str());
  } else {
    ok = http.begin(url.c_str());
  }
  if (!ok) return false;

  http.addHeader("Accept", "application/json");
  http.addHeader("Accept-Encoding", "identity");
  int code = http.GET();
  statusOut = code;
  if (code <= 0) { http.end(); return false; }
  String payload = http.getString();
  bodyOut.assign(payload.c_str(), payload.length());
  http.end();
  return true;
}
#endif
