#pragma once
#include "http_client.h"
#include <cstdint>
#include <string>
#include <vector>

struct Partido {
  std::string local;
  std::string visitante;
  std::string fechaHora;   // "YYYY-MM-DD HH:MM" o formato humano ya compuesto
  int         golesLocal = -1;      // -1 si aún no jugado
  int         golesVisitante = -1;
};

struct PartidoLive {
  std::string local;
  std::string visitante;
  int         golesLocal = 0;
  int         golesVisitante = 0;
  std::string progreso;   // "32", "HT", etc.
};

struct FutbolSnapshot {
  bool     ok = false;
  uint32_t obtenido_ms = 0;
  bool     stale = false;

  bool         hayLive = false;
  PartidoLive  live;

  bool         hayUltimo = false;
  Partido      ultimoJugado;

  std::vector<Partido> hoyManana;  // partidos de LaLiga hoy o hoy+1
};

class FutbolClient {
 public:
  explicit FutbolClient(IHttpClient& http) : http_(http) {}

  bool fetch(FutbolSnapshot& out);

  // Helpers estáticos expuestos para tests.
  // Devuelve true y rellena `out` con el primer evento cuyo idLeague sea 4335 (LaLiga).
  static bool parsearLive(const std::string& json, PartidoLive& out);
  // Devuelve true y rellena `out` con events[0] como Partido (último jugado).
  static bool parsearUltimo(const std::string& json, Partido& out);
  // Rellena `out` con los partidos cuyo dateEvent coincide con hoyISO o mananaISO.
  static void filtrarHoyManana(const std::string& json,
                               std::vector<Partido>& out,
                               const std::string& hoyISO,
                               const std::string& mananaISO);

 private:
  IHttpClient& http_;
};
