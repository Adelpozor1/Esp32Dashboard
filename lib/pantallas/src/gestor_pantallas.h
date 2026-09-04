#pragma once
#include "pantalla.h"
#include "renderizador_ui.h"
#include <cstdint>
#include <vector>

namespace pantallas {

enum class ModoGestor : uint8_t { FIJO = 0, CARRUSEL = 1 };

enum class TipoEventoUi : uint8_t {
  TAP             = 0,
  SWIPE_IZQUIERDA = 1,
  SWIPE_DERECHA   = 2,
  SWIPE_ARRIBA    = 3,
  SWIPE_ABAJO     = 4,
};

struct EventoUi {
  TipoEventoUi tipo;
  int16_t x;
  int16_t y;
};

// Zona tocable del botón "☰" (en coordenadas de pantalla completa).
constexpr int ZONA_MENU_X_MAX = 40;
constexpr int ZONA_MENU_Y_MAX = 20;

class GestorPantallas {
 public:
  explicit GestorPantallas(IRenderizadorUi& renderer);

  void registrar(Pantalla* p);
  void setHome(Pantalla* home);
  void configurarModo(ModoGestor modo, uint16_t intervaloS,
                      const std::vector<uint8_t>& vistasOrden, uint8_t idFija);
  void iniciar(uint32_t msAhora);

  // Motor: llamar cada frame desde la task de display.
  void tick(uint32_t msAhora);

  // Entrada desde touch (ya destinada al gestor; la task Arduino la reencola aquí).
  void encolarEvento(const EventoUi& ev);

  // Navegación programática (usada desde el menú y sub-pantallas).
  void mostrarPorId(uint8_t id);
  void abrirEnPila(Pantalla* p);
  void volverAtras();

 private:
  void aplicarPantalla(Pantalla* p, uint32_t msAhora);
  Pantalla* pantallaPorId(uint8_t id);

  IRenderizadorUi& renderer_;
  std::vector<Pantalla*> registradas_;
  Pantalla* home_ = nullptr;
  Pantalla* actual_ = nullptr;
  std::vector<Pantalla*> pilaUi_;
  ModoGestor modo_ = ModoGestor::CARRUSEL;
  uint16_t intervaloS_ = 10;
  std::vector<uint8_t> orden_;
  uint8_t idFija_ = 0;
  size_t   indiceCarrusel_ = 0;
  uint32_t ultimoCambioMs_ = 0;
  uint32_t ultimoTickMs_ = 0;
  std::vector<EventoUi> cola_;
};

}  // namespace pantallas
