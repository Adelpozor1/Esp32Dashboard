#pragma once
#include "config_store.h"
#include "radar_state.h"
#include "http_client.h"

class RadarWebServer {
 public:
  // Arranca las rutas HTTP. `http` se usa para geocoding al reconfigurar.
  static void iniciar(const Config& cfg, RadarState& estado, IHttpClient& http);
};
