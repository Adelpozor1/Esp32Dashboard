#pragma once
#include "config_store.h"
#include "http_client.h"

class WifiPortal {
 public:
  // Bloquea indefinidamente sirviendo el portal AP.
  // Cuando el usuario envía el formulario y el geocoding tiene éxito, guarda
  // la config en NVS y reinicia la ESP32 (no retorna).
  //
  // `cfgPrevia` es opcional: si se pasa (porque había config guardada pero la
  // WiFi no conecta), la vista del QR muestra qué ajustes ya estaban puestos
  // para que el usuario sepa qué reintroducir.
  static void ejecutar(IHttpClient& http, const Config* cfgPrevia = nullptr);
};
