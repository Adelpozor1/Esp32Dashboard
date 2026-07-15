#include "geocoder.h"
#include <ArduinoJson.h>
#include <cctype>
#include <cstdio>
#include <cstdlib>

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

}  // namespace

bool Geocoder::resolver(const std::string& direccion, double& latOut, double& lonOut) {
  std::string url = "https://nominatim.openstreetmap.org/search?format=json&limit=1&q=";
  url += urlEncode(direccion);

  std::string body;
  int status = 0;
  if (!http_.get(url, body, status, 5000) || status != 200 || body.empty()) {
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) return false;
  if (!doc.is<JsonArray>() || doc.size() == 0) return false;

  auto primero = doc[0];
  const char* latStr = primero["lat"];
  const char* lonStr = primero["lon"];
  if (!latStr || !lonStr) return false;

  char* endLat = nullptr;
  char* endLon = nullptr;
  double lat = std::strtod(latStr, &endLat);
  double lon = std::strtod(lonStr, &endLon);
  if (endLat == latStr || endLon == lonStr) return false;

  latOut = lat;
  lonOut = lon;
  return true;
}
