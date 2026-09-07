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

const char* MESES[13] = {"", "ene","feb","mar","abr","may","jun",
                         "jul","ago","sep","oct","nov","dic"};
const char* DIAS[7]   = {"Dom","Lun","Mar","Mie","Jue","Vie","Sab"};

void pintarFondo(TFT_eSPI& tft) {
  tft.fillRect(0, OFFSET_Y, W, H_CONTENIDO, paleta_dark::COL_FONDO);
}
}  // namespace

void PantallaMeteo::alEntrar() {
  dirty_ = true;
  ultObtenidoMs_ = 0;
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
  dibujarVista(tft);
  dirty_ = false;
  ultObtenidoMs_ = snap_.obtenido_ms;
}

void PantallaMeteo::dibujarSinDatos(TFT_eSPI& tft) {
  pintarFondo(tft);
  tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
  tft.setTextFont(4);
  const char* t = (snap_.obtenido_ms == 0) ? "Cargando datos..." : "Meteo: sin datos";
  int16_t w = tft.textWidth(t);
  tft.setCursor((W - w) / 2, OFFSET_Y + 90);
  tft.print(t);
}

void PantallaMeteo::dibujarVista(TFT_eSPI& tft) {
  pintarFondo(tft);
  dibujarHoy(tft);
  dibujarProximosDias(tft);
}

void PantallaMeteo::dibujarHoy(TFT_eSPI& tft) {
  // Panel superior (aprox 100 px de alto): temperatura grande + icono + viento.
  char buf[24];

  // Temperatura actual en font 7 (LCD grande).
  std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(std::round(snap_.temp_actual_c)));
  tft.setTextFont(7);
  tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
  tft.setCursor(14, OFFSET_Y + 8);
  tft.print(buf);
  const int xTemp = 14 + tft.textWidth(buf);
  tft.setTextFont(4);
  tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
  tft.setCursor(xTemp + 4, OFFSET_Y + 18);
  tft.print("\xB0" "C");

  // Icono actual grande a la derecha.
  dibujarIcono(tft, 250, OFFSET_Y + 40, 60, snap_.codigo_actual);

  // Etiqueta "Hoy" + min/max si tenemos el día actual en snap_.dias[0].
  tft.setTextFont(2);
  tft.setTextColor(paleta_dark::COL_ACENTO, paleta_dark::COL_FONDO);
  tft.setCursor(14, OFFSET_Y + 70);
  tft.print("Hoy");
  if (!snap_.dias.empty()) {
    const auto& h = snap_.dias[0];
    std::snprintf(buf, sizeof(buf), "%d\xB0 / %d\xB0",
                  static_cast<int>(std::round(h.tmin)),
                  static_cast<int>(std::round(h.tmax)));
    tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
    tft.setCursor(50, OFFSET_Y + 70);
    tft.print(buf);
  }

  // Viento a la derecha (bajo el icono).
  std::snprintf(buf, sizeof(buf), "Viento %d km/h", snap_.viento_kmh);
  tft.setTextFont(2);
  tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
  int16_t wV = tft.textWidth(buf);
  tft.setCursor(W - 10 - wV, OFFSET_Y + 88);
  tft.print(buf);

  // Separador entre bloque hoy y lista.
  tft.drawFastHLine(6, OFFSET_Y + 105, W - 12, paleta_dark::COL_CAJA);
}

void PantallaMeteo::dibujarProximosDias(TFT_eSPI& tft) {
  // Lista de 4 días siguientes a hoy (snap_.dias[1..4]) con fecha, icono y min/max.
  const int inicio = 1;   // saltamos hoy
  const int nDisp = std::max<int>(0, static_cast<int>(snap_.dias.size()) - inicio);
  const int n = std::min<int>(4, nDisp);
  const int filaH = 26;
  int y = OFFSET_Y + 114;
  char buf[24];
  for (int i = 0; i < n; ++i) {
    const auto& d = snap_.dias[inicio + i];
    // Día semana (Lun/Mar/…) con protección de rango.
    const int wd = (d.dia_semana >= 0 && d.dia_semana < 7) ? d.dia_semana : 0;
    tft.setTextFont(2);
    tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
    tft.setCursor(10, y + 4);
    tft.print(DIAS[wd]);
    // Fecha "d mmm"
    const int mes = (d.mes >= 1 && d.mes <= 12) ? d.mes : 0;
    std::snprintf(buf, sizeof(buf), "%d %s", d.dia_mes, MESES[mes]);
    tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
    tft.setCursor(70, y + 4);
    tft.print(buf);
    // Icono
    dibujarIcono(tft, 170, y + 12, 18, d.codigo);
    // Min/max a la derecha
    std::snprintf(buf, sizeof(buf), "%d\xB0 / %d\xB0",
                  static_cast<int>(std::round(d.tmin)),
                  static_cast<int>(std::round(d.tmax)));
    tft.setTextFont(2);
    tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
    int16_t wt = tft.textWidth(buf);
    tft.setCursor(W - 12 - wt, y + 4);
    tft.print(buf);
    y += filaH;
  }
  if (n == 0) {
    tft.setTextFont(2);
    tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
    const char* t = "(sin prevision)";
    int16_t w = tft.textWidth(t);
    tft.setCursor((W - w) / 2, OFFSET_Y + 150);
    tft.print(t);
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
      tft.drawLine(cx - r / 2, y1, cx - r / 2 - 2, y2, COL_LLUV);
      tft.drawLine(cx,         y1, cx - 2,         y2, COL_LLUV);
      tft.drawLine(cx + r / 2, y1, cx + r / 2 - 2, y2, COL_LLUV);
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
    case IconoMeteo::SOL_NUBE: {
      const int rSol = r - 2;
      const int solCx = cx - r / 2;
      const int solCy = cy - r / 3;
      tft.fillCircle(solCx, solCy, rSol, COL_SOL);
      for (int a = 0; a < 360; a += 60) {
        const double rad = a * M_PI / 180.0;
        const int x1 = solCx + int((rSol + 2) * std::cos(rad));
        const int y1 = solCy + int((rSol + 2) * std::sin(rad));
        const int x2 = solCx + int((rSol + lado / 8) * std::cos(rad));
        const int y2 = solCy + int((rSol + lado / 8) * std::sin(rad));
        tft.drawLine(x1, y1, x2, y2, COL_SOL);
      }
      const int nCx = cx + r / 3;
      const int nCy = cy + r / 4;
      tft.fillCircle(nCx - r / 2, nCy - 1, r / 2 + 1, COL_NUBE);
      tft.fillCircle(nCx + r / 2, nCy - 1, r / 2 + 1, COL_NUBE);
      tft.fillCircle(nCx, nCy - r / 2, r / 2 + 1, COL_NUBE);
      tft.fillRect(nCx - r, nCy - 1, 2 * r, r / 2 + 1, COL_NUBE);
      break;
    }
  }
}
