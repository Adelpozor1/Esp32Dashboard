#include "geocoder.h"
#include <ArduinoJson.h>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef ARDUINO
  #include <Arduino.h>
  #define GEO_LOG(...) Serial.printf(__VA_ARGS__)
  #define GEO_LOGLN(x) Serial.println(x)
#else
  #define GEO_LOG(...)  do {} while (0)
  #define GEO_LOGLN(x)  do {} while (0)
#endif

namespace {

std::string urlEncode(const std::string& in) {
  std::string out;
  out.reserve(in.size() * 3);
  char buf[4];
  for (unsigned char c : in) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out.push_back(c);
    } else if (c == ' ') {
      out += "%20";
    } else {
      std::snprintf(buf, sizeof(buf), "%%%02X", c);
      out += buf;
    }
  }
  return out;
}

std::string toLower(const std::string& s) {
  std::string out; out.reserve(s.size());
  for (unsigned char c : s) out.push_back(std::tolower(c));
  return out;
}

// Devuelve el ISO country code (2 letras, minúsculas) para nombres comunes
// de país en español/inglés. Cadena vacía si no reconoce.
std::string paisANominatim(const std::string& p) {
  std::string q = toLower(p);
  // recortar espacios
  while (!q.empty() && q.back() == ' ') q.pop_back();
  while (!q.empty() && q.front() == ' ') q.erase(0, 1);
  if (q == "es" || q == "spain" || q == "españa" || q == "espana") return "es";
  if (q == "fr" || q == "france" || q == "francia") return "fr";
  if (q == "de" || q == "germany" || q == "alemania" || q == "deutschland") return "de";
  if (q == "pt" || q == "portugal") return "pt";
  if (q == "it" || q == "italy" || q == "italia") return "it";
  if (q == "gb" || q == "uk" || q == "united kingdom" || q == "reino unido") return "gb";
  if (q == "us" || q == "usa" || q == "estados unidos") return "us";
  if (q == "nl" || q == "netherlands" || q == "holanda" || q == "países bajos") return "nl";
  if (q == "be" || q == "belgium" || q == "bélgica" || q == "belgica") return "be";
  return "";
}

// Extrae (cp, pais_iso) si `direccion` parece "CP" o "CP país".
// Si no se puede extraer un país reconocido, devuelve "es" por defecto
// cuando el CP tiene el formato español (5 dígitos).
// Devuelve true si detecta un patrón de CP.
bool extraerCodigoPostal(const std::string& direccion,
                         std::string& cpOut, std::string& paisOut) {
  size_t i = 0;
  while (i < direccion.size() && direccion[i] == ' ') ++i;
  std::string cp;
  while (i < direccion.size() && std::isdigit((unsigned char)direccion[i])) {
    cp.push_back(direccion[i]); ++i;
  }
  if (cp.size() < 4 || cp.size() > 5) return false;
  // Saltar espacios
  while (i < direccion.size() && direccion[i] == ' ') ++i;
  std::string resto = (i < direccion.size()) ? direccion.substr(i) : "";
  std::string pais = resto.empty() ? "es" : paisANominatim(resto);
  if (pais.empty()) return false;
  cpOut = cp;
  paisOut = pais;
  return true;
}

bool resolverPorZippopotam(IHttpClient& http, const std::string& pais,
                           const std::string& cp,
                           double& latOut, double& lonOut) {
  std::string url = "http://api.zippopotam.us/" + pais + "/" + cp;
  GEO_LOG("[geo] Zippopotam GET %s\n", url.c_str());
  std::string body;
  int status = 0;
  bool ok = http.get(url, body, status, 8000);
  GEO_LOG("[geo] zippo ok=%d status=%d body_len=%u\n",
                ok ? 1 : 0, status, (unsigned)body.size());
  if (!ok || status != 200 || body.empty()) return false;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    GEO_LOG("[geo] zippo JSON error: %s\n", err.c_str());
    return false;
  }
  JsonArrayConst places = doc["places"].as<JsonArrayConst>();
  if (places.isNull() || places.size() == 0) {
    GEO_LOGLN("[geo] zippo sin places");
    return false;
  }
  const char* latStr = places[0]["latitude"];
  const char* lonStr = places[0]["longitude"];
  if (!latStr || !lonStr) return false;
  latOut = std::strtod(latStr, nullptr);
  lonOut = std::strtod(lonStr, nullptr);
  GEO_LOG("[geo] zippo OK lat=%.4f lon=%.4f\n", latOut, lonOut);
  return true;
}

bool resolverPorNominatim(IHttpClient& http, const std::string& direccion,
                          double& latOut, double& lonOut) {
  std::string url = "https://nominatim.openstreetmap.org/search?format=json&limit=1&q=";
  url += urlEncode(direccion);
  GEO_LOG("[geo] Nominatim GET %s\n", url.c_str());
  std::string body;
  int status = 0;
  bool ok = http.get(url, body, status, 10000);
  GEO_LOG("[geo] nomin ok=%d status=%d body_len=%u\n",
                ok ? 1 : 0, status, (unsigned)body.size());
  if (!ok || status != 200 || body.empty()) return false;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err || !doc.is<JsonArray>() || doc.size() == 0) return false;
  const char* latStr = doc[0]["lat"];
  const char* lonStr = doc[0]["lon"];
  if (!latStr || !lonStr) return false;
  latOut = std::strtod(latStr, nullptr);
  lonOut = std::strtod(lonStr, nullptr);
  GEO_LOG("[geo] nomin OK lat=%.4f lon=%.4f\n", latOut, lonOut);
  return true;
}

}  // namespace

bool Geocoder::resolver(const std::string& direccion, double& latOut, double& lonOut) {
  // Ruta preferente para códigos postales: Zippopotam por HTTP puro
  // (Nominatim HTTPS suele fallar el TLS handshake en la ESP32).
  std::string cp, pais;
  if (extraerCodigoPostal(direccion, cp, pais)) {
    GEO_LOG("[geo] detectado CP=%s pais=%s\n", cp.c_str(), pais.c_str());
    if (resolverPorZippopotam(http_, pais, cp, latOut, lonOut)) return true;
    GEO_LOGLN("[geo] Zippopotam falló, probando Nominatim");
  }
  return resolverPorNominatim(http_, direccion, latOut, lonOut);
}
