#pragma once
#include <cstdint>

namespace pantallas {

enum class Direccion : uint8_t {
  IZQUIERDA = 0,
  DERECHA   = 1,
  ARRIBA    = 2,
  ABAJO     = 3,
};

class Pantalla {
 public:
  virtual ~Pantalla() = default;
  // El puntero devuelto debe permanecer válido durante toda la vida de la
  // pantalla (típicamente un string literal estático o un miembro de la clase).
  virtual const char* nombre() const = 0;
  // Debe ser único entre las pantallas registradas en un GestorPantallas.
  // Ids estables definidos en el spec (IdVista): 0=RADAR, 1=RELOJ, 2=METEO,
  // 3=FUTBOL, 4=MOTOGP, 5=F1. Las pantallas de UI (menú, ajustes, sub-menús)
  // usan ids >= 10 para no colisionar con las vistas persistidas en NVS.
  virtual uint8_t id() const = 0;

  virtual void alEntrar() {}
  virtual void alSalir() {}

  // Coordenadas relativas al área de contenido (0..319, 0..219). Origen arriba-izq.
  virtual void alTocar(int /*x*/, int /*y*/) {}

  // direccion: sentido del swipe interpretado por el gestor táctil.
  virtual void alDeslizar(Direccion /*direccion*/) {}

  // msAhora: reloj monótono en ms. La pantalla decide si repinta o no.
  virtual void dibujar(uint32_t msAhora) = 0;
};

}  // namespace pantallas
