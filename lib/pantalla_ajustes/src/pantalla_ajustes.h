#pragma once
#include "pantalla.h"
#include "gestor_pantallas.h"
#include "config_store.h"

class PantallaAjustes : public pantallas::Pantalla {
 public:
  struct SubPantallas {
    pantallas::Pantalla* intervalo;
    pantallas::Pantalla* seleccionVistas;
    pantallas::Pantalla* seleccionVistaFija;
    pantallas::Pantalla* configLocalizacion;
    pantallas::Pantalla* calibrarTouch;
    pantallas::Pantalla* confirmarReset;
  };

  PantallaAjustes(pantallas::GestorPantallas& g, Config& cfg, SubPantallas subs);

  const char* nombre() const override { return "Ajustes"; }
  uint8_t id() const override { return 20; }

  void alEntrar() override { dirty_ = true; }
  void alTocar(int x, int y) override;
  void dibujar(uint32_t) override;

 private:
  pantallas::GestorPantallas& gestor_;
  Config& cfg_;
  SubPantallas subs_;
  bool dirty_ = true;
};
