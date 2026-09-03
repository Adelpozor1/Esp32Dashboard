#pragma once
#include "pantalla.h"
#include "gestor_pantallas.h"
#include "config_store.h"

class PantallaCalibrarTouch : public pantallas::Pantalla {
 public:
  PantallaCalibrarTouch(pantallas::GestorPantallas& g, Config& cfg)
    : gestor_(g), cfg_(cfg) {}
  const char* nombre() const override { return "Calibrar"; }
  uint8_t id() const override { return 35; }
  void alEntrar() override;
  void dibujar(uint32_t) override;

 private:
  pantallas::GestorPantallas& gestor_;
  Config& cfg_;
  int paso_ = 0;  // 0..3 esquinas, 4 = terminado
  int16_t rawX_[4] = {0, 0, 0, 0};
  int16_t rawY_[4] = {0, 0, 0, 0};
  bool dirty_ = true;
};
