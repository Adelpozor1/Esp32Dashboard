#include "status_led.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

int             s_pin = 2;
EstadoLed       s_estado = EstadoLed::PORTAL;
TaskHandle_t    s_task = nullptr;

void tareaLed(void*) {
  bool encendido = false;
  uint32_t contadorMs = 0;
  for (;;) {
    switch (s_estado) {
      case EstadoLed::PORTAL:
        encendido = !encendido;
        digitalWrite(s_pin, encendido ? HIGH : LOW);
        vTaskDelay(pdMS_TO_TICKS(500));  // 1 Hz
        break;
      case EstadoLed::CONECTANDO_WIFI:
        encendido = !encendido;
        digitalWrite(s_pin, encendido ? HIGH : LOW);
        vTaskDelay(pdMS_TO_TICKS(100));  // 5 Hz
        break;
      case EstadoLed::RADAR_OK:
        digitalWrite(s_pin, HIGH);
        vTaskDelay(pdMS_TO_TICKS(200));
        break;
      case EstadoLed::RADAR_ERROR:
        // encendido, con corte de 2 s cada 60 s
        if (contadorMs >= 60000) {
          digitalWrite(s_pin, LOW);
          vTaskDelay(pdMS_TO_TICKS(2000));
          contadorMs = 0;
        } else {
          digitalWrite(s_pin, HIGH);
          vTaskDelay(pdMS_TO_TICKS(200));
          contadorMs += 200;
        }
        break;
    }
  }
}

}  // namespace

void StatusLed::iniciar(int pin) {
  s_pin = pin;
  pinMode(s_pin, OUTPUT);
  digitalWrite(s_pin, LOW);
  if (!s_task) {
    xTaskCreatePinnedToCore(tareaLed, "led", 2048, nullptr, 1, &s_task, 1);
  }
}

void StatusLed::setEstado(EstadoLed nuevo) {
  s_estado = nuevo;
}
