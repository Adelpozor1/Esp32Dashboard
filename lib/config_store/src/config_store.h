#pragma once
#include <string>
#include <vector>
#include <cstdint>

enum class ModoVista : uint8_t { FIJO = 0, CARRUSEL = 1 };

struct Config {
  // v1 (persistido desde el inicio del proyecto):
  std::string ssid;
  std::string password;
  std::string direccion;
  double lat = 0.0;
  double lon = 0.0;
  int    radio_km = 25;

  // v2:
  ModoVista modo = ModoVista::CARRUSEL;
  uint16_t  intervalo_carrusel_s = 10;
  uint8_t   vista_fija = 0;
  // Ids: 0=Radar, 2=Meteo, 3=Futbol, 4=MotoGP, 5=F1. (Id 1=Reloj eliminado 2026-09-07.)
  std::vector<uint8_t> vistas_orden = {0, 2, 3, 4, 5};

  int16_t touch_min_x = 0;
  int16_t touch_max_x = 0;
  int16_t touch_min_y = 0;
  int16_t touch_max_y = 0;
  bool    touch_calibrado = false;
};

// Persistencia en NVS mediante Preferences. La serialización a bytes se expone
// como funciones estáticas para poder testearla en native (sin depender de NVS).
//
// Layout binario (little-endian):
//   [0..1]   magic 0xC0DE
//   [2]      version (1 o 2)
//   Después, los campos v1 tal como estaban:
//     lat (double), lon (double), radio_km (u16),
//     ssid (u16 len + bytes), password (u16 len + bytes), direccion (u16 len + bytes)
//   Si version == 2, a continuación:
//     modo (u8), intervalo (u16), vista_fija (u8),
//     n_vistas (u8), vistas_orden[n_vistas] (u8 c/u — cada vid en 0..MAX_ID_VISTA),
//     touch_min_x (i16), touch_max_x (i16), touch_min_y (i16), touch_max_y (i16),
//     touch_calibrado (u8)
class ConfigStore {
 public:
  static constexpr uint16_t MAGIC = 0xC0DE;
  static constexpr uint8_t  VERSION = 2;
  static constexpr size_t   MAX_STR = 128;
  // Cap defensivo del vector `vistas_orden` en NVS. Hoy sobra (sólo hay 6 vistas),
  // pero dejamos margen si se añaden pantallas nuevas o para tolerar upgrades.
  static constexpr uint8_t  MAX_VISTAS = 32;

  static void serializar(const Config& in, std::vector<uint8_t>& out);
  static bool deserializar(const uint8_t* data, size_t size, Config& out);

#ifndef UNIT_TEST
  static bool cargar(Config& out);
  static bool guardar(const Config& cfg);
  static void borrar();
#endif
};
