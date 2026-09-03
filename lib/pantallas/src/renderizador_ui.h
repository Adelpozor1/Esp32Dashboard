#pragma once
#include <cstdint>

namespace pantallas {

class IRenderizadorUi {
 public:
  virtual ~IRenderizadorUi() = default;

  // Pinta la barra superior de 20 px con botón "☰" a la izquierda, título centrado
  // y (opcional) dots del carrusel abajo-derecha si nDots > 0.
  virtual void pintarBarraSuperior(const char* titulo, uint8_t dotActual, uint8_t nDots) = 0;

  // Limpia el área de contenido (320×220) a negro.
  virtual void limpiarAreaContenido() = 0;
};

}  // namespace pantallas
