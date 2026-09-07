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

void PantallaMotogp::alternarSubVista() {
  sub_ = (sub_ == SubVista::CALENDARIO) ? SubVista::CLASIFICACION
                                         : SubVista::CALENDARIO;
  dirty_ = true;
}

void PantallaMotogp::alDeslizar(pantallas::Direccion dir) {
  if (dir == pantallas::Direccion::ARRIBA || dir == pantallas::Direccion::ABAJO) {
    alternarSubVista();
  }
}

void PantallaMotogp::alTocar(int /*x*/, int /*y*/) {
  alternarSubVista();
}

void PantallaMotogp::dibujar(uint32_t) {
  auto& tft = tft_driver::obtenerTft();
  const bool datosNuevos = snap_.obtenido_ms != ultObtenidoMs_;
  if (!dirty_ && !datosNuevos) return;

  if (!snap_.ok) {
    dibujarSinDatos(tft);
    dirty_ = false; ultObtenidoMs_ = snap_.obtenido_ms; return;
  }
  if (sub_ == SubVista::CALENDARIO) dibujarCalendario(tft);
  else                                dibujarClasificacion(tft);
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

  // Mismo layout que F1: cajas con nombre GP + fecha ISO a la derecha.
  const int cajaAlto = 44;
  int y = OFFSET_Y + 32;
  for (int i = 0; i < n; ++i) {
    const auto& e = snap_.proximos[i];
    tft.drawRect(6, y, W - 12, cajaAlto, paleta_dark::COL_CAJA);
    // Nombre GP
    tft.setTextFont(2);
    tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
    tft.setCursor(12, y + 4);
    tft.print(truncar(e.nombre, 22).c_str());
    // Fecha ISO a la derecha en font 2 acento (los primeros 10 chars = YYYY-MM-DD).
    std::string f = e.fechaHora.size() >= 10 ? e.fechaHora.substr(0, 10)
                                              : e.fechaHora;
    tft.setTextColor(paleta_dark::COL_ACENTO, paleta_dark::COL_FONDO);
    int16_t wFH = tft.textWidth(f.c_str());
    tft.setCursor(W - 16 - wFH, y + 14);
    tft.print(f.c_str());
    y += cajaAlto + 4;
  }
}

// Devuelve el color RGB565 aproximado por marca de fabricante.
static uint16_t colorMarcaMoto(const std::string& m) {
  if (m == "Ducati")   return 0xF800;  // rojo
  if (m == "Aprilia")  return 0x001F;  // azul
  if (m == "KTM")      return 0xFC00;  // naranja
  if (m == "Yamaha")   return 0x0417;  // azul cyan
  if (m == "Honda")    return 0xF9E7;  // rojo/blanco
  return paleta_dark::COL_ACENTO;
}

// Sub-vista 2: Clasificación del mundial (top 4 pilotos, mismo estilo que F1)
void PantallaMotogp::dibujarClasificacion(TFT_eSPI& tft) {
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
    // Chip color marca
    uint16_t col = colorMarcaMoto(p.marca);
    tft.fillRect(10, y, 26, 26, col);
    tft.setTextFont(4);
    tft.setTextColor(0xFFFF, col);
    char posStr[4];
    std::snprintf(posStr, sizeof(posStr), "%d", p.posicion);
    int16_t wp = tft.textWidth(posStr);
    tft.setCursor(10 + (26 - wp) / 2, y + 1);
    tft.print(posStr);

    // Nombre
    tft.setTextFont(4);
    tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
    tft.setCursor(46, y);
    tft.print(truncar(p.nombre, 12).c_str());
    // Equipo debajo
    tft.setTextFont(1);
    tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
    tft.setCursor(46, y + 28);
    tft.print(truncar(p.equipo, 24).c_str());
    // Puntos a la derecha
    char ptsStr[8];
    std::snprintf(ptsStr, sizeof(ptsStr), "%d", p.puntos);
    tft.setTextFont(4);
    tft.setTextColor(paleta_dark::COL_ACENTO, paleta_dark::COL_FONDO);
    int16_t wpts = tft.textWidth(ptsStr);
    tft.setCursor(W - 10 - wpts, y);
    tft.print(ptsStr);
    tft.setTextFont(1);
    tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
    int16_t wLbl = tft.textWidth("pts");
    tft.setCursor(W - 10 - wLbl, y + 28);
    tft.print("pts");
    y += filaAlto;
  }
}
