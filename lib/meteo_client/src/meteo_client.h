#pragma once
#include "http_client.h"
#include <cstdint>
#include <string>
#include <vector>

enum class IconoMeteo : uint8_t { SOL=0, NUBE=1, LLUVIA=2, NIEVE=3, TORMENTA=4, NIEBLA=5, SOL_NUBE=6 };

struct MeteoSnapshot {
  bool     ok = false;
  uint32_t obtenido_ms = 0;
  float    temp_actual_c = 0.0f;
  int      codigo_actual = 0;
  int      viento_kmh = 0;
  bool     stale = false;
  struct Hora { int8_t hora; float temp_c; int codigo; };
  struct Dia  { int8_t dia_mes; int8_t mes; int8_t dia_semana; float tmin; float tmax; int codigo; };
  std::vector<Hora> horas;
  std::vector<Dia>  dias;
};

class MeteoClient {
 public:
  explicit MeteoClient(IHttpClient& http) : http_(http) {}

  bool fetch(double lat, double lon, MeteoSnapshot& out);
  static bool parsear(const std::string& json, MeteoSnapshot& out);
  static IconoMeteo categoria(int wmo);

 private:
  IHttpClient& http_;
};
