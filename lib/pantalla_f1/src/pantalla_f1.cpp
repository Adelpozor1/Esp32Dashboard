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

// Color por escudería (aproximados en RGB565)
uint16_t colorEscuderia(const std::string& e) {
  if (e == "Red Bull")                                       return 0x2A5F;  // azul marino
  if (e == "Mercedes")                                        return 0x0575;  // verde/turquesa
  if (e == "Ferrari")                                         return 0xF800;  // rojo
  if (e == "McLaren")                                         return 0xFC00;  // naranja
  if (e == "Aston Martin")                                    return 0x03E0;  // verde oscuro
  if (e == "Alpine" || e == "Alpine F1 Team")                 return 0x03BF;  // azul claro
  if (e == "Williams")                                        return 0x001F;  // azul
  if (e == "Racing Bulls" || e == "RB" || e == "RB F1 Team")  return 0x2A5F;  // azul marino
  if (e == "Kick Sauber" || e == "Sauber")                    return 0x07E0;  // verde
  if (e == "Haas F1 Team" || e == "Haas")                     return 0xFFFF;  // blanco
  return paleta_dark::COL_ACENTO;
}
}  // namespace

std::string PantallaF1::truncar(const std::string& s, size_t n) {
  if (s.size() <= n) return s;
  return s.substr(0, n - 1) + ".";
}

void PantallaF1::alEntrar() { dirty_ = true; ultObtenidoMs_ = 0; }

void PantallaF1::alternarSubVista() {
  sub_ = (sub_ == SubVista::CALENDARIO) ? SubVista::CLASIFICACION
                                         : SubVista::CALENDARIO;
  dirty_ = true;
}

void PantallaF1::alDeslizar(pantallas::Direccion dir) {
  if (dir == pantallas::Direccion::ARRIBA || dir == pantallas::Direccion::ABAJO) {
    alternarSubVista();
  }
}

void PantallaF1::alTocar(int /*x*/, int /*y*/) {
  alternarSubVista();
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
  if (sub_ == SubVista::CALENDARIO) dibujarCalendario(tft);
  else                                dibujarClasificacion(tft);
  dirty_ = false; ultObtenidoMs_ = snap_.obtenido_ms;
}

void PantallaF1::dibujarSinDatos(TFT_eSPI& tft) {
  pintarFondo(tft);
  tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
  tft.setTextFont(4);
  const char* t = (snap_.obtenido_ms == 0) ? "Cargando datos..." : "F1: sin datos";
  int16_t w = tft.textWidth(t);
  tft.setCursor((W - w) / 2, OFFSET_Y + 90);
  tft.print(t);
}

void PantallaF1::dibujarIndicador(TFT_eSPI& tft) {
  const int y = OFFSET_Y + 6;
  const int r = 3;
  const int xA = W - 22, xB = W - 10;
  const bool cal = (sub_ == SubVista::CALENDARIO);
  if (cal) {
    tft.fillCircle(xA, y, r, paleta_dark::COL_ACENTO);
    tft.drawCircle(xB, y, r, paleta_dark::COL_TXT_SECUND);
  } else {
    tft.drawCircle(xA, y, r, paleta_dark::COL_TXT_SECUND);
    tft.fillCircle(xB, y, r, paleta_dark::COL_ACENTO);
  }
}

