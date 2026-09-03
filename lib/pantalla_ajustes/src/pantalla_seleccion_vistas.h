#pragma once
#include "pantalla.h"
#include "gestor_pantallas.h"
#include "config_store.h"
#include <vector>

class PantallaSeleccionVistas : public pantallas::Pantalla {
 public:
  PantallaSeleccionVistas(pantallas::GestorPantallas& g, Config& cfg)
    : gestor_(g), cfg_(cfg) {}
  const char* nombre() const override { return "Vistas"; }
  uint8_t id() const override { return 31; }
  void alEntrar() override;
  void alTocar(int x, int y) override;
  void dibujar(uint32_t) override;

 private:
  void guardar();
  int nActivas() const;

  pantallas::GestorPantallas& gestor_;
  Config& cfg_;
  // Estado local: para cada id 0..5, {activa, posición en orden si activa}.
  std::vector<uint8_t> ordenLocal_;   // ids en orden actual, sólo activas
  bool                 activas_[6] = {false, false, false, false, false, false};
  bool dirty_ = true;
};
