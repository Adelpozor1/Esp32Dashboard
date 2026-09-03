#include "tft_driver.h"
#include <Arduino.h>
#include <TFT_eSPI.h>

namespace tft_driver {

namespace {
TFT_eSPI s_tft;
bool     s_iniciado = false;

constexpr uint16_t COL_FONDO         = 0x0000;
constexpr uint16_t COL_TEXTO_SPLASH  = 0x07E0;
}  // namespace

void iniciar() {
  if (s_iniciado) return;
  s_tft.init();
  s_tft.setRotation(1);
  s_tft.fillScreen(COL_FONDO);
  // En el CYD ESP32-2432S028R, el backlight del TFT es active-high en GPIO 21.
  constexpr int PIN_BACKLIGHT = 21;
  pinMode(PIN_BACKLIGHT, OUTPUT);
  digitalWrite(PIN_BACKLIGHT, HIGH);
  s_iniciado = true;
}

TFT_eSPI& obtenerTft() { return s_tft; }

void pintarSplash(const std::string& titulo, const std::string& detalle) {
  if (!s_iniciado) return;
  s_tft.fillScreen(COL_FONDO);
  s_tft.setTextColor(COL_TEXTO_SPLASH, COL_FONDO);
  s_tft.setTextFont(4);
  s_tft.setCursor(10, 60);
  s_tft.print(titulo.c_str());
  s_tft.setTextFont(2);
  s_tft.setCursor(10, 110);
  s_tft.print(detalle.c_str());
}

}  // namespace tft_driver
