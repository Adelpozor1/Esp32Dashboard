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

  // Extrae intRound y strSeason del primer evento del JSON (para poder pedir
  // las siguientes rondas). Devuelve true si ambos se encuentran.
  static bool parsearUltimaRondaYSeason(const std::string& json,
                                        int& outRonda,
                                        std::string& outSeason);

  // Del strEvent de una sesión ("San Marino Free Practice 1", "Aragón GP",
  // "Spanish Grand Prix"...) devuelve el nombre del país/prefijo del GP.
  static std::string extraerPaisDeSesion(const std::string& strEvent);

  // Dado el JSON de eventsround.php de una ronda, compone un EventoMotor
  // virtual "GP" con nombre=pais + " GP" y fechaHora del máximo dateEvent
  // (domingo = carrera). Devuelve false si el JSON no tiene eventos.
  static bool parsearRondaComoGp(const std::string& json, EventoMotor& out);

 private:
  IHttpClient& http_;
};
