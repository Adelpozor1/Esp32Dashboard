#include "radar_state.h"

RadarState::RadarState(double lat, double lon, int radioKm) {
  s_.lat = lat;
  s_.lon = lon;
  s_.radio_km = radioKm;
#ifdef ARDUINO
  mutex_ = xSemaphoreCreateMutex();
#endif
}

RadarState::~RadarState() {
#ifdef ARDUINO
  if (mutex_) vSemaphoreDelete(mutex_);
#endif
}

void RadarState::actualizar(const std::vector<Aeronave>& aviones, uint32_t timestampMs) {
#ifdef ARDUINO
  xSemaphoreTake(mutex_, portMAX_DELAY);
#endif
  s_.aeronaves = aviones;
  s_.ts = timestampMs;
  s_.stale = false;
#ifdef ARDUINO
  xSemaphoreGive(mutex_);
#endif
}

void RadarState::marcarStale() {
#ifdef ARDUINO
  xSemaphoreTake(mutex_, portMAX_DELAY);
#endif
  s_.stale = true;
#ifdef ARDUINO
  xSemaphoreGive(mutex_);
#endif
}

Snapshot RadarState::snapshot() {
#ifdef ARDUINO
  xSemaphoreTake(mutex_, portMAX_DELAY);
  Snapshot copia = s_;
  xSemaphoreGive(mutex_);
  return copia;
#else
  return s_;
#endif
}
