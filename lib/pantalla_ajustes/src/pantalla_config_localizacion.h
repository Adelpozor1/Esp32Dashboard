#pragma once
#include "pantalla.h"
#include "gestor_pantallas.h"
#include "config_store.h"

class PantallaConfigLocalizacion : public pantallas::Pantalla {
 public:
  PantallaConfigLocalizacion(pantallas::GestorPantallas& g, const Config& cfg)
      : gestor_(g), cfg_(cfg) {}
  const char* nombre() const override { return "Localizacion"; }
  uint8_t id() const override { return 33; }
  void alEntrar() override { dirty_ = true; }
  void alTocar(int x, int y) override;
  void dibujar(uint32_t) override;

 private:
  pantallas::GestorPantallas& gestor_;
  const Config&                cfg_;
  bool dirty_ = true;
};
