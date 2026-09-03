#include "pantalla_reloj.h"
#include "tft_driver.h"
#include "paleta_dark.h"
#include <TFT_eSPI.h>
#include <time.h>
#include <cstdio>

namespace {
constexpr int OFFSET_Y = 20;
constexpr int LARGO_HORA_PX = 168;   // ancho aprox de "HH:MM" en font 7 (7*24 - lo justamos empíricamente)
constexpr int Y_HORA        = OFFSET_Y + 60;
constexpr int Y_FECHA       = OFFSET_Y + 160;
constexpr int Y_SYNC        = OFFSET_Y + 80;

const char* diasSemana[7] = {
  "domingo", "lunes", "martes", "miércoles", "jueves", "viernes", "sábado",
};
const char* meses[12] = {
  "enero", "febrero", "marzo", "abril", "mayo", "junio",
  "julio", "agosto", "septiembre", "octubre", "noviembre", "diciembre",
};

void pintarFondo(TFT_eSPI& tft) {
  tft.fillRect(0, OFFSET_Y, 320, 220, paleta_dark::COL_FONDO);
}

void pintarSincronizando(TFT_eSPI& tft) {
  pintarFondo(tft);
  tft.setTextColor(paleta_dark::COL_ACENTO, paleta_dark::COL_FONDO);
  tft.setTextFont(4);
  const char* txt = "Sincronizando...";
  int16_t ancho = tft.textWidth(txt);
  tft.setCursor((320 - ancho) / 2, Y_SYNC);
  tft.print(txt);
}

void pintarHoraMinuto(TFT_eSPI& tft, int h, int m) {
  char buf[8];
  std::snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
  tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
  tft.setTextFont(7);
  // Rect que cubre la zona antigua: ~192 px de ancho, 56 px de alto, centrado.
  const int x = (320 - LARGO_HORA_PX) / 2;
  tft.fillRect(x - 4, Y_HORA - 4, LARGO_HORA_PX + 8, 56, paleta_dark::COL_FONDO);
  tft.setCursor(x, Y_HORA);
  tft.print(buf);
}

void pintarSegundos(TFT_eSPI& tft, int s) {
  char buf[8];
  std::snprintf(buf, sizeof(buf), ":%02d", s);
  tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
  tft.setTextFont(4);
  // Colocados a la derecha del bloque HH:MM.
  const int xIni = (320 + LARGO_HORA_PX) / 2 + 4;
  tft.fillRect(xIni - 2, Y_HORA + 2, 60, 30, paleta_dark::COL_FONDO);
  tft.setCursor(xIni, Y_HORA + 4);
  tft.print(buf);
}

void pintarFecha(TFT_eSPI& tft, const struct tm& tm) {
  char buf[48];
  std::snprintf(buf, sizeof(buf), "%s %d %s",
                diasSemana[tm.tm_wday % 7],
                tm.tm_mday,
                meses[tm.tm_mon % 12]);
  tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
  tft.setTextFont(2);
  int16_t ancho = tft.textWidth(buf);
  tft.fillRect(0, Y_FECHA - 2, 320, 22, paleta_dark::COL_FONDO);
  tft.setCursor((320 - ancho) / 2, Y_FECHA);
  tft.print(buf);
}
}  // namespace

void PantallaReloj::alEntrar() {
  dirty_ = true;
  ultSeg_ = -1;
  ultMin_ = -1;
  ultDia_ = -1;
}

void PantallaReloj::dibujar(uint32_t) {
  auto& tft = tft_driver::obtenerTft();

  time_t now = time(nullptr);
  struct tm tm;
  localtime_r(&now, &tm);
  const bool sincronizado = (tm.tm_year + 1900) >= 2000;

  if (!sincronizado) {
    if (dirty_) {
      pintarSincronizando(tft);
      dirty_ = false;
    }
    return;
  }

  if (dirty_) {
    pintarFondo(tft);
    pintarHoraMinuto(tft, tm.tm_hour, tm.tm_min);
    pintarSegundos(tft, tm.tm_sec);
    pintarFecha(tft, tm);
    ultSeg_ = tm.tm_sec;
    ultMin_ = tm.tm_min;
    ultDia_ = tm.tm_mday;
    dirty_ = false;
    return;
  }
  if (tm.tm_min != ultMin_) {
    pintarHoraMinuto(tft, tm.tm_hour, tm.tm_min);
    ultMin_ = tm.tm_min;
  }
  if (tm.tm_sec != ultSeg_) {
    pintarSegundos(tft, tm.tm_sec);
    ultSeg_ = tm.tm_sec;
  }
  if (tm.tm_mday != ultDia_) {
    pintarFecha(tft, tm);
    ultDia_ = tm.tm_mday;
  }
}
