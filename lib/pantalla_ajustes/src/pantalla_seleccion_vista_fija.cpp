#include "pantalla_seleccion_vista_fija.h"
#include "tft_driver.h"
#include <TFT_eSPI.h>
#include <cstdio>

namespace {
const char* labelVista(uint8_t id) {
  switch (id) {
    case 0: return "Radar"; case 1: return "Reloj"; case 2: return "Meteo";
    case 3: return "Futbol"; case 4: return "MotoGP"; case 5: return "F1";
    default: return "?";
  }
}
constexpr int OFFSET_Y = 20;
constexpr int ALTO_FILA = 26;
}

void PantallaSeleccionVistaFija::alTocar(int x, int y) {
  int fila = y / ALTO_FILA;
  if (fila >= 0 && fila < 6) {
    cfg_.vista_fija = static_cast<uint8_t>(fila);
    ConfigStore::guardar(cfg_);
    dirty_ = true;
  } else if (fila == 6 && x >= 10 && x <= 90) {
    gestor_.volverAtras();
  }
}

void PantallaSeleccionVistaFija::dibujar(uint32_t) {
  if (!dirty_) return;
  auto& tft = tft_driver::obtenerTft();
  tft.fillRect(0, OFFSET_Y, 320, 220, 0x0000);
  tft.setTextFont(2);
  tft.setTextColor(0x07E0, 0x0000);
  char buf[32];
  int y = OFFSET_Y + 2;
  for (uint8_t id = 0; id < 6; ++id) {
    tft.drawCircle(22, y + 12, 8, 0x07E0);
    if (cfg_.vista_fija == id) tft.fillCircle(22, y + 12, 5, 0x07E0);
    std::snprintf(buf, sizeof(buf), "  %s", labelVista(id));
    tft.setCursor(40, y + 4);
    tft.print(buf);
    y += ALTO_FILA;
  }
  tft.setCursor(10, y + 4);
  tft.print("< Volver");
  dirty_ = false;
}
