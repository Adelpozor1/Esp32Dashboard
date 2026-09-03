#pragma once
#include <string>

class TFT_eSPI;

namespace tft_driver {

// Inicializa el TFT (rotación landscape 320x240), backlight en GPIO 21 HIGH,
// rellena la pantalla de negro. Idempotente.
void iniciar();

// Devuelve la instancia global de TFT_eSPI. Necesario porque TFT_eSPI ocupa
// bastante RAM y no queremos duplicarla por pantalla.
TFT_eSPI& getTft();

// Pinta un splash centrado de dos líneas (título grande + detalle). Usado
// antes de que exista GestorPantallas (splash de arranque, "conectando WiFi").
void pintarSplash(const std::string& titulo, const std::string& detalle);

}  // namespace tft_driver
