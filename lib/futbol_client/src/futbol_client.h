#pragma once
#include "http_client.h"
#include <cstdint>
#include <string>
#include <vector>

struct Partido {
  std::string local;
  std::string visitante;
  std::string fechaHora;       // formato humano "Vie 5 sep 14:15" o vacío
  int         golesLocal = -1;      // -1 si aún no jugado
  int         golesVisitante = -1;
};

// PartidoLive queda como legado por los tests antiguos; ya no se usa en runtime.
struct PartidoLive {
  std::string local;
  std::string visitante;
  int         golesLocal = 0;
  int         golesVisitante = 0;
  std::string progreso;
};

struct FutbolSnapshot {
  bool     ok = false;
  uint32_t obtenido_ms = 0;
  bool     stale = false;

  int      jornadaActual = 0;
  std::vector<Partido> partidosJornada;      // marcador rellenado si jugado

  int      jornadaSiguiente = 0;
  std::vector<Partido> partidosSiguiente;    // fechaHora rellenada, marcador -1
};

class FutbolClient {
 public:
  explicit FutbolClient(IHttpClient& http) : http_(http) {}

  bool fetch(FutbolSnapshot& out);

  // Rellena `out` con los partidos del JSON. Nombre humano en fechaHora si hay
  // strTime, o "Vie 5 sep" si sólo fecha.
  static bool parsearJornada(const std::string& json, std::vector<Partido>& out);

  // Devuelve intRound y strSeason del events[0] del JSON de past.
  static bool parsearUltimaRondaYSeason(const std::string& json,
                                        int& outRonda,
                                        std::string& outSeason);

  // Legacy: mantenidos para no romper tests antiguos.
  static bool parsearLive(const std::string& json, PartidoLive& out);
  static bool parsearUltimo(const std::string& json, Partido& out);
  static void filtrarHoyManana(const std::string& json,
                               std::vector<Partido>& out,
                               const std::string& hoyISO,
                               const std::string& mananaISO);

 private:
  IHttpClient& http_;
};
