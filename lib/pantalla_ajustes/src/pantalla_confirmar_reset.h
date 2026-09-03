#pragma once
#include "pantalla.h"
#include "gestor_pantallas.h"

class PantallaConfirmarReset : public pantallas::Pantalla {
 public:
  explicit PantallaConfirmarReset(pantallas::GestorPantallas& g) : gestor_(g) {}
  const char* nombre() const override { return "Reset"; }
  uint8_t id() const override { return 34; }
  void alEntrar() override { dirty_ = true; }
  void alTocar(int x, int y) override;
  void dibujar(uint32_t) override;

 private:
  pantallas::GestorPantallas& gestor_;
  bool dirty_ = true;
};
