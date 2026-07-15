#include "adsb_client.h"
#include "geo_math.h"
#include <ArduinoJson.h>
#include <algorithm>
#include <cstdio>

namespace {

std::string trimEspacios(const char* s) {
  if (!s) return "";
  std::string out(s);
  while (!out.empty() && out.back() == ' ') out.pop_back();
  while (!out.empty() && out.front() == ' ') out.erase(0, 1);
  return out;
}

}  // namespace

bool AdsbClient::fetchCerca(double lat, double lon, int radioKm,
                            std::vector<Aeronave>& out) {
  out.clear();

  char url[192];
  std::snprintf(url, sizeof(url),
                "https://api.adsb.lol/v2/point/%.6f/%.6f/%d",
                lat, lon, radioKm);

  std::string body;
  int status = 0;
  if (!http_.get(url, body, status, 5000) || status != 200 || body.empty()) {
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) return false;

  JsonArrayConst ac = doc["ac"].as<JsonArrayConst>();
  if (ac.isNull()) return true;   // sin campo "ac" pero JSON válido → 0 aviones

  std::vector<Aeronave> temp;
  temp.reserve(ac.size());

  for (JsonObjectConst obj : ac) {
    if (!obj["lat"].is<double>() || !obj["lon"].is<double>()) continue;
    Aeronave a;
    a.hex       = obj["hex"] | "";
    a.callsign  = trimEspacios(obj["flight"] | "");
    a.lat       = obj["lat"].as<double>();
    a.lon       = obj["lon"].as<double>();
    a.alt_ft    = obj["alt_baro"] | 0;
    a.gs_kt     = static_cast<int>(obj["gs"] | 0.0);
    a.track_deg = static_cast<int>(obj["track"] | 0.0);
    a.dist_km   = geo::distanciaKm(lat, lon, a.lat, a.lon);
    a.bearing   = geo::bearingGrados(lat, lon, a.lat, a.lon);
    temp.push_back(std::move(a));
  }

  std::sort(temp.begin(), temp.end(),
            [](const Aeronave& x, const Aeronave& y) { return x.dist_km < y.dist_km; });

  if (temp.size() > MAX_AVIONES) temp.resize(MAX_AVIONES);
  out = std::move(temp);
  return true;
}
