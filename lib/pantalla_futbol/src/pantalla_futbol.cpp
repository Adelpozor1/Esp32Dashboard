#include "pantalla_futbol.h"
#include "tft_driver.h"
#include "paleta_dark.h"
#include <TFT_eSPI.h>
#include <cstdio>
#include <algorithm>

namespace {
constexpr int OFFSET_Y      = 20;
constexpr int W             = 320;
constexpr int H_CONTENIDO   = 220;
constexpr int PARPADEO_MS   = 600;   // periodo del badge EN VIVO

void pintarFondo(TFT_eSPI& tft) {
  tft.fillRect(0, OFFSET_Y, W, H_CONTENIDO, paleta_dark::COL_FONDO);
}

// Dibuja una caja rectangular con borde (sin relleno interior — para no borrar
// lo ya escrito dentro).
void dibujarCaja(TFT_eSPI& tft, int x, int y, int w, int h, uint16_t color) {
  tft.drawRect(x,     y,     w, h, color);
  tft.drawRect(x + 1, y + 1, w - 2, h - 2, color);
}
}  // namespace

std::string PantallaFutbol::truncar(const std::string& s, size_t n) {
  if (s.size() <= n) return s;
  return s.substr(0, n - 1) + ".";
}

void PantallaFutbol::alEntrar() {
  dirty_ = true;
  ultObtenidoMs_ = 0;
  ultParpadeoMs_ = 0;
  badgeEncendido_ = true;
}

void PantallaFutbol::alDeslizar(pantallas::Direccion dir) {
  if (dir == pantallas::Direccion::ARRIBA || dir == pantallas::Direccion::ABAJO) {
    sub_ = (sub_ == SubVista::RESULTADO) ? SubVista::AGENDA : SubVista::RESULTADO;
    dirty_ = true;
  }
}

void PantallaFutbol::dibujar(uint32_t msAhora) {
  auto& tft = tft_driver::obtenerTft();
  const bool datosNuevos = snap_.obtenido_ms != ultObtenidoMs_;

  // Parpadeo del badge EN VIVO — sólo si estamos en RESULTADO con live.
  bool refrescarBadge = false;
  if (sub_ == SubVista::RESULTADO && snap_.ok && snap_.hayLive) {
    if (ultParpadeoMs_ == 0 || (msAhora - ultParpadeoMs_) >= PARPADEO_MS) {
      badgeEncendido_ = !badgeEncendido_;
      ultParpadeoMs_ = msAhora;
      refrescarBadge = true;
    }
  }

  if (!dirty_ && !datosNuevos && !refrescarBadge) return;

  if (!snap_.ok) {
    dibujarSinDatos(tft);
    dirty_ = false;
    ultObtenidoMs_ = snap_.obtenido_ms;
    return;
  }
  if (sub_ == SubVista::RESULTADO) dibujarResultado(tft, msAhora);
  else                              dibujarAgenda(tft);
  dirty_ = false;
  ultObtenidoMs_ = snap_.obtenido_ms;
}

void PantallaFutbol::dibujarSinDatos(TFT_eSPI& tft) {
  pintarFondo(tft);
  dibujarIndicador(tft);
  tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
  tft.setTextFont(4);
  const char* t = "Futbol: sin datos";
  int16_t w = tft.textWidth(t);
  tft.setCursor((W - w) / 2, OFFSET_Y + 90);
  tft.print(t);
}

void PantallaFutbol::dibujarIndicador(TFT_eSPI& tft) {
  const int y = OFFSET_Y + 8;
  const int r = 3;
  const int xA = W - 22;
  const int xB = W - 10;
  const bool res = (sub_ == SubVista::RESULTADO);
  if (res) {
    tft.fillCircle(xA, y, r, paleta_dark::COL_ACENTO);
    tft.drawCircle(xB, y, r, paleta_dark::COL_TXT_SECUND);
  } else {
    tft.drawCircle(xA, y, r, paleta_dark::COL_TXT_SECUND);
    tft.fillCircle(xB, y, r, paleta_dark::COL_ACENTO);
  }
}

