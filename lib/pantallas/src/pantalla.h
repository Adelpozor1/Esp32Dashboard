#pragma once
#include <cstdint>

namespace pantallas {

class Pantalla {
 public:
  virtual ~Pantalla() = default;
  virtual const char* nombre() const = 0;
  virtual uint8_t id() const = 0;

  virtual void alEntrar() {}
  virtual void alSalir() {}

  // Coordenadas relativas al área de contenido (0..319, 0..219). Origen arriba-izq.
  virtual void alTocar(int x, int y) {}

  // direccion: -1 swipe a izquierda (siguiente), +1 swipe a derecha (anterior).
  virtual void alDeslizar(int direccion) {}

  // msAhora: reloj monótono en ms. La pantalla decide si repinta o no.
  virtual void dibujar(uint32_t msAhora) = 0;
};

}  // namespace pantallas
