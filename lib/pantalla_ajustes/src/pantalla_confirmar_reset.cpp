#include "pantalla_confirmar_reset.h"
#include "tft_driver.h"
#include "config_store.h"
#include <Arduino.h>
#include <TFT_eSPI.h>

void PantallaConfirmarReset::alTocar(int x, int y) {
  // Cancelar: [10, 150] × [160, 210].  Borrar: [170, 310] × [160, 210].
  if (y >= 160 && y <= 210) {
    if (x >= 10 && x <= 150) {
      gestor_.volverAtras();
    } else if (x >= 170 && x <= 310) {
      ConfigStore::borrar();
      delay(300);
      ESP.restart();
    }
  }
}

void PantallaConfirmarReset::dibujar(uint32_t) {
  if (!dirty_) return;
  auto& tft = tft_driver::obtenerTft();
  tft.fillRect(0, 20, 320, 220, 0x0000);
  tft.setTextFont(2);
  tft.setTextColor(0xF800, 0x0000);
  tft.setCursor(10, 30);
  tft.print("Reset total");
  tft.setTextColor(0x07E0, 0x0000);
  tft.setCursor(10, 60);
  tft.print("Se borrara TODA la config:");
  tft.setCursor(10, 80);
  tft.print("WiFi, direccion, ajustes.");
  tft.setCursor(10, 110);
  tft.print("Confirmas?");
  // Botón Cancelar
  tft.fillRect(10, 160, 140, 50, 0x03E0);
  tft.setTextColor(0xFFFF, 0x03E0);
  tft.setCursor(40, 180);
  tft.print("Cancelar");
  // Botón Borrar
  tft.fillRect(170, 160, 140, 50, 0xF800);
  tft.setCursor(210, 180);
  tft.print("Borrar");
  dirty_ = false;
}
