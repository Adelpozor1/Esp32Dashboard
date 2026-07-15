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
  // pin: GPIO donde vive el LED indicador.
  // activoBajo: true si el LED se enciende con LOW (típico de LEDs integrados con
  // ánodo común, como el LED RGB del ESP32-2432S028 CYD).
  static void iniciar(int pin = 2, bool activoBajo = false);
  static void setEstado(EstadoLed nuevo);
};
