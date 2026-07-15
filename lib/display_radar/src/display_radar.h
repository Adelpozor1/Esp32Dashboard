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

  // Redibuja el radar tipo sonar con el snapshot dado. Usa sprite offscreen
  // para evitar parpadeo. Lado izquierdo: círculos + línea de barrido rotando
  // + aviones (verde fosforito, rojo si dist<3km). Lado derecho: avión más
  // cercano con callsign, distancia y altitud grandes.
  //
  // anguloBarridoDeg: ángulo actual del sweep 0..359. El caller debe
  // incrementarlo entre llamadas para animarlo.
  static void pintarRadar(const Snapshot& snap, int anguloBarridoDeg);
};
