#include "pantalla_proximamente.h"
#include "tft_driver.h"
#include <TFT_eSPI.h>

void PantallaProximamente::dibujar(uint32_t) {
  if (!dirty_) return;
  auto& tft = tft_driver::obtenerTft();
  tft.fillRect(0, 20, 320, 220, 0x0000);
  tft.setTextColor(0x07E0, 0x0000);
  tft.setTextFont(4);
  tft.setCursor(20, 90);
  tft.print(nombre_.c_str());
  tft.setTextFont(2);
  tft.setCursor(20, 140);
  tft.print("Proximamente");
  dirty_ = false;
}
