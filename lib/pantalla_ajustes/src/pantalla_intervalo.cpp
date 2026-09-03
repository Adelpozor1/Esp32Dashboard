#include "pantalla_intervalo.h"
#include "tft_driver.h"
#include <TFT_eSPI.h>
#include <cstdio>

void PantallaIntervalo::alTocar(int x, int y) {
  // Botón "-": [30, 90] × [80, 160].  Botón "+": [200, 260] × [80, 160].
  // Botón "< Volver": [10, 90] × [180, 210].
  if (y >= 80 && y <= 160) {
    if (x >= 30 && x <= 90 && cfg_.intervalo_carrusel_s > 5) {
      cfg_.intervalo_carrusel_s -= 5;
      ConfigStore::guardar(cfg_);
      dirty_ = true;
    } else if (x >= 200 && x <= 260 && cfg_.intervalo_carrusel_s < 120) {
      cfg_.intervalo_carrusel_s += 5;
      ConfigStore::guardar(cfg_);
      dirty_ = true;
    }
  } else if (y >= 180 && y <= 210 && x >= 10 && x <= 90) {
    gestor_.volverAtras();
  }
}

void PantallaIntervalo::dibujar(uint32_t) {
  if (!dirty_) return;
  auto& tft = tft_driver::obtenerTft();
  tft.fillRect(0, 20, 320, 220, 0x0000);
  tft.setTextColor(0x07E0, 0x0000);
  tft.setTextFont(2);
  tft.setCursor(10, 30);
  tft.print("Intervalo carrusel");

  // Botón "-"
  tft.fillRect(30, 80, 60, 80, 0x03E0);
  tft.setTextColor(0xFFFF, 0x03E0);
  tft.setTextFont(4);
  tft.setCursor(50, 100);
  tft.print("-");
  // Botón "+"
  tft.fillRect(200, 80, 60, 80, 0x03E0);
  tft.setCursor(215, 100);
  tft.print("+");
  // Valor centrado
  tft.setTextColor(0x07E0, 0x0000);
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%u s", (unsigned)cfg_.intervalo_carrusel_s);
  tft.setCursor(120, 100);
  tft.print(buf);
  // Volver
  tft.setTextFont(2);
  tft.setCursor(10, 190);
  tft.setTextColor(0x07E0, 0x0000);
  tft.print("< Volver");

  dirty_ = false;
}
