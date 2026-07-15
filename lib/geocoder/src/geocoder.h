#pragma once
#include "http_client.h"
#include <string>

class Geocoder {
 public:
  explicit Geocoder(IHttpClient& http) : http_(http) {}

  // Resuelve una dirección postal a coordenadas usando Nominatim (OpenStreetMap).
  // Devuelve true si encontró resultado, false si no o hubo error de red/parseo.
  // No modifica latOut/lonOut si devuelve false.
  bool resolver(const std::string& direccion, double& latOut, double& lonOut);

 private:
  IHttpClient& http_;
};
