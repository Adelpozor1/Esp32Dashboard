#include "pantalla_calibrar_touch.h"
#include "tft_driver.h"
#include "touch.h"
#include <TFT_eSPI.h>
#include <Arduino.h>
#include <algorithm>

namespace {
struct Cruz { int x, y; };
const Cruz cruces[4] = {{20, 20}, {300, 20}, {20, 220}, {300, 220}};
}

void PantallaCalibrarTouch::alEntrar() {
  paso_ = 0;
  dirty_ = true;
}

void PantallaCalibrarTouch::dibujar(uint32_t) {
  if (!dirty_) return;
  auto& tft = tft_driver::obtenerTft();
  if (paso_ >= 4) {
    // Guardar y salir.
    int16_t minX = std::min({rawX_[0], rawX_[1], rawX_[2], rawX_[3]});
    int16_t maxX = std::max({rawX_[0], rawX_[1], rawX_[2], rawX_[3]});
    int16_t minY = std::min({rawY_[0], rawY_[1], rawY_[2], rawY_[3]});
    int16_t maxY = std::max({rawY_[0], rawY_[1], rawY_[2], rawY_[3]});
    cfg_.touch_min_x = minX; cfg_.touch_max_x = maxX;
    cfg_.touch_min_y = minY; cfg_.touch_max_y = maxY;
    cfg_.touch_calibrado = true;
    ConfigStore::guardar(cfg_);
    touch::CalibracionTouch nueva{minX, maxX, minY, maxY, true};
    touch::actualizarCalibracion(nueva);
    gestor_.volverAtras();
    dirty_ = false;
    return;
  }
  tft.fillScreen(0x0000);
  tft.setTextFont(2);
  tft.setTextColor(0x07E0, 0x0000);
  tft.setCursor(80, 110);
  tft.print("Toca la cruz");
  const Cruz& c = cruces[paso_];
  tft.drawLine(c.x - 8, c.y, c.x + 8, c.y, 0x07E0);
  tft.drawLine(c.x, c.y - 8, c.x, c.y + 8, 0x07E0);
  dirty_ = false;

  // Lectura bloqueante: sí, esta pantalla se queda hasta que se toque cada cruz.
  int16_t rx = 0, ry = 0;
  if (touch::leerCrudoBloqueante(rx, ry, /*timeoutMs=*/30000)) {
    rawX_[paso_] = rx; rawY_[paso_] = ry;
    ++paso_;
    dirty_ = true;
  } else {
    // Timeout: aborta la calibración sin guardar.
    gestor_.volverAtras();
  }
}
