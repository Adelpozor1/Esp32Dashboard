#pragma once
#include "http_client.h"
#include <cstdint>
#include <string>
#include <vector>

struct Partido {
  std::string local;
  std::string visitante;
  std::string fechaHora;   // "YYYY-MM-DD HH:MM"
  int         golesLocal = -1;      // -1 si aún no jugado
  int         golesVisitante = -1;
};

struct FutbolSnapshot {
  bool     ok = false;
  uint32_t obtenido_ms = 0;
  bool     stale = false;
  std::vector<Partido> ultimos;
  std::vector<Partido> proximos;
};

class FutbolClient {
 public:
  explicit FutbolClient(IHttpClient& http) : http_(http) {}

  bool fetch(FutbolSnapshot& out);
  static bool parsearEventos(const std::string& json,
                             std::vector<Partido>& out,
                             size_t maxN);

 private:
  IHttpClient& http_;
};
