#include "pantalla_ajustes.h"
#include "tft_driver.h"
#include <TFT_eSPI.h>
#include <cstdio>

namespace {
constexpr int OFFSET_Y  = 20;
constexpr int ALTO_FILA = 26;
constexpr uint16_t COL_FONDO = 0x0000;
constexpr uint16_t COL_TXT   = 0x07E0;
constexpr uint16_t COL_LINEA = 0x03E0;

const char* labelVista(uint8_t id) {
  switch (id) {
    case 0: return "Radar";
    case 1: return "Reloj";
    case 2: return "Meteo";
    case 3: return "Fútbol";
    case 4: return "MotoGP";
    case 5: return "F1";
    default: return "?";
  }
}
}  // namespace

PantallaAjustes::PantallaAjustes(pantallas::GestorPantallas& g, Config& c, SubPantallas s)
  : gestor_(g), cfg_(c), subs_(s) {}

void PantallaAjustes::alTocar(int x, int y) {
  const int fila = y / ALTO_FILA;
  // Índice dinámico según modo:
  // 0: Modo (siempre)
  // 1: (CARRUSEL) Intervalo | (FIJO) Vista fija
  // 2: (CARRUSEL) Vistas activas
  // 3: Reconfigurar localización
  // 4: Calibrar táctil
  // 5: Reset total
  // 6: Volver
  const bool carrusel = (cfg_.modo == ModoVista::CARRUSEL);
  const int idxVistasActivas = carrusel ? 2 : -1;
  const int idxVistaFija     = carrusel ? -1 : 1;
  const int idxIntervalo     = carrusel ? 1 : -1;
  const int idxLoc           = 3;
  const int idxCal           = 4;
  const int idxReset         = 5;
  const int idxVolver        = 6;

  if (fila == 0) {
    cfg_.modo = (cfg_.modo == ModoVista::CARRUSEL) ? ModoVista::FIJO : ModoVista::CARRUSEL;
    ConfigStore::guardar(cfg_);
    dirty_ = true;
  } else if (fila == idxIntervalo && subs_.intervalo) {
    gestor_.abrirEnPila(subs_.intervalo);
  } else if (fila == idxVistasActivas && subs_.seleccionVistas) {
    gestor_.abrirEnPila(subs_.seleccionVistas);
  } else if (fila == idxVistaFija && subs_.seleccionVistaFija) {
    gestor_.abrirEnPila(subs_.seleccionVistaFija);
  } else if (fila == idxLoc && subs_.configLocalizacion) {
    gestor_.abrirEnPila(subs_.configLocalizacion);
  } else if (fila == idxCal && subs_.calibrarTouch) {
    gestor_.abrirEnPila(subs_.calibrarTouch);
  } else if (fila == idxReset && subs_.confirmarReset) {
    gestor_.abrirEnPila(subs_.confirmarReset);
  } else if (fila == idxVolver) {
    gestor_.volverAtras();
  }
}

void PantallaAjustes::dibujar(uint32_t) {
  if (!dirty_) return;
  auto& tft = tft_driver::obtenerTft();
  tft.fillRect(0, OFFSET_Y, 320, 220, COL_FONDO);
  tft.setTextFont(2);
  tft.setTextColor(COL_TXT, COL_FONDO);
  char buf[48];
  int y = OFFSET_Y + 4;
  auto fila = [&](const char* txt) {
    tft.setCursor(10, y);
    tft.print(txt);
    tft.drawFastHLine(0, y + ALTO_FILA - 4, 320, COL_LINEA);
    y += ALTO_FILA;
  };
  std::snprintf(buf, sizeof(buf), "Modo: %s",
                cfg_.modo == ModoVista::CARRUSEL ? "Carrusel" : "Fijo");
  fila(buf);
  if (cfg_.modo == ModoVista::CARRUSEL) {
    std::snprintf(buf, sizeof(buf), "Intervalo: %u s", (unsigned)cfg_.intervalo_carrusel_s);
    fila(buf);
    fila("Vistas activas y orden");
  } else {
    std::snprintf(buf, sizeof(buf), "Vista fija: %s", labelVista(cfg_.vista_fija));
    fila(buf);
    fila(" ");  // ocupa el hueco de la fila 2 en modo fijo
  }
  fila("Cambiar WiFi / ubicacion");
  fila("Calibrar tactil");
  fila("Reset total");
  fila("< Volver");
  dirty_ = false;
}
