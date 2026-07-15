#pragma once
#include "config_store.h"
#include "http_client.h"

class WifiPortal {
 public:
  // Bloquea indefinidamente sirviendo el portal AP.
  // Cuando el usuario envía el formulario y el geocoding tiene éxito, guarda
  // la config en NVS y reinicia la ESP32 (no retorna).
  static void ejecutar(IHttpClient& http);
};
