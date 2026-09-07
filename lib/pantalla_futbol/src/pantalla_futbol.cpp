#include "pantalla_futbol.h"
#include "tft_driver.h"
#include "paleta_dark.h"
#include <TFT_eSPI.h>
#include <cstdio>
#include <algorithm>

namespace {
constexpr int OFFSET_Y    = 20;
constexpr int W           = 320;
constexpr int H_CONTENIDO = 220;

// Botón toggle Champions / LaLiga en la cabecera, arriba-derecha.
constexpr int BTN_W  = 88;
constexpr int BTN_H  = 22;
constexpr int BTN_X  = W - BTN_W - 6;
constexpr int BTN_Y  = OFFSET_Y + 4;

void pintarFondo(TFT_eSPI& tft) {
  tft.fillRect(0, OFFSET_Y, W, H_CONTENIDO, paleta_dark::COL_FONDO);
}

void dibujarCaja(TFT_eSPI& tft, int x, int y, int w, int h, uint16_t color) {
  tft.drawRect(x, y, w, h, color);
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
    sub_ = (sub_ == SubVista::JORNADA_ACTUAL) ? SubVista::PROXIMA_JORNADA
                                              : SubVista::JORNADA_ACTUAL;
    dirty_ = true;
  }
}

bool PantallaFutbol::tapEnBotonToggle(int x, int y) const {
  // y viene relativo al área de contenido (0..220). Sumamos OFFSET_Y.
  const int yAbs = y + OFFSET_Y;
  return snap_.hayChampionsDisponible &&
         x >= BTN_X && x <= BTN_X + BTN_W &&
         yAbs >= BTN_Y && yAbs <= BTN_Y + BTN_H;
}

void PantallaFutbol::alTocar(int x, int y) {
  if (tapEnBotonToggle(x, y) && onToggle_) {
    onToggle_();
  }
}

void PantallaFutbol::dibujar(uint32_t /*msAhora*/) {
  auto& tft = tft_driver::obtenerTft();
  const bool datosNuevos = snap_.obtenido_ms != ultObtenidoMs_;
  if (!dirty_ && !datosNuevos) return;

  if (!snap_.ok) {
    dibujarSinDatos(tft);
    dirty_ = false;
    ultObtenidoMs_ = snap_.obtenido_ms;
    return;
  }
  if (sub_ == SubVista::JORNADA_ACTUAL) dibujarJornadaActual(tft);
  else                                   dibujarProximaJornada(tft);
  dirty_ = false;
  ultObtenidoMs_ = snap_.obtenido_ms;
}

void PantallaFutbol::dibujarSinDatos(TFT_eSPI& tft) {
  pintarFondo(tft);
  dibujarCabecera(tft);
  dibujarIndicador(tft);
  tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
  tft.setTextFont(4);
  const char* t = (snap_.obtenido_ms == 0) ? "Cargando datos..." : "Futbol: sin datos";
  int16_t w = tft.textWidth(t);
  tft.setCursor((W - w) / 2, OFFSET_Y + 90);
  tft.print(t);
}

void PantallaFutbol::dibujarCabecera(TFT_eSPI& tft) {
  // Título competición actual (izquierda)
  tft.setTextFont(2);
  tft.setTextColor(paleta_dark::COL_ACENTO, paleta_dark::COL_FONDO);
  tft.setCursor(8, OFFSET_Y + 7);
  const char* nombre = (snap_.competicion == Competicion::CHAMPIONS)
      ? "Champions" : "LaLiga";
  tft.print(nombre);

  // Botón toggle (derecha) sólo si hay Champions activa o ya estamos en Champions
  if (snap_.hayChampionsDisponible || snap_.competicion == Competicion::CHAMPIONS) {
    tft.drawRect(BTN_X, BTN_Y, BTN_W, BTN_H, paleta_dark::COL_ACENTO);
    tft.setTextFont(2);
    tft.setTextColor(paleta_dark::COL_ACENTO, paleta_dark::COL_FONDO);
    const char* etq = (snap_.competicion == Competicion::CHAMPIONS)
        ? "> LaLiga" : "> Champions";
    int16_t tw = tft.textWidth(etq);
    tft.setCursor(BTN_X + (BTN_W - tw) / 2, BTN_Y + 4);
    tft.print(etq);
  }
}

void PantallaFutbol::dibujarIndicador(TFT_eSPI& tft) {
  // Dos dots debajo del botón (o del título si no hay botón), a la derecha.
  const int y = OFFSET_Y + 30;
  const int r = 3;
  const int xA = W - 22;
  const int xB = W - 10;
  const bool act = (sub_ == SubVista::JORNADA_ACTUAL);
  if (act) {
    tft.fillCircle(xA, y, r, paleta_dark::COL_ACENTO);
    tft.drawCircle(xB, y, r, paleta_dark::COL_TXT_SECUND);
  } else {
    tft.drawCircle(xA, y, r, paleta_dark::COL_TXT_SECUND);
    tft.fillCircle(xB, y, r, paleta_dark::COL_ACENTO);
  }
}

