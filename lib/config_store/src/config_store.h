#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct Config {
  std::string ssid;
  std::string password;
  std::string direccion;
  double lat = 0.0;
  double lon = 0.0;
  int    radio_km = 25;
};

// Persistencia en NVS mediante Preferences. La serialización a bytes se expone
// como funciones estáticas para poder testearla en native (sin depender de NVS).
//
// Formato binario (little-endian):
//   [0..1]   magic 0xC0DE
//   [2]      versión (1)
//   [3..10]  lat (double, 8 bytes)
//   [11..18] lon (double, 8 bytes)
//   [19..20] radio_km (uint16)
//   [21..22] len_ssid (uint16) | ssid bytes
//   [..]     len_pass (uint16) | pass bytes
//   [..]     len_dir  (uint16) | dir bytes
class ConfigStore {
 public:
  static constexpr uint16_t MAGIC = 0xC0DE;
  static constexpr uint8_t  VERSION = 1;
  static constexpr size_t   MAX_STR = 128;

  // Serialización pura (testeable en native).
  static void serializar(const Config& in, std::vector<uint8_t>& out);
  static bool deserializar(const uint8_t* data, size_t size, Config& out);

#ifndef UNIT_TEST
  // API con NVS (solo compila en la placa).
  static bool cargar(Config& out);
  static bool guardar(const Config& cfg);
  static void borrar();
#endif
};
