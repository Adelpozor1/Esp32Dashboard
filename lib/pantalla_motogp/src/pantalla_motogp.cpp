#include "pantalla_motogp.h"
#include "tft_driver.h"
#include "paleta_dark.h"
#include <TFT_eSPI.h>
#include <algorithm>

namespace {
constexpr int OFFSET_Y = 20;
constexpr int W = 320;
constexpr int H_CONTENIDO = 220;

void pintarFondo(TFT_eSPI& tft) {
  tft.fillRect(0, OFFSET_Y, W, H_CONTENIDO, paleta_dark::COL_FONDO);
}
}  // namespace

std::string PantallaMotogp::truncar(const std::string& s, size_t n) {
  if (s.size() <= n) return s;
  return s.substr(0, n - 1) + ".";
}

void PantallaMotogp::alEntrar() { dirty_ = true; ultObtenidoMs_ = 0; }

void PantallaMotogp::alDeslizar(pantallas::Direccion dir) {
  if (dir == pantallas::Direccion::ARRIBA || dir == pantallas::Direccion::ABAJO) {
    sub_ = (sub_ == SubVista::ULTIMOS) ? SubVista::CALENDARIO : SubVista::ULTIMOS;
    dirty_ = true;
  }
}

void PantallaMotogp::dibujar(uint32_t) {
  auto& tft = tft_driver::obtenerTft();
  const bool datosNuevos = snap_.obtenido_ms != ultObtenidoMs_;
  if (!dirty_ && !datosNuevos) return;

  if (!snap_.ok) {
    dibujarSinDatos(tft);
    dirty_ = false; ultObtenidoMs_ = snap_.obtenido_ms; return;
  }
  if (sub_ == SubVista::ULTIMOS) dibujarUltimos(tft);
  else                            dibujarCalendario(tft);
  dirty_ = false; ultObtenidoMs_ = snap_.obtenido_ms;
}

void PantallaMotogp::dibujarSinDatos(TFT_eSPI& tft) {
  pintarFondo(tft);
  tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
  tft.setTextFont(4);
  const char* t = "MotoGP: sin datos";
  int16_t w = tft.textWidth(t);
  tft.setCursor((W - w) / 2, OFFSET_Y + 90);
  tft.print(t);
}

void PantallaMotogp::dibujarIndicador(TFT_eSPI& tft) {
  const int y = OFFSET_Y + 6;
  const int r = 3;
  const int xA = W - 22, xB = W - 10;
  const bool ultimos = (sub_ == SubVista::ULTIMOS);
  if (ultimos) {
    tft.fillCircle(xA, y, r, paleta_dark::COL_ACENTO);
    tft.drawCircle(xB, y, r, paleta_dark::COL_TXT_SECUND);
  } else {
    tft.drawCircle(xA, y, r, paleta_dark::COL_TXT_SECUND);
    tft.fillCircle(xB, y, r, paleta_dark::COL_ACENTO);
  }
}

void PantallaMotogp::dibujarUltimos(TFT_eSPI& tft) {
  pintarFondo(tft);
  dibujarIndicador(tft);
  tft.setTextFont(2);
  tft.setTextColor(paleta_dark::COL_ACENTO, paleta_dark::COL_FONDO);
  tft.setCursor(10, OFFSET_Y + 6);
  tft.print("Ultimas carreras");
  const int n = std::min<int>(4, static_cast<int>(snap_.ultimos.size()));
  if (n == 0) {
    tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
    tft.setCursor(10, OFFSET_Y + 100);
    tft.print("(sin datos)");
    return;
  }
  if (n == 1) {
    const auto& e = snap_.ultimos[0];
    tft.setTextFont(4);
    tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
    std::string nombre = truncar(e.nombre, 20);
    int16_t w = tft.textWidth(nombre.c_str());
    tft.setCursor((W - w) / 2, OFFSET_Y + 60);
    tft.print(nombre.c_str());
    tft.setTextFont(2);
    tft.setTextColor(paleta_dark::COL_ACENTO, paleta_dark::COL_FONDO);
    int16_t wF = tft.textWidth(e.fechaHora.c_str());
    tft.setCursor((W - wF) / 2, OFFSET_Y + 105);
    tft.print(e.fechaHora.c_str());
    if (!e.ganador.empty()) {
      tft.setTextFont(2);
      tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
      const char* lbl = "Ganador:";
      int16_t wL = tft.textWidth(lbl);
      tft.setCursor((W - wL) / 2, OFFSET_Y + 145);
      tft.print(lbl);
      tft.setTextFont(4);
      tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
      std::string g = truncar(e.ganador, 18);
      int16_t wG = tft.textWidth(g.c_str());
      tft.setCursor((W - wG) / 2, OFFSET_Y + 170);
      tft.print(g.c_str());
    }
    return;
  }
  int y = OFFSET_Y + 34;
  for (int i = 0; i < n; ++i) {
    const auto& e = snap_.ultimos[i];
    tft.setTextFont(2);
    tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
    tft.setCursor(10, y);
    tft.print(truncar(e.nombre, 26).c_str());
    tft.setTextFont(1);
    tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
    tft.setCursor(10, y + 18);
    tft.print(e.ganador.empty() ? "-" : truncar(e.ganador, 40).c_str());
    y += 42;
  }
}

void PantallaMotogp::dibujarCalendario(TFT_eSPI& tft) {
  pintarFondo(tft);
  dibujarIndicador(tft);
  tft.setTextFont(2);
  tft.setTextColor(paleta_dark::COL_ACENTO, paleta_dark::COL_FONDO);
  tft.setCursor(10, OFFSET_Y + 6);
  tft.print("Calendario");
  const int n = std::min<int>(5, static_cast<int>(snap_.proximos.size()));
  if (n == 0) {
    tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
    tft.setCursor(10, OFFSET_Y + 100);
    tft.print("(sin datos)");
    return;
  }
  if (n == 1) {
    const auto& e = snap_.proximos[0];
    tft.setTextFont(4);
    tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
    std::string nombre = truncar(e.nombre, 20);
    int16_t w = tft.textWidth(nombre.c_str());
    tft.setCursor((W - w) / 2, OFFSET_Y + 80);
    tft.print(nombre.c_str());
    tft.setTextFont(2);
    tft.setTextColor(paleta_dark::COL_ACENTO, paleta_dark::COL_FONDO);
    int16_t wF = tft.textWidth(e.fechaHora.c_str());
    tft.setCursor((W - wF) / 2, OFFSET_Y + 130);
    tft.print(e.fechaHora.c_str());
    return;
  }
  int y = OFFSET_Y + 34;
  for (int i = 0; i < n; ++i) {
    const auto& e = snap_.proximos[i];
    tft.setTextFont(2);
    tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
    tft.setCursor(10, y);
    tft.print(truncar(e.nombre, 26).c_str());
    tft.setTextFont(1);
    tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
    int16_t wFH = tft.textWidth(e.fechaHora.c_str());
    tft.setCursor(W - 10 - wFH, y + 4);
    tft.print(e.fechaHora.c_str());
    y += 32;
  }
}
