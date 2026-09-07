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

bool WifiHttpClient::getStreamed(const std::string& url,
                                 int& statusOut,
                                 int timeoutMs,
                                 std::function<bool(void* stream)> cb) {
  statusOut = 0;
  HTTPClient http;
  http.setTimeout(timeoutMs);
  // HTTP/1.0 desactiva chunked transfer encoding: todo el body llega en un
  // stream continuo sin frames de longitud intercalados, y deserializeJson
  // lee limpiamente. Con chunked (HTTP/1.1 por defecto) el parse a veces
  // aborta antes de tiempo porque `available()` devuelve 0 entre chunks.
  http.useHTTP10(true);
  http.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:120.0) Gecko/20100101 Firefox/120.0");
  WiFiClientSecure secureClient;
  bool ok;
  if (url.rfind("https://", 0) == 0) {
    secureClient.setInsecure();
    ok = http.begin(secureClient, url.c_str());
  } else {
    ok = http.begin(url.c_str());
  }
  if (!ok) return false;
  http.addHeader("Accept", "application/json");
  http.addHeader("Accept-Encoding", "identity");
  int code = http.GET();
  statusOut = code;
  if (code != 200) { http.end(); return false; }
  // http.getStreamPtr() devuelve el stream que va leyendo directamente del
  // socket TLS. deserializeJson lee de él sin acumular en un String — clave
  // para respuestas grandes (>10KB) que reventarían el heap con getString().
  WiFiClient* stream = http.getStreamPtr();
  bool res = false;
  if (stream) res = cb(static_cast<void*>(stream));
  http.end();
  return res;
}
#endif
