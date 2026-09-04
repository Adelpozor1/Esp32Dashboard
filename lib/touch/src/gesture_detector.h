#pragma once
#include <cstdint>
#include <optional>

namespace touch {

enum class TipoEvento : uint8_t {
  TAP             = 0,
  SWIPE_IZQUIERDA = 1,
  SWIPE_DERECHA   = 2,
  SWIPE_ARRIBA    = 3,
  SWIPE_ABAJO     = 4,
};

struct EventoTactil {
  TipoEvento tipo;
  int16_t x;
  int16_t y;
};

// Detector puro y sin dependencias de Arduino. Recibe eventos crudos press/release
// con coordenadas en el espacio de pantalla (ya calibradas) y timestamps en ms.
// Devuelve un evento gesto si el release cierra un patrón reconocido.
//
// Umbrales (comparaciones estrictas):
//   TAP:            duración < 300 ms, |dx| < 20 px, |dy| < 20 px.
//   SWIPE horiz:    duración < 400 ms, |dx| > 60 px, |dx| > |dy|.
//   SWIPE vertical: duración < 400 ms, |dy| > 60 px, |dy| >= |dx|.
//
// Nota: el intervalo |dx| (o |dy|) ∈ [20, 60] con el otro eje pequeño es un
// dead zone intencional (ni TAP ni SWIPE). Los tests fijan estos bordes;
// no cambiar `<`↔`<=` sin actualizar tests.
class GestureDetector {
 public:
  void onPress(int16_t x, int16_t y, uint32_t ms);
  std::optional<EventoTactil> onRelease(int16_t x, int16_t y, uint32_t ms);

  static constexpr uint32_t UMBRAL_TAP_MS   = 300;
  static constexpr int16_t  UMBRAL_TAP_PX   = 20;
  static constexpr uint32_t UMBRAL_SWIPE_MS = 400;
  static constexpr int16_t  UMBRAL_SWIPE_PX = 60;

 private:
  int16_t  x0_ = 0;
  int16_t  y0_ = 0;
  uint32_t t0_ = 0;
  bool     activo_ = false;
};

}  // namespace touch
