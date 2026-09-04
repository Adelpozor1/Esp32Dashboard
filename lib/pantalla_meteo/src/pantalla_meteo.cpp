#include "pantalla_meteo.h"
#include "tft_driver.h"
#include "paleta_dark.h"
#include <TFT_eSPI.h>
#include <cstdio>
#include <cmath>
#include <algorithm>

namespace {
constexpr int OFFSET_Y = 20;
constexpr int W = 320;
constexpr int H_CONTENIDO = 220;

constexpr uint16_t COL_SOL   = 0xFEA0;
constexpr uint16_t COL_NUBE  = 0xBDF7;
constexpr uint16_t COL_LLUV  = 0x5D9F;
constexpr uint16_t COL_TORM  = 0xFEA0;
constexpr uint16_t COL_NIEV  = 0xFFFF;

void pintarFondo(TFT_eSPI& tft) {
  tft.fillRect(0, OFFSET_Y, W, H_CONTENIDO, paleta_dark::COL_FONDO);
}
}  // namespace

void PantallaMeteo::alEntrar() {
  dirty_ = true;
  ultObtenidoMs_ = 0;
}

void PantallaMeteo::alDeslizar(pantallas::Direccion dir) {
  if (dir == pantallas::Direccion::ARRIBA || dir == pantallas::Direccion::ABAJO) {
    sub_ = (sub_ == SubVista::HORAS) ? SubVista::DIAS : SubVista::HORAS;
    dirty_ = true;
  }
}

void PantallaMeteo::dibujar(uint32_t) {
  auto& tft = tft_driver::obtenerTft();
  const bool datosNuevos = snap_.obtenido_ms != ultObtenidoMs_;
  if (!dirty_ && !datosNuevos) return;

  if (!snap_.ok) {
    dibujarSinDatos(tft);
    dirty_ = false;
    ultObtenidoMs_ = snap_.obtenido_ms;
    return;
  }
  if (sub_ == SubVista::HORAS) dibujarHoras(tft);
  else                          dibujarDias(tft);
  dirty_ = false;
  ultObtenidoMs_ = snap_.obtenido_ms;
}

void PantallaMeteo::dibujarSinDatos(TFT_eSPI& tft) {
  pintarFondo(tft);
  tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
  tft.setTextFont(4);
  const char* t = "Meteo: sin datos";
  int16_t w = tft.textWidth(t);
  tft.setCursor((W - w) / 2, OFFSET_Y + 90);
  tft.print(t);
}

void PantallaMeteo::dibujarBloqueActual(TFT_eSPI& tft) {
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(std::round(snap_.temp_actual_c)));
  tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
  tft.setTextFont(7);
  tft.setCursor(14, OFFSET_Y + 14);
  tft.print(buf);
  const int xTemp = 14 + tft.textWidth(buf);
  tft.setTextFont(4);
  tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
  tft.setCursor(xTemp + 4, OFFSET_Y + 24);
  tft.print("\xB0" "C");
  dibujarIcono(tft, 250, OFFSET_Y + 46, 60, snap_.codigo_actual);
  tft.setTextFont(2);
  tft.setCursor(14, OFFSET_Y + 78);
  std::snprintf(buf, sizeof(buf), "Viento %d km/h", snap_.viento_kmh);
  tft.print(buf);
}

void PantallaMeteo::dibujarHoras(TFT_eSPI& tft) {
  pintarFondo(tft);
  dibujarBloqueActual(tft);
  dibujarIndicador(tft);
  const int n = std::min<int>(6, static_cast<int>(snap_.horas.size()));
  const int slot = W / 6;
  for (int i = 0; i < n; ++i) {
    const size_t idx = static_cast<size_t>(i) * 2 < snap_.horas.size() ? i * 2 : i;
    const auto& h = snap_.horas[idx];
    const int cx = slot * i + slot / 2;
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%02d", h.hora);
    tft.setTextFont(1);
    tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
    int16_t wh = tft.textWidth(buf);
    tft.setCursor(cx - wh / 2, OFFSET_Y + 118);
    tft.print(buf);
    dibujarIcono(tft, cx, OFFSET_Y + 148, 22, h.codigo);
    tft.setTextFont(2);
    tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
    std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(std::round(h.temp_c)));
    int16_t wt = tft.textWidth(buf);
    tft.setCursor(cx - wt / 2, OFFSET_Y + 180);
    tft.print(buf);
  }
}

void PantallaMeteo::dibujarDias(TFT_eSPI& tft) {
  pintarFondo(tft);
  dibujarBloqueActual(tft);
  dibujarIndicador(tft);
  const int n = std::min<int>(5, static_cast<int>(snap_.dias.size()));
  int y = OFFSET_Y + 110;
  tft.setTextFont(2);
  char buf[24];
  for (int i = 0; i < n; ++i) {
    const auto& d = snap_.dias[i];
    tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
    std::snprintf(buf, sizeof(buf), "%02d", d.dia_mes);
    tft.setCursor(20, y);
    tft.print(buf);
    dibujarIcono(tft, 90, y + 10, 18, d.codigo);
    tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
    std::snprintf(buf, sizeof(buf), "%d\xB0 - %d\xB0",
                  static_cast<int>(std::round(d.tmin)),
                  static_cast<int>(std::round(d.tmax)));
    tft.setCursor(140, y);
    tft.print(buf);
    y += 22;
  }
}

