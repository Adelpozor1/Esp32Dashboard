#pragma once
#include "pantalla.h"
#include <cstdint>

class PantallaReloj : public pantallas::Pantalla {
 public:
  PantallaReloj() = default;

  const char* nombre() const override { return "Reloj"; }
  uint8_t id() const override { return 1; }

  void alEntrar() override;
  void dibujar(uint32_t msAhora) override;

 private:
  int  ultSeg_ = -1;
  int  ultMin_ = -1;
  int  ultDia_ = -1;
  bool dirty_  = true;   // fuerza repintado completo (fondo + todo)
  bool ultSinc_ = false;
};
