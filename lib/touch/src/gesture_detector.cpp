#include "gesture_detector.h"
#include <cstdlib>

namespace touch {

void GestureDetector::onPress(int16_t x, int16_t y, uint32_t ms) {
  x0_ = x; y0_ = y; t0_ = ms; activo_ = true;
}

std::optional<EventoTactil> GestureDetector::onRelease(int16_t x, int16_t y, uint32_t ms) {
  if (!activo_) return std::nullopt;
  activo_ = false;
  const int32_t dx = static_cast<int32_t>(x) - x0_;
  const int32_t dy = static_cast<int32_t>(y) - y0_;
  const uint32_t dur = (ms >= t0_) ? (ms - t0_) : 0;

  if (dur < UMBRAL_TAP_MS && std::abs(dx) < UMBRAL_TAP_PX && std::abs(dy) < UMBRAL_TAP_PX) {
    return EventoTactil{TipoEvento::TAP, x0_, y0_};
  }
  if (dur < UMBRAL_SWIPE_MS) {
    const bool horiz = std::abs(dx) > std::abs(dy);
    if (horiz && std::abs(dx) > UMBRAL_SWIPE_PX) {
      return EventoTactil{dx > 0 ? TipoEvento::SWIPE_DERECHA : TipoEvento::SWIPE_IZQUIERDA,
                          x0_, y0_};
    }
    if (!horiz && std::abs(dy) > UMBRAL_SWIPE_PX) {
      return EventoTactil{dy > 0 ? TipoEvento::SWIPE_ABAJO : TipoEvento::SWIPE_ARRIBA,
                          x0_, y0_};
    }
  }
  return std::nullopt;
}

}  // namespace touch