// Sub-vista 1: partidos de la jornada actual con marcador.
void PantallaFutbol::dibujarJornadaActual(TFT_eSPI& tft) {
  pintarFondo(tft);
  dibujarCabecera(tft);
  dibujarIndicador(tft);

  tft.setTextFont(2);
  tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
  tft.setCursor(8, OFFSET_Y + 30);
  char titulo[24];
  std::snprintf(titulo, sizeof(titulo), "Jornada %d", snap_.jornadaActual);
  tft.print(titulo);

  const auto& lst = snap_.partidosJornada;
  if (lst.empty()) {
    tft.setTextFont(2);
    tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
    const char* t = "(sin partidos)";
    int16_t w = tft.textWidth(t);
    tft.setCursor((W - w) / 2, OFFSET_Y + 110);
    tft.print(t);
    return;
  }

  const int n = std::min<int>(5, static_cast<int>(lst.size()));
  int y = OFFSET_Y + 54;
  const int filaH = 30;
  for (int i = 0; i < n; ++i) {
    const auto& p = lst[i];
    dibujarCaja(tft, 6, y, W - 12, filaH - 4, paleta_dark::COL_CAJA);

    // Local
    tft.setTextFont(2);
    tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
    tft.setCursor(12, y + 5);
    tft.print(truncar(p.local, 12).c_str());

    // Marcador central (o "-" si no jugado)
    char marc[16];
    if (p.golesLocal >= 0 && p.golesVisitante >= 0) {
      std::snprintf(marc, sizeof(marc), "%d-%d", p.golesLocal, p.golesVisitante);
    } else {
      std::snprintf(marc, sizeof(marc), "-");
    }
    tft.setTextFont(4);
    uint16_t colM = paleta_dark::COL_ACENTO;
    if (p.golesLocal >= 0 && p.golesLocal > p.golesVisitante) colM = paleta_dark::COL_OK;
    tft.setTextColor(colM, paleta_dark::COL_FONDO);
    int16_t wm = tft.textWidth(marc);
    tft.setCursor((W - wm) / 2, y + 2);
    tft.print(marc);

    // Visitante
    tft.setTextFont(2);
    tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
    std::string sVis = truncar(p.visitante, 12);
    int16_t wv = tft.textWidth(sVis.c_str());
    tft.setCursor(W - 12 - wv, y + 5);
    tft.print(sVis.c_str());

    y += filaH;
  }
}

// Sub-vista 2: próxima jornada con fecha/hora de cada partido.
void PantallaFutbol::dibujarProximaJornada(TFT_eSPI& tft) {
  pintarFondo(tft);
  dibujarCabecera(tft);
  dibujarIndicador(tft);

  tft.setTextFont(2);
  tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
  tft.setCursor(8, OFFSET_Y + 30);
  char titulo[32];
  std::snprintf(titulo, sizeof(titulo), "Proxima J%d", snap_.jornadaSiguiente);
  tft.print(titulo);

  const auto& lst = snap_.partidosSiguiente;
  if (lst.empty()) {
    tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
    const char* t = "(sin partidos)";
    int16_t w = tft.textWidth(t);
    tft.setCursor((W - w) / 2, OFFSET_Y + 110);
    tft.print(t);
    return;
  }

  const int n = std::min<int>(5, static_cast<int>(lst.size()));
  int y = OFFSET_Y + 54;
  const int filaH = 30;
  for (int i = 0; i < n; ++i) {
    const auto& p = lst[i];
    dibujarCaja(tft, 6, y, W - 12, filaH - 4, paleta_dark::COL_CAJA);

    // Hora (últimos 5 chars de fechaHora si viene "Vie 5 sep 19:00")
    std::string hora;
    if (p.fechaHora.size() >= 5) {
      std::string tail = p.fechaHora.substr(p.fechaHora.size() - 5);
      if (tail[2] == ':' && tail[0] >= '0' && tail[0] <= '9') hora = tail;
    }
    tft.setTextFont(2);
    tft.setTextColor(paleta_dark::COL_ACENTO, paleta_dark::COL_FONDO);
    tft.setCursor(10, y + 5);
    tft.print(hora.empty() ? "--:--" : hora.c_str());

    // Local vs visitante (compacto)
    tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
    std::string linea = truncar(p.local, 10) + " vs " + truncar(p.visitante, 10);
    int16_t wt = tft.textWidth(linea.c_str());
    tft.setCursor((W - wt) / 2 + 20, y + 5);
    tft.print(linea.c_str());

    y += filaH;
  }
}