void PantallaFutbol::dibujarBadgeLive(TFT_eSPI& tft, int x, int y, bool encendido) {
  const int bw = 66;
  const int bh = 20;
  const uint16_t col = encendido ? paleta_dark::COL_LIVE : paleta_dark::COL_FONDO;
  tft.fillRect(x, y, bw, bh, col);
  tft.drawRect(x, y, bw, bh, paleta_dark::COL_LIVE);
  tft.setTextFont(2);
  const uint16_t txtCol = encendido ? paleta_dark::COL_TXT_TITULO
                                     : paleta_dark::COL_LIVE;
  tft.setTextColor(txtCol, col);
  const char* t = "EN VIVO";
  int16_t tw = tft.textWidth(t);
  tft.setCursor(x + (bw - tw) / 2, y + 3);
  tft.print(t);
}

void PantallaFutbol::dibujarResultado(TFT_eSPI& tft, uint32_t /*msAhora*/) {
  pintarFondo(tft);
  dibujarIndicador(tft);

  const bool live = snap_.hayLive;
  const int  cabeceraY = OFFSET_Y + 6;

  // Cabecera: badge EN VIVO + título "Ultimo/Resultado".
  if (live) {
    dibujarBadgeLive(tft, 10, cabeceraY, badgeEncendido_);
    tft.setTextFont(2);
    tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
    tft.setCursor(10 + 66 + 10, cabeceraY + 3);
    tft.print("En directo");
  } else {
    tft.setTextFont(2);
    tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
    tft.setCursor(10, cabeceraY + 3);
    tft.print("Ultimo partido");
  }

  // Datos.
  std::string local, visitante, subFecha;
  int gL = 0, gV = 0;
  bool valido = false;
  if (live) {
    local = snap_.live.local;
    visitante = snap_.live.visitante;
    gL = snap_.live.golesLocal;
    gV = snap_.live.golesVisitante;
    if (!snap_.live.progreso.empty()) {
      subFecha = snap_.live.progreso;
      // Si es numérico añade la comilla ("32'"); si es "HT" lo deja tal cual.
      bool esNum = !subFecha.empty();
      for (char c : subFecha) if (c < '0' || c > '9') { esNum = false; break; }
      if (esNum) subFecha += "'";
    }
    valido = true;
  } else if (snap_.hayUltimo) {
    const auto& p = snap_.ultimoJugado;
    local = p.local;
    visitante = p.visitante;
    gL = p.golesLocal;
    gV = p.golesVisitante;
    subFecha = p.fechaHora;
    valido = (gL >= 0 && gV >= 0);
  }

  // Caja central.
  const int cajaX = 10;
  const int cajaY = OFFSET_Y + 40;
  const int cajaW = W - 20;
  const int cajaH = 140;
  dibujarCaja(tft, cajaX, cajaY, cajaW, cajaH, paleta_dark::COL_ACENTO);

  if (!valido && !live && !snap_.hayUltimo) {
    tft.setTextFont(2);
    tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
    const char* t = "(sin resultado reciente)";
    int16_t w = tft.textWidth(t);
    tft.setCursor((W - w) / 2, cajaY + cajaH / 2 - 8);
    tft.print(t);
    return;
  }

  // LOCAL — font 4, blanco, alineado a la izquierda dentro de la caja.
  tft.setTextFont(4);
  tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
  std::string sLoc = truncar(local, 10);
  tft.setCursor(cajaX + 14, cajaY + 18);
  tft.print(sLoc.c_str());

  // VISITANTE — font 4, blanco, alineado a la derecha.
  std::string sVis = truncar(visitante, 10);
  tft.setTextFont(4);
  int16_t wVis = tft.textWidth(sVis.c_str());
  tft.setCursor(cajaX + cajaW - 14 - wVis, cajaY + cajaH - 42);
  tft.print(sVis.c_str());

  // Resultado central — font 7, color acento (o COL_OK si local gana en último).
  char buf[16];
  if (live || (gL >= 0 && gV >= 0)) {
    std::snprintf(buf, sizeof(buf), "%d - %d", gL, gV);
  } else {
    std::snprintf(buf, sizeof(buf), "vs");
  }
  uint16_t colRes = paleta_dark::COL_ACENTO;
  if (!live && gL > gV) colRes = paleta_dark::COL_OK;
  tft.setTextFont(7);
  tft.setTextColor(colRes, paleta_dark::COL_FONDO);
  int16_t wR = tft.textWidth(buf);
  tft.setCursor((W - wR) / 2, cajaY + cajaH / 2 - 26);
  tft.print(buf);

  // Sub-fecha / minuto.
  if (!subFecha.empty()) {
    tft.setTextFont(2);
    uint16_t colSub = live ? paleta_dark::COL_LIVE : paleta_dark::COL_TXT_SECUND;
    tft.setTextColor(colSub, paleta_dark::COL_FONDO);
    int16_t wF = tft.textWidth(subFecha.c_str());
    tft.setCursor((W - wF) / 2, cajaY + cajaH + 14);
    tft.print(subFecha.c_str());
  }
}

