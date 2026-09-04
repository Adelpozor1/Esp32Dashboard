#include "pantalla_f1.h"
#include "tft_driver.h"
#include "paleta_dark.h"
#include <TFT_eSPI.h>
#include <algorithm>
#include <cstdio>

namespace {
constexpr int OFFSET_Y = 20;
constexpr int W = 320;
constexpr int H_CONTENIDO = 220;

void pintarFondo(TFT_eSPI& tft) {
  tft.fillRect(0, OFFSET_Y, W, H_CONTENIDO, paleta_dark::COL_FONDO);
}
}  // namespace

std::string PantallaF1::truncar(const std::string& s, size_t n) {
  if (s.size() <= n) return s;
  return s.substr(0, n - 1) + ".";
}

void PantallaF1::alEntrar() { dirty_ = true; ultObtenidoMs_ = 0; }

void PantallaF1::alDeslizar(pantallas::Direccion dir) {
  if (dir == pantallas::Direccion::ARRIBA || dir == pantallas::Direccion::ABAJO) {
    sub_ = (sub_ == SubVista::ULTIMA) ? SubVista::CALENDARIO : SubVista::ULTIMA;
    dirty_ = true;
  }
}

void PantallaF1::dibujar(uint32_t) {
  auto& tft = tft_driver::obtenerTft();
  const bool datosNuevos = snap_.obtenido_ms != ultObtenidoMs_;
  if (!dirty_ && !datosNuevos) return;

  if (!snap_.ok) {
    dibujarSinDatos(tft);
    dirty_ = false; ultObtenidoMs_ = snap_.obtenido_ms;
    return;
  }
  if (sub_ == SubVista::ULTIMA) dibujarUltima(tft);
  else                           dibujarCalendario(tft);
  dirty_ = false; ultObtenidoMs_ = snap_.obtenido_ms;
}

void PantallaF1::dibujarSinDatos(TFT_eSPI& tft) {
  pintarFondo(tft);
  tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
  tft.setTextFont(4);
  const char* t = "F1: sin datos";
  int16_t w = tft.textWidth(t);
  tft.setCursor((W - w) / 2, OFFSET_Y + 90);
  tft.print(t);
}

void PantallaF1::dibujarIndicador(TFT_eSPI& tft) {
  const int y = OFFSET_Y + 6;
  const int r = 3;
  const int xA = W - 22, xB = W - 10;
  const bool ultima = (sub_ == SubVista::ULTIMA);
  if (ultima) {
    tft.fillCircle(xA, y, r, paleta_dark::COL_ACENTO);
    tft.drawCircle(xB, y, r, paleta_dark::COL_TXT_SECUND);
  } else {
    tft.drawCircle(xA, y, r, paleta_dark::COL_TXT_SECUND);
    tft.fillCircle(xB, y, r, paleta_dark::COL_ACENTO);
  }
}

void PantallaF1::dibujarUltima(TFT_eSPI& tft) {
  pintarFondo(tft);
  dibujarIndicador(tft);
  tft.setTextFont(2);
  tft.setTextColor(paleta_dark::COL_ACENTO, paleta_dark::COL_FONDO);
  tft.setCursor(10, OFFSET_Y + 6);
  tft.print("Ultima carrera");
  // Nombre GP + circuito
  tft.setTextFont(2);
  tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
  tft.setCursor(10, OFFSET_Y + 34);
  tft.print(truncar(snap_.ultima.nombreGp, 26).c_str());
  tft.setTextFont(1);
  tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
  tft.setCursor(10, OFFSET_Y + 54);
  tft.print(truncar(snap_.ultima.circuito, 40).c_str());
  // Podio
  int y = OFFSET_Y + 82;
  if (snap_.podio.empty()) {
    tft.setTextFont(2);
    tft.setCursor(10, y);
    tft.print("(sin resultados)");
    return;
  }
  const int n = std::min<int>(3, static_cast<int>(snap_.podio.size()));
  for (int i = 0; i < n; ++i) {
    const auto& p = snap_.podio[i];
    char pos[4];
    std::snprintf(pos, sizeof(pos), "%d.", p.posicion);
    tft.setTextFont(2);
    tft.setTextColor(paleta_dark::COL_ACENTO, paleta_dark::COL_FONDO);
    tft.setCursor(10, y);
    tft.print(pos);
    tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
    tft.setCursor(38, y);
    tft.print(truncar(p.nombre, 14).c_str());
    tft.setTextFont(1);
    tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
    tft.setCursor(38, y + 18);
    tft.print(truncar(p.equipo, 30).c_str());
    if (!p.tiempo.empty()) {
      tft.setTextFont(1);
      tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
      int16_t wT = tft.textWidth(p.tiempo.c_str());
      tft.setCursor(W - 10 - wT, y + 8);
      tft.print(p.tiempo.c_str());
    }
    y += 40;
  }
}

void PantallaF1::dibujarCalendario(TFT_eSPI& tft) {
  pintarFondo(tft);
  dibujarIndicador(tft);
  tft.setTextFont(2);
  tft.setTextColor(paleta_dark::COL_ACENTO, paleta_dark::COL_FONDO);
  tft.setCursor(10, OFFSET_Y + 6);
  tft.print("Calendario");
  int y = OFFSET_Y + 34;
  const int n = std::min<int>(5, static_cast<int>(snap_.proximas.size()));
  if (n == 0) {
    tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
    tft.setCursor(10, y);
    tft.print("(sin datos)");
    return;
  }
  for (int i = 0; i < n; ++i) {
    const auto& c = snap_.proximas[i];
    tft.setTextFont(2);
    tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
    tft.setCursor(10, y);
    tft.print(truncar(c.nombreGp, 26).c_str());
    tft.setTextFont(1);
    tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
    int16_t wFH = tft.textWidth(c.fechaHora.c_str());
    tft.setCursor(W - 10 - wFH, y + 4);
    tft.print(c.fechaHora.c_str());
    y += 32;
  }
}
