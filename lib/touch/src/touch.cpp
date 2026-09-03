#include "touch.h"

// El env native (tests desktop) define UNIT_TEST y no dispone de Arduino ni de
// XPT2046_Touchscreen; el LDF de PlatformIO compila todos los .cpp de la lib
// cuando cualquier header suyo es referenciado (test_gesture_detector incluye
// gesture_detector.h), así que hay que apagar este .cpp fuera del firmware.
#ifndef UNIT_TEST

#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

namespace touch {

namespace {

SPIClass* s_spi = nullptr;
XPT2046_Touchscreen* s_ts = nullptr;
CalibracionTouch s_cal;
GestureDetector s_det;
QueueHandle_t s_cola = nullptr;

int16_t mapearX(int16_t xRaw) {
  if (!s_cal.valida || s_cal.max_x <= s_cal.min_x) return 0;
  int32_t v = ((int32_t)(xRaw - s_cal.min_x) * 320) / (s_cal.max_x - s_cal.min_x);
  if (v < 0) v = 0; if (v > 319) v = 319;
  return static_cast<int16_t>(v);
}
int16_t mapearY(int16_t yRaw) {
  if (!s_cal.valida || s_cal.max_y <= s_cal.min_y) return 0;
  int32_t v = ((int32_t)(yRaw - s_cal.min_y) * 240) / (s_cal.max_y - s_cal.min_y);
  if (v < 0) v = 0; if (v > 239) v = 239;
  return static_cast<int16_t>(v);
}

void tareaTouch(void*) {
  bool estabaTocado = false;
  int16_t xr = 0, yr = 0;
  for (;;) {
    bool tocado = s_ts->touched();
    if (tocado) {
      TS_Point p = s_ts->getPoint();
      xr = p.x; yr = p.y;
      if (!estabaTocado) {
        s_det.onPress(mapearX(xr), mapearY(yr), millis());
        estabaTocado = true;
      }
    } else if (estabaTocado) {
      auto ev = s_det.onRelease(mapearX(xr), mapearY(yr), millis());
      if (ev.has_value() && s_cola != nullptr) {
        EventoTactil e = ev.value();
        xQueueSend(s_cola, &e, 0);
      }
      estabaTocado = false;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

}  // namespace

void iniciar(const CalibracionTouch& cal) {
  s_cal = cal;
  if (s_spi == nullptr) {
    s_spi = new SPIClass(HSPI);
    s_spi->begin(XPT_CLK, XPT_MISO, XPT_MOSI, XPT_CS);
  }
  if (s_ts == nullptr) {
    s_ts = new XPT2046_Touchscreen(XPT_CS, XPT_IRQ);
    s_ts->begin(*s_spi);
    s_ts->setRotation(1);
  }
  if (s_cola == nullptr) {
    s_cola = xQueueCreate(16, sizeof(EventoTactil));
  }
  xTaskCreatePinnedToCore(tareaTouch, "touch", 4096, nullptr, 1, nullptr, 0);
  Serial.println("[touch] task de poll arrancada");
}

void setCalibracion(const CalibracionTouch& cal) { s_cal = cal; }

bool esperarEvento(EventoTactil& out, uint32_t timeoutMs) {
  if (!s_cola) return false;
  return xQueueReceive(s_cola, &out, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

bool leerCrudoBloqueante(int16_t& xRawOut, int16_t& yRawOut, uint32_t timeoutMs) {
  const uint32_t inicio = millis();
  // Espera press
  while (!s_ts->touched()) {
    if (millis() - inicio > timeoutMs) return false;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  // Promedia 8 muestras mientras está tocando
  int32_t sx = 0, sy = 0;
  int n = 0;
  while (s_ts->touched() && n < 8) {
    TS_Point p = s_ts->getPoint();
    sx += p.x; sy += p.y;
    ++n;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  if (n == 0) return false;
  xRawOut = static_cast<int16_t>(sx / n);
  yRawOut = static_cast<int16_t>(sy / n);
  // Espera release
  while (s_ts->touched()) vTaskDelay(pdMS_TO_TICKS(20));
  return true;
}

}  // namespace touch

#endif  // !UNIT_TEST