void PantallaMeteo::dibujarIndicador(TFT_eSPI& tft) {
  const int y = OFFSET_Y + 6;
  const int r = 3;
  const int xA = W - 22;
  const int xB = W - 10;
  const bool horas = (sub_ == SubVista::HORAS);
  if (horas) {
    tft.fillCircle(xA, y, r, paleta_dark::COL_ACENTO);
    tft.drawCircle(xB, y, r, paleta_dark::COL_TXT_SECUND);
  } else {
    tft.drawCircle(xA, y, r, paleta_dark::COL_TXT_SECUND);
    tft.fillCircle(xB, y, r, paleta_dark::COL_ACENTO);
  }
}

void PantallaMeteo::dibujarIcono(TFT_eSPI& tft, int cx, int cy, int lado, int wmo) {
  IconoMeteo cat = MeteoClient::categoria(wmo);
  const int r = lado / 3;
  switch (cat) {
    case IconoMeteo::SOL: {
      tft.fillCircle(cx, cy, r, COL_SOL);
      for (int a = 0; a < 360; a += 45) {
        const double rad = a * M_PI / 180.0;
        const int x1 = cx + int((r + 2) * std::cos(rad));
        const int y1 = cy + int((r + 2) * std::sin(rad));
        const int x2 = cx + int((r + lado / 6) * std::cos(rad));
        const int y2 = cy + int((r + lado / 6) * std::sin(rad));
        tft.drawLine(x1, y1, x2, y2, COL_SOL);
      }
      break;
    }
    case IconoMeteo::NUBE: {
      tft.fillCircle(cx - r / 2, cy - 1, r / 2 + 1, COL_NUBE);
      tft.fillCircle(cx + r / 2, cy - 1, r / 2 + 1, COL_NUBE);
      tft.fillCircle(cx, cy - r / 2, r / 2 + 1, COL_NUBE);
      tft.fillRect(cx - r, cy - 1, 2 * r, r / 2 + 1, COL_NUBE);
      break;
    }
    case IconoMeteo::LLUVIA: {
      tft.fillCircle(cx - r / 2, cy - 2, r / 2 + 1, COL_NUBE);
      tft.fillCircle(cx + r / 2, cy - 2, r / 2 + 1, COL_NUBE);
      tft.fillCircle(cx, cy - r / 2 - 2, r / 2 + 1, COL_NUBE);
      tft.fillRect(cx - r, cy - 2, 2 * r, r / 2 + 1, COL_NUBE);
      const int y1 = cy + r / 2 + 1;
      const int y2 = y1 + std::max(3, lado / 6);
      tft.drawLine(cx - r / 2, y1, cx - r / 2, y2, COL_LLUV);
      tft.drawLine(cx,         y1, cx,         y2, COL_LLUV);
      tft.drawLine(cx + r / 2, y1, cx + r / 2, y2, COL_LLUV);
      break;
    }
    case IconoMeteo::NIEVE: {
      tft.fillCircle(cx - r / 2, cy - 2, r / 2 + 1, COL_NUBE);
      tft.fillCircle(cx + r / 2, cy - 2, r / 2 + 1, COL_NUBE);
      tft.fillCircle(cx, cy - r / 2 - 2, r / 2 + 1, COL_NUBE);
      tft.fillRect(cx - r, cy - 2, 2 * r, r / 2 + 1, COL_NUBE);
      tft.setTextFont(1);
      tft.setTextColor(COL_NIEV, paleta_dark::COL_FONDO);
      tft.setCursor(cx - r / 2 - 2, cy + r / 2 + 1);
      tft.print("* * *");
      break;
    }
    case IconoMeteo::TORMENTA: {
      tft.fillCircle(cx - r / 2, cy - 2, r / 2 + 1, COL_NUBE);
      tft.fillCircle(cx + r / 2, cy - 2, r / 2 + 1, COL_NUBE);
      tft.fillCircle(cx, cy - r / 2 - 2, r / 2 + 1, COL_NUBE);
      tft.fillRect(cx - r, cy - 2, 2 * r, r / 2 + 1, COL_NUBE);
      tft.fillTriangle(cx - 2, cy + 4, cx + 4, cy + 4, cx + 1, cy + r + 4, COL_TORM);
      tft.fillTriangle(cx - 4, cy + r + 4, cx + 4, cy + r + 4, cx, cy + r + 10, COL_TORM);
      break;
    }
    case IconoMeteo::NIEBLA: {
      for (int i = -2; i <= 2; ++i) {
        tft.drawFastHLine(cx - r, cy + i * 3, 2 * r, COL_NUBE);
      }
      break;
    }
  }
}
