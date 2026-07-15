#pragma once
#include <cstdint>

enum class EstadoLed {
  PORTAL,           // parpadeo 1 Hz
  CONECTANDO_WIFI,  // parpadeo 5 Hz
  RADAR_OK,         // fijo encendido
  RADAR_ERROR       // encendido, se apaga 2 s cada minuto
};

class StatusLed {
 public:
  static void iniciar(int pin = 2);          // GPIO 2 = LED interno de la mayoría de ESP32 dev
  static void setEstado(EstadoLed nuevo);
};
