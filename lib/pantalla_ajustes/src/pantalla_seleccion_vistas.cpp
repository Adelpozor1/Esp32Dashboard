#include "pantalla_seleccion_vistas.h"
#include "tft_driver.h"
#include <TFT_eSPI.h>
#include <algorithm>
#include <cstdio>

namespace {
const char* labelVista(uint8_t id) {
  switch (id) {
    case 0: return "Radar";
    case 1: return "Reloj";
    case 2: return "Meteo";
    case 3: return "Futbol";
    case 4: return "MotoGP";
    case 5: return "F1";
    default: return "?";
  }
}
constexpr int OFFSET_Y  = 20;
constexpr int ALTO_FILA = 26;
}

void PantallaSeleccionVistas::alEntrar() {
  ordenLocal_ = cfg_.vistas_orden;
  for (int i = 0; i < 6; ++i) activas_[i] = false;
  for (auto id : ordenLocal_) if (id < 6) activas_[id] = true;
  dirty_ = true;
}

int PantallaSeleccionVistas::nActivas() const { return static_cast<int>(ordenLocal_.size()); }

void PantallaSeleccionVistas::guardar() {
  cfg_.vistas_orden = ordenLocal_;
  ConfigStore::guardar(cfg_);
}

void PantallaSeleccionVistas::alTocar(int x, int y) {
  // Cada fila del listado muestra las 6 vistas (fila 0..5).
  // Fila 6 = "< Volver".
  // Dentro de una fila: check [10,60], nombre [70,220], flecha ↑ [230,265], flecha ↓ [270,305].
  int fila = y / ALTO_FILA;
  if (fila >= 0 && fila < 6) {
    uint8_t id = static_cast<uint8_t>(fila);
    if (x >= 10 && x <= 60) {
      if (activas_[id]) {
        if (nActivas() > 1) {
          activas_[id] = false;
          ordenLocal_.erase(std::remove(ordenLocal_.begin(), ordenLocal_.end(), id),
                            ordenLocal_.end());
        }
      } else {
        activas_[id] = true;
        ordenLocal_.push_back(id);
      }
      guardar();
      dirty_ = true;
    } else if (x >= 230 && x <= 265 && activas_[id]) {
      auto it = std::find(ordenLocal_.begin(), ordenLocal_.end(), id);
      if (it != ordenLocal_.begin() && it != ordenLocal_.end()) {
        std::iter_swap(it, it - 1);
        guardar();
        dirty_ = true;
      }
    } else if (x >= 270 && x <= 305 && activas_[id]) {
      auto it = std::find(ordenLocal_.begin(), ordenLocal_.end(), id);
      if (it != ordenLocal_.end() && (it + 1) != ordenLocal_.end()) {
        std::iter_swap(it, it + 1);
        guardar();
        dirty_ = true;
      }
    }
  } else if (fila == 6 && x >= 10 && x <= 90) {
    gestor_.volverAtras();
  }
}

void PantallaSeleccionVistas::dibujar(uint32_t) {
  if (!dirty_) return;
  auto& tft = tft_driver::obtenerTft();
  tft.fillRect(0, OFFSET_Y, 320, 220, 0x0000);
  tft.setTextFont(2);
  tft.setTextColor(0x07E0, 0x0000);
  char buf[32];
  int y = OFFSET_Y + 2;
  for (uint8_t id = 0; id < 6; ++id) {
    // Check
    tft.drawRect(12, y + 2, 20, 20, 0x07E0);
    if (activas_[id]) tft.fillRect(15, y + 5, 14, 14, 0x07E0);
    // Nombre
    std::snprintf(buf, sizeof(buf), " %s", labelVista(id));
    tft.setCursor(40, y + 4);
    tft.print(buf);
    // Flechas
    if (activas_[id]) {
      tft.setCursor(238, y + 4); tft.print("^");
      tft.setCursor(278, y + 4); tft.print("v");
    }
    y += ALTO_FILA;
  }
  tft.setCursor(10, y + 4);
  tft.print("< Volver");
  dirty_ = false;
}
