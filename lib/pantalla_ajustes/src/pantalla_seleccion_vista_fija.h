#pragma once
#include "pantalla.h"
#include "gestor_pantallas.h"
#include "config_store.h"

class PantallaSeleccionVistaFija : public pantallas::Pantalla {
 public:
  PantallaSeleccionVistaFija(pantallas::GestorPantallas& g, Config& cfg)
    : gestor_(g), cfg_(cfg) {}
  const char* nombre() const override { return "Vista fija"; }
  uint8_t id() const override { return 32; }
  void alEntrar() override { dirty_ = true; }
  void alTocar(int x, int y) override;
  void dibujar(uint32_t) override;

 private:
  pantallas::GestorPantallas& gestor_;
  Config& cfg_;
  bool dirty_ = true;
};
