#include "pantalla_motogp.h"
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

// Devuelve la parte "Vie 5 sep" quitando la hora si es 00:00 placeholder.
// La cadena entra como "Vie 5 sep 00:00" o "Vie 5 sep 14:00" o "Vie 5 sep".
std::string fechaSinHoraPlaceholder(const std::string& s) {
  if (s.size() >= 5) {
    // Si termina con " 00:00", quitar la hora
    if (s.size() >= 6 && s.compare(s.size() - 6, 6, " 00:00") == 0) {
      return s.substr(0, s.size() - 6);
    }
  }
  return s;
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
  const char* t = (snap_.obtenido_ms == 0) ? "Cargando datos..." : "MotoGP: sin datos";
  int16_t w = tft.textWidth(t);
  tft.setCursor((W - w) / 2, OFFSET_Y + 90);
  tft.print(t);
}

void PantallaMotogp::dibujarIndicador(TFT_eSPI& tft) {
  const int y = OFFSET_Y + 6;
  const int r = 3;
  const int xA = W - 22, xB = W - 10;
  // Sub-vista 1 (default) = CALENDARIO (Próximos GPs)
  // Sub-vista 2           = ULTIMOS
  const bool calendario = (sub_ == SubVista::CALENDARIO);
  if (calendario) {
    tft.fillCircle(xA, y, r, paleta_dark::COL_ACENTO);
    tft.drawCircle(xB, y, r, paleta_dark::COL_TXT_SECUND);
  } else {
    tft.drawCircle(xA, y, r, paleta_dark::COL_TXT_SECUND);
    tft.fillCircle(xB, y, r, paleta_dark::COL_ACENTO);
  }
}

// Sub-vista 1: Próximos GPs (estética con cajas, fecha grande y separadores)
void PantallaMotogp::dibujarCalendario(TFT_eSPI& tft) {
  pintarFondo(tft);
  dibujarIndicador(tft);
  tft.setTextFont(2);
  tft.setTextColor(paleta_dark::COL_ACENTO, paleta_dark::COL_FONDO);
  tft.setCursor(10, OFFSET_Y + 6);
  tft.print("Proximos GPs");

  const int n = std::min<int>(4, static_cast<int>(snap_.proximos.size()));
  if (n == 0) {
    tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
    tft.setTextFont(2);
    const char* t = "(sin datos)";
    int16_t w = tft.textWidth(t);
    tft.setCursor((W - w) / 2, OFFSET_Y + 100);
    tft.print(t);
    return;
  }

  const int filaAlto = 44;
  int y = OFFSET_Y + 32;
  for (int i = 0; i < n; ++i) {
    const auto& e = snap_.proximos[i];
    // Fecha (grande, izquierda)
    std::string fecha = fechaSinHoraPlaceholder(e.fechaHora);
    tft.setTextFont(4);
    tft.setTextColor(paleta_dark::COL_ACENTO, paleta_dark::COL_FONDO);
    tft.setCursor(10, y + 4);
    tft.print(truncar(fecha, 12).c_str());
    // Nombre GP (derecha, título)
    tft.setTextFont(2);
    tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
    std::string nombre = truncar(e.nombre, 16);
    int16_t wN = tft.textWidth(nombre.c_str());
    tft.setCursor(W - 10 - wN, y + 12);
    tft.print(nombre.c_str());
    // Separador
    tft.drawFastHLine(0, y + filaAlto, W, paleta_dark::COL_CAJA);
    y += filaAlto;
  }
}

// Sub-vista 2: Últimos ganadores (chip amarillo con "1", nombre GP, ganador)
void PantallaMotogp::dibujarUltimos(TFT_eSPI& tft) {
  pintarFondo(tft);
  dibujarIndicador(tft);
  tft.setTextFont(2);
  tft.setTextColor(paleta_dark::COL_ACENTO, paleta_dark::COL_FONDO);
  tft.setCursor(10, OFFSET_Y + 6);
  tft.print("Ultimas carreras");

  const int n = std::min<int>(3, static_cast<int>(snap_.ultimos.size()));
  if (n == 0) {
    tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
    tft.setTextFont(2);
    const char* t = "(sin datos)";
    int16_t w = tft.textWidth(t);
    tft.setCursor((W - w) / 2, OFFSET_Y + 100);
    tft.print(t);
    return;
  }

  const int filaAlto = 60;
  int y = OFFSET_Y + 32;
  for (int i = 0; i < n; ++i) {
    const auto& e = snap_.ultimos[i];
    // Chip amarillo con "1"
    const uint16_t COL_ORO = 0xFEA0;  // amarillo
    tft.fillRect(10, y, 26, 26, COL_ORO);
    tft.setTextFont(4);
    tft.setTextColor(0x0000, COL_ORO);
    const char* posStr = "1";
    int16_t wp = tft.textWidth(posStr);
    tft.setCursor(10 + (26 - wp) / 2, y + 1);
    tft.print(posStr);

    // Nombre GP arriba
    tft.setTextFont(2);
    tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
    tft.setCursor(46, y + 2);
    tft.print(truncar(e.nombre, 22).c_str());
    // Ganador en font 4 abajo
    tft.setTextFont(4);
    tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
    tft.setCursor(46, y + 22);
    tft.print(e.ganador.empty() ? "-" : truncar(e.ganador, 14).c_str());

    // Separador
    tft.drawFastHLine(0, y + filaAlto - 2, W, paleta_dark::COL_CAJA);
    y += filaAlto;
  }
}
