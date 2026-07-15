#pragma once
#include "adsb_client.h"
#include <vector>
#include <cstdint>

#ifdef ARDUINO
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

struct Snapshot {
  double lat = 0.0;
  double lon = 0.0;
  int    radio_km = 25;
  uint32_t ts = 0;         // millis() del último update exitoso
  bool   stale = true;
  std::vector<Aeronave> aeronaves;
};

class RadarState {
 public:
  RadarState(double lat, double lon, int radioKm);
  ~RadarState();

  // Sustituye la lista y marca no-stale con ts actual.
  void actualizar(const std::vector<Aeronave>& aviones, uint32_t timestampMs);

  // Mantiene la lista pero marca stale=true.
  void marcarStale();

  // Devuelve una copia atómica del snapshot.
  Snapshot snapshot();

 private:
  Snapshot s_;
#ifdef ARDUINO
  SemaphoreHandle_t mutex_;
#endif
};
