#pragma once
#include "gesture_detector.h"
#include <cstdint>

namespace touch {

struct CalibracionTouch {
  int16_t min_x = 300;
  int16_t max_x = 3800;
  int16_t min_y = 300;
  int16_t max_y = 3800;
  bool    valida = false;
};

// Arranca la task de poll del XPT2046 en core 0. Convierte lecturas crudas
// a coordenadas de pantalla (320×240 en rotación landscape) con la calibración
// dada, y publica eventos gesto en una cola FreeRTOS interna.
void iniciar(const CalibracionTouch& cal);

// Actualiza la calibración usada por la task (sin necesidad de reiniciar).
void setCalibracion(const CalibracionTouch& cal);

// Espera hasta `timeoutMs` un evento gesto. Devuelve true si sacó uno; false por timeout.
bool esperarEvento(EventoTactil& out, uint32_t timeoutMs);

// Lectura cruda del panel (para el flujo de calibración). Bloqueante hasta
// detectar un press y su release. Devuelve el promedio del press.
bool leerCrudoBloqueante(int16_t& xRawOut, int16_t& yRawOut, uint32_t timeoutMs);

}  // namespace touch
