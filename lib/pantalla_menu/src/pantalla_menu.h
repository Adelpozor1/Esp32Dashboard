#pragma once
#include "pantalla.h"
#include "gestor_pantallas.h"
#include <string>
#include <vector>

class PantallaMenu : public pantallas::Pantalla {
 public:
  struct Entrada { uint8_t id; std::string etiqueta; };

  PantallaMenu(pantallas::GestorPantallas& gestor, pantallas::Pantalla* ajustes);
  void configurarEntradas(const std::vector<Entrada>& entradas);

  const char* nombre() const override { return "Menú"; }
  uint8_t id() const override { return 10; }

  void alEntrar() override { dirty_ = true; }
  void alTocar(int x, int y) override;
  void dibujar(uint32_t msAhora) override;

 private:
  pantallas::GestorPantallas& gestor_;
  pantallas::Pantalla* ajustes_;
  std::vector<Entrada> entradas_;
  bool dirty_ = true;
};
