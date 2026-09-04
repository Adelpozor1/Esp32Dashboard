#pragma once
#include "http_client.h"
#include <cstdint>
#include <string>
#include <vector>

struct EventoMotor {
  std::string nombre;
  std::string fechaHora;
  std::string ganador;
  std::string resultado;
};

struct MotogpSnapshot {
  bool     ok = false;
  uint32_t obtenido_ms = 0;
  bool     stale = false;
  std::vector<EventoMotor> ultimos;
  std::vector<EventoMotor> proximos;
};

class MotogpClient {
 public:
  explicit MotogpClient(IHttpClient& http) : http_(http) {}

  bool fetch(MotogpSnapshot& out);
  static bool parsearEventos(const std::string& json,
                             std::vector<EventoMotor>& out,
                             size_t maxN);

 private:
  IHttpClient& http_;
};
