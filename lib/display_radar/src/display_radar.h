#pragma once
#include "radar_state.h"
#include <string>

class DisplayRadar {
 public:
  // Inicializa TFT_eSPI (rotación landscape 320x240) y enciende el backlight.
  static void iniciar();

  // Pinta un mensaje de texto centrado a pantalla completa (para estados como
  // "Conectando WiFi..." o el splash de arranque).
  static void pintarMensaje(const std::string& titulo, const std::string& detalle);

  // Pinta la pantalla del Modo Portal con un QR grande a la izquierda que
  // codifica la URL del portal, y a la derecha instrucciones con el SSID del AP.
  static void pintarPortalQR(const std::string& ssidAp, const std::string& url);

  // Redibuja el radar polar completo con el snapshot dado. Usa sprite offscreen
  // para evitar parpadeo. Lado izquierdo: círculos + aviones. Lado derecho:
  // callsigns con distancia.
  static void pintarRadar(const Snapshot& snap);
};
