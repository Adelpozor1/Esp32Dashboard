#include "pantalla_menu.h"
#include "tft_driver.h"
#include <TFT_eSPI.h>

namespace {
constexpr int OFFSET_Y = 20;
constexpr int ALTO_FILA = 28;
constexpr uint16_t COL_FONDO = 0x0000;
constexpr uint16_t COL_TXT   = 0x07E0;
constexpr uint16_t COL_LINEA = 0x03E0;
}

PantallaMenu::PantallaMenu(pantallas::GestorPantallas& g, pantallas::Pantalla* a)
  : gestor_(g), ajustes_(a) {}

void PantallaMenu::configurarEntradas(const std::vector<Entrada>& e) {
  entradas_ = e;
  dirty_ = true;
}

void PantallaMenu::alTocar(int x, int y) {
  const int fila = y / ALTO_FILA;
  const int nTotal = static_cast<int>(entradas_.size()) + 1;  // + Ajustes
  if (fila < 0 || fila >= nTotal) return;
  if (fila < static_cast<int>(entradas_.size())) {
    gestor_.mostrarPorId(entradas_[fila].id);
  } else if (ajustes_) {
    gestor_.abrirEnPila(ajustes_);
  }
}

void PantallaMenu::dibujar(uint32_t) {
  if (!dirty_) return;
  auto& tft = tft_driver::obtenerTft();
  tft.fillRect(0, OFFSET_Y, 320, 220, COL_FONDO);
  tft.setTextFont(2);
  tft.setTextColor(COL_TXT, COL_FONDO);
  int y = OFFSET_Y + 4;
  for (const auto& e : entradas_) {
    tft.setCursor(12, y);
    tft.print(e.etiqueta.c_str());
    tft.drawFastHLine(0, y + ALTO_FILA - 4, 320, COL_LINEA);
    y += ALTO_FILA;
  }
  tft.setCursor(12, y);
  tft.print("Ajustes");
  dirty_ = false;
}
