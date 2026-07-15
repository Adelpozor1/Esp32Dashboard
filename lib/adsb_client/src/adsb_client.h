#pragma once
#include "http_client.h"
#include <string>
#include <vector>

struct Aeronave {
  std::string hex;
  std::string callsign;
  double lat = 0.0;
  double lon = 0.0;
  int    alt_ft = 0;
  int    gs_kt = 0;
  int    track_deg = 0;
  double dist_km = 0.0;
  int    bearing = 0;
};

class AdsbClient {
 public:
  static constexpr size_t MAX_AVIONES = 50;

  explicit AdsbClient(IHttpClient& http) : http_(http) {}

  // Consulta ADSB.lol y devuelve los aviones dentro de `radioKm` de (lat,lon).
  // Rellena dist_km y bearing por cada uno; ordena por distancia ascendente y
  // trunca a MAX_AVIONES. Devuelve false si hubo error de red o parseo.
  bool fetchCerca(double lat, double lon, int radioKm, std::vector<Aeronave>& out);

 private:
  IHttpClient& http_;
};
