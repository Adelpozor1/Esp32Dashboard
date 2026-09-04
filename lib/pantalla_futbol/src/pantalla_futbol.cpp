#include "pantalla_futbol.h"
#include "tft_driver.h"
#include "paleta_dark.h"
#include <TFT_eSPI.h>
#include <cstdio>
#include <algorithm>

namespace {
constexpr int OFFSET_Y = 20;
constexpr int W = 320;
constexpr int H_CONTENIDO = 220;

void pintarFondo(TFT_eSPI& tft) {
  tft.fillRect(0, OFFSET_Y, W, H_CONTENIDO, paleta_dark::COL_FONDO);
}
}  // namespace

std::string PantallaFutbol::truncar(const std::string& s, size_t n) {
  if (s.size() <= n) return s;
  return s.substr(0, n - 1) + ".";
}

void PantallaFutbol::alEntrar() {
  dirty_ = true;
  ultObtenidoMs_ = 0;
}

void PantallaFutbol::alDeslizar(pantallas::Direccion dir) {
  if (dir == pantallas::Direccion::ARRIBA || dir == pantallas::Direccion::ABAJO) {
    sub_ = (sub_ == SubVista::ULTIMOS) ? SubVista::PROXIMOS : SubVista::ULTIMOS;
    dirty_ = true;
  }
}

void PantallaFutbol::dibujar(uint32_t) {
  auto& tft = tft_driver::obtenerTft();
  const bool datosNuevos = snap_.obtenido_ms != ultObtenidoMs_;
  if (!dirty_ && !datosNuevos) return;

  if (!snap_.ok) {
    dibujarSinDatos(tft);
    dirty_ = false;
    ultObtenidoMs_ = snap_.obtenido_ms;
    return;
  }
  if (sub_ == SubVista::ULTIMOS) dibujarUltimos(tft);
  else                            dibujarProximos(tft);
  dirty_ = false;
  ultObtenidoMs_ = snap_.obtenido_ms;
}

void PantallaFutbol::dibujarSinDatos(TFT_eSPI& tft) {
  pintarFondo(tft);
  tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
  tft.setTextFont(4);
  const char* t = "Futbol: sin datos";
  int16_t w = tft.textWidth(t);
  tft.setCursor((W - w) / 2, OFFSET_Y + 90);
  tft.print(t);
}

void PantallaFutbol::dibujarIndicador(TFT_eSPI& tft) {
  const int y = OFFSET_Y + 6;
  const int r = 3;
  const int xA = W - 22;
  const int xB = W - 10;
  const bool ultimos = (sub_ == SubVista::ULTIMOS);
  if (ultimos) {
    tft.fillCircle(xA, y, r, paleta_dark::COL_ACENTO);
    tft.drawCircle(xB, y, r, paleta_dark::COL_TXT_SECUND);
  } else {
    tft.drawCircle(xA, y, r, paleta_dark::COL_TXT_SECUND);
    tft.fillCircle(xB, y, r, paleta_dark::COL_ACENTO);
  }
}

void PantallaFutbol::dibujarUltimos(TFT_eSPI& tft) {
  pintarFondo(tft);
  dibujarIndicador(tft);
  // Título
  tft.setTextFont(2);
  tft.setTextColor(paleta_dark::COL_ACENTO, paleta_dark::COL_FONDO);
  tft.setCursor(10, OFFSET_Y + 6);
  tft.print("Ultima jornada");
  // Filas
  int y = OFFSET_Y + 34;
  tft.setTextFont(2);
  const int n = std::min<int>(5, static_cast<int>(snap_.ultimos.size()));
  if (n == 0) {
    tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
    tft.setCursor(10, y);
    tft.print("(sin partidos)");
    return;
  }
  for (int i = 0; i < n; ++i) {
    const auto& p = snap_.ultimos[i];
    // Local
    tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
    tft.setCursor(10, y);
    tft.print(truncar(p.local, 12).c_str());
    // Resultado
    char buf[12];
    if (p.golesLocal >= 0 && p.golesVisitante >= 0) {
      std::snprintf(buf, sizeof(buf), "%d - %d", p.golesLocal, p.golesVisitante);
    } else {
      std::snprintf(buf, sizeof(buf), "-");
    }
    tft.setTextColor(paleta_dark::COL_ACENTO, paleta_dark::COL_FONDO);
    int16_t wRes = tft.textWidth(buf);
    tft.setCursor((W - wRes) / 2, y);
    tft.print(buf);
    // Visitante alineado a la derecha
    std::string vis = truncar(p.visitante, 12);
    int16_t wVis = tft.textWidth(vis.c_str());
    tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
    tft.setCursor(W - 10 - wVis, y);
    tft.print(vis.c_str());
    y += 34;
  }
}

void PantallaFutbol::dibujarProximos(TFT_eSPI& tft) {
  pintarFondo(tft);
  dibujarIndicador(tft);
  tft.setTextFont(2);
  tft.setTextColor(paleta_dark::COL_ACENTO, paleta_dark::COL_FONDO);
  tft.setCursor(10, OFFSET_Y + 6);
  tft.print("Proxima jornada");
  int y = OFFSET_Y + 34;
  const int n = std::min<int>(4, static_cast<int>(snap_.proximos.size()));   // 4 partidos porque cada uno son 2 filas
  if (n == 0) {
    tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
    tft.setCursor(10, y);
    tft.print("(sin partidos)");
    return;
  }
  for (int i = 0; i < n; ++i) {
    const auto& p = snap_.proximos[i];
    // Fila 1: LOCAL vs VISITANTE
    tft.setTextFont(2);
    tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
    tft.setCursor(10, y);
    tft.print(truncar(p.local, 12).c_str());
    // "vs" centrado
    tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
    int16_t wVs = tft.textWidth("vs");
    tft.setCursor((W - wVs) / 2, y);
    tft.print("vs");
    // Visitante
    std::string vis = truncar(p.visitante, 12);
    int16_t wVis = tft.textWidth(vis.c_str());
    tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
    tft.setCursor(W - 10 - wVis, y);
    tft.print(vis.c_str());
    // Fila 2: fecha/hora en gris centrada
    tft.setTextFont(1);
    tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
    int16_t wFH = tft.textWidth(p.fechaHora.c_str());
    tft.setCursor((W - wFH) / 2, y + 18);
    tft.print(p.fechaHora.c_str());
    y += 42;
  }
}
