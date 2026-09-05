#pragma once
#include "http_client.h"
#include <cstdint>
#include <string>
#include <vector>

struct F1Piloto {
  int         posicion = 0;
  std::string nombre;
  std::string equipo;
  std::string tiempo;    // "1:32:35.045" o "+5.123" o "" si DNF
};

struct F1Carrera {
  int         ronda = 0;
  std::string nombreGp;
  std::string circuito;
  std::string fechaHora;  // "YYYY-MM-DD HH:MM"
};

struct F1PilotoClas {
  int         posicion = 0;    // 1..N
  std::string nombre;          // familyName
  std::string equipo;          // constructor name
  int         puntos = 0;
};

struct F1Snapshot {
  bool     ok = false;
  uint32_t obtenido_ms = 0;
  bool     stale = false;
  F1Carrera ultima;
  std::vector<F1Piloto> podio;
  std::vector<F1Carrera> proximas;
  std::vector<F1PilotoClas> clasificacion;
};

class F1Client {
 public:
  explicit F1Client(IHttpClient& http) : http_(http) {}

  bool fetch(F1Snapshot& out);

  static bool parsearUltima(const std::string& json,
                            F1Carrera& carrera,
                            std::vector<F1Piloto>& podio);

  static bool parsearCalendario(const std::string& json,
                                std::vector<F1Carrera>& proximas,
                                size_t maxN);

  static bool parsearClasificacion(const std::string& json,
                                   std::vector<F1PilotoClas>& out,
                                   size_t maxN);

 private:
  IHttpClient& http_;
};