void PantallaFutbol::dibujarAgenda(TFT_eSPI& tft) {
  pintarFondo(tft);
  dibujarIndicador(tft);

  tft.setTextFont(2);
  tft.setTextColor(paleta_dark::COL_ACENTO, paleta_dark::COL_FONDO);
  tft.setCursor(10, OFFSET_Y + 9);
  tft.print("Hoy y manana");

  const int n = std::min<int>(4, static_cast<int>(snap_.hoyManana.size()));
  if (n == 0) {
    tft.setTextFont(2);
    tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
    const char* t = "(sin partidos hoy o manana)";
    int16_t w = tft.textWidth(t);
    tft.setCursor((W - w) / 2, OFFSET_Y + 100);
    tft.print(t);
    return;
  }

  int y = OFFSET_Y + 40;
  const int filaH = 44;
  for (int i = 0; i < n; ++i) {
    const auto& p = snap_.hoyManana[i];
    // Caja fila.
    dibujarCaja(tft, 8, y, W - 16, filaH - 6, paleta_dark::COL_CAJA);

    // Hora en font 4 — extrae "HH:MM" de fechaHora ("Vie 5 sep 14:15").
    std::string hora;
    // Busca los últimos 5 chars con formato HH:MM.
    if (p.fechaHora.size() >= 5) {
      hora = p.fechaHora.substr(p.fechaHora.size() - 5);
      // valida que sea "NN:NN"
      bool ok = (hora[2] == ':') &&
                (hora[0] >= '0' && hora[0] <= '9') &&
                (hora[1] >= '0' && hora[1] <= '9') &&
                (hora[3] >= '0' && hora[3] <= '9') &&
                (hora[4] >= '0' && hora[4] <= '9');
      if (!ok) hora.clear();
    }
    tft.setTextFont(4);
    tft.setTextColor(paleta_dark::COL_ACENTO, paleta_dark::COL_FONDO);
    tft.setCursor(16, y + 8);
    tft.print(hora.empty() ? "--:--" : hora.c_str());

    // Nombre local vs visitante en font 2 a la derecha de la hora.
    tft.setTextFont(2);
    tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
    std::string linea = truncar(p.local, 10) + " vs " + truncar(p.visitante, 10);
    tft.setCursor(100, y + 8);
    tft.print(linea.c_str());

    // Fecha bonita (sin hora) debajo.
    tft.setTextFont(1);
    tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
    std::string fecha = p.fechaHora;
    if (fecha.size() > 6 && fecha[fecha.size() - 3] == ':') {
      // recorta " HH:MM"
      fecha = fecha.substr(0, fecha.size() - 6);
    }
    tft.setCursor(100, y + 26);
    tft.print(fecha.c_str());

    y += filaH;
  }
}
