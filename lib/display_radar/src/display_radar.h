#pragma once
#include "radar_state.h"
#include <string>

class DisplayRadar {
 public:
  // Inicializa TFT_eSPI (rotación landscape 320x240) y enciende el backlight.
  static void iniciar();

  // Pinta un mensaje de texto centrado a pantalla completa (para estados como
  // "Modo Portal — conecta al AP RadarVuelos-XXXX" o "Conectando WiFi...").
  static void pintarMensaje(const std::string& titulo, const std::string& detalle);

  // Redibuja el radar polar completo con el snapshot dado. Usa sprite offscreen
  // para evitar parpadeo. Lado izquierdo: círculos + aviones. Lado derecho:
  // callsigns con distancia.
  static void pintarRadar(const Snapshot& snap);
};