void PantallaF1::dibujarClasificacion(TFT_eSPI& tft) {
  pintarFondo(tft);
  dibujarIndicador(tft);
  tft.setTextFont(2);
  tft.setTextColor(paleta_dark::COL_ACENTO, paleta_dark::COL_FONDO);
  tft.setCursor(10, OFFSET_Y + 6);
  tft.print("Mundial");

  if (snap_.clasificacion.empty()) {
    tft.setTextFont(2);
    tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
    const char* t = "Clasificacion no disponible";
    int16_t w = tft.textWidth(t);
    tft.setCursor((W - w) / 2, OFFSET_Y + 100);
    tft.print(t);
    return;
  }

  const int n = std::min<int>(4, static_cast<int>(snap_.clasificacion.size()));
  const int filaAlto = 44;
  int y = OFFSET_Y + 32;
  for (int i = 0; i < n; ++i) {
    const auto& p = snap_.clasificacion[i];
    // Chip de posición
    uint16_t col = colorEscuderia(p.equipo);
    tft.fillRect(10, y, 26, 26, col);
    // Contraste: si el fondo es muy claro (Haas blanco), texto negro
    uint16_t colNum = (col == 0xFFFF) ? 0x0000 : 0xFFFF;
    tft.setTextFont(4);
    tft.setTextColor(colNum, col);
    char posStr[4];
    std::snprintf(posStr, sizeof(posStr), "%d", p.posicion);
    int16_t wp = tft.textWidth(posStr);
    tft.setCursor(10 + (26 - wp) / 2, y + 1);
    tft.print(posStr);

    // Nombre en font 4 blanco
    tft.setTextFont(4);
    tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
    tft.setCursor(46, y);
    tft.print(truncar(p.nombre, 12).c_str());

    // Equipo debajo en font 1 gris
    tft.setTextFont(1);
    tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
    tft.setCursor(46, y + 28);
    tft.print(truncar(p.equipo, 24).c_str());

    // Puntos a la derecha en font 4 acento
    char ptsStr[8];
    std::snprintf(ptsStr, sizeof(ptsStr), "%d", p.puntos);
    tft.setTextFont(4);
    tft.setTextColor(paleta_dark::COL_ACENTO, paleta_dark::COL_FONDO);
    int16_t wpts = tft.textWidth(ptsStr);
    tft.setCursor(W - 10 - wpts, y);
    tft.print(ptsStr);
    // "pts" en font 1 debajo
    tft.setTextFont(1);
    tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
    int16_t wLbl = tft.textWidth("pts");
    tft.setCursor(W - 10 - wLbl, y + 28);
    tft.print("pts");

    y += filaAlto;
  }
}

void PantallaF1::dibujarCalendario(TFT_eSPI& tft) {
  pintarFondo(tft);
  dibujarIndicador(tft);
  tft.setTextFont(2);
  tft.setTextColor(paleta_dark::COL_ACENTO, paleta_dark::COL_FONDO);
  tft.setCursor(10, OFFSET_Y + 6);
  tft.print("Calendario");

  const int n = std::min<int>(4, static_cast<int>(snap_.proximas.size()));
  if (n == 0) {
    tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
    tft.setCursor(10, OFFSET_Y + 100);
    tft.print("(sin datos)");
    return;
  }

  const int cajaAlto = 44;
  int y = OFFSET_Y + 32;
  for (int i = 0; i < n; ++i) {
    const auto& c = snap_.proximas[i];
    // Caja
    tft.drawRect(6, y, W - 12, cajaAlto, paleta_dark::COL_CAJA);
    // Nombre GP
    tft.setTextFont(2);
    tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
    tft.setCursor(12, y + 4);
    tft.print(truncar(c.nombreGp, 22).c_str());
    // Circuito
    tft.setTextFont(1);
    tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
    tft.setCursor(12, y + 26);
    tft.print(truncar(c.circuito, 32).c_str());
    // Fecha a la derecha en font 2 acento
    tft.setTextFont(2);
    tft.setTextColor(paleta_dark::COL_ACENTO, paleta_dark::COL_FONDO);
    // Mostrar solo YYYY-MM-DD si viene con hora
    std::string f = c.fechaHora.size() >= 10 ? c.fechaHora.substr(0, 10) : c.fechaHora;
    int16_t wFH = tft.textWidth(f.c_str());
    tft.setCursor(W - 16 - wFH, y + 14);
    tft.print(f.c_str());

    y += cajaAlto + 4;
  }
}
