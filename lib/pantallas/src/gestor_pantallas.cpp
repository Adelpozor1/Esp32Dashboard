#include "gestor_pantallas.h"

namespace pantallas {

GestorPantallas::GestorPantallas(IRenderizadorUi& r) : renderer_(r) {}

void GestorPantallas::registrar(Pantalla* p) { registradas_.push_back(p); }
void GestorPantallas::setHome(Pantalla* h) { home_ = h; }

void GestorPantallas::configurarModo(ModoGestor m, uint16_t iv,
                                     const std::vector<uint8_t>& orden, uint8_t idF) {
  modo_ = m;
  intervaloS_ = iv;
  orden_ = orden;
  idFija_ = idF;
}

Pantalla* GestorPantallas::pantallaPorId(uint8_t id) {
  for (auto* p : registradas_) if (p->id() == id) return p;
  return nullptr;
}

void GestorPantallas::aplicarPantalla(Pantalla* p, uint32_t msAhora) {
  if (actual_ == p) return;
  if (actual_) actual_->alSalir();
  actual_ = p;
  ultimoCambioMs_ = msAhora;
  renderer_.limpiarAreaContenido();
  if (actual_) actual_->alEntrar();
}

void GestorPantallas::iniciar(uint32_t msAhora) {
  ultimoTickMs_ = msAhora;
  Pantalla* inicial = nullptr;
  if (modo_ == ModoGestor::FIJO) {
    inicial = pantallaPorId(idFija_);
  } else if (!orden_.empty()) {
    indiceCarrusel_ = 0;
    inicial = pantallaPorId(orden_[0]);
  }
  if (!inicial && home_) inicial = home_;
  aplicarPantalla(inicial, msAhora);
}

void GestorPantallas::encolarEvento(const EventoUi& ev) { cola_.push_back(ev); }

void GestorPantallas::mostrarPorId(uint8_t id) {
  Pantalla* p = pantallaPorId(id);
  if (!p) return;
  pilaUi_.clear();
  aplicarPantalla(p, ultimoTickMs_);
  // Ubicar el índice en el carrusel si corresponde.
  for (size_t i = 0; i < orden_.size(); ++i) {
    if (orden_[i] == id) { indiceCarrusel_ = i; break; }
  }
}

void GestorPantallas::abrirEnPila(Pantalla* p) {
  if (!p) return;
  if (actual_) pilaUi_.push_back(actual_);
  aplicarPantalla(p, ultimoTickMs_);
}

void GestorPantallas::volverAtras() {
  if (pilaUi_.empty()) {
    if (home_) aplicarPantalla(home_, ultimoTickMs_);
    return;
  }
  Pantalla* prev = pilaUi_.back();
  pilaUi_.pop_back();
  aplicarPantalla(prev, ultimoTickMs_);
}

void GestorPantallas::tick(uint32_t msAhora) {
  ultimoTickMs_ = msAhora;
  // 1. Procesar eventos táctiles.
  for (const auto& ev : cola_) {
    switch (ev.tipo) {
      case TipoEventoUi::TAP:
        if (ev.x < ZONA_MENU_X_MAX && ev.y < ZONA_MENU_Y_MAX && home_) {
          // Volver al home vaciando la pila.
          pilaUi_.clear();
          aplicarPantalla(home_, msAhora);
        } else if (actual_) {
          // Coordenadas relativas al área de contenido (restar barra).
          actual_->alTocar(ev.x, ev.y - 20);
        }
        break;
      case TipoEventoUi::SWIPE_IZQUIERDA:
      case TipoEventoUi::SWIPE_DERECHA:
        if (!pilaUi_.empty()) break;                  // pila abierta: ignorar
        if (modo_ != ModoGestor::CARRUSEL) break;     // fijo: ignorar
        if (orden_.empty()) break;
        if (ev.tipo == TipoEventoUi::SWIPE_IZQUIERDA)
          indiceCarrusel_ = (indiceCarrusel_ + 1) % orden_.size();
        else
          indiceCarrusel_ = (indiceCarrusel_ + orden_.size() - 1) % orden_.size();
        aplicarPantalla(pantallaPorId(orden_[indiceCarrusel_]), msAhora);
        break;
    }
  }
  cola_.clear();

  // 2. Rotación automática en carrusel (sólo si no hay pila UI y no estamos en home).
  if (modo_ == ModoGestor::CARRUSEL && pilaUi_.empty() && actual_ != home_
      && !orden_.empty() && intervaloS_ > 0) {
    const uint32_t delta = msAhora - ultimoCambioMs_;
    if (delta >= static_cast<uint32_t>(intervaloS_) * 1000u) {
      indiceCarrusel_ = (indiceCarrusel_ + 1) % orden_.size();
      aplicarPantalla(pantallaPorId(orden_[indiceCarrusel_]), msAhora);
    }
  }

  // 3. Pintar.
  const uint8_t nDots = (modo_ == ModoGestor::CARRUSEL && pilaUi_.empty() && actual_ != home_)
                        ? static_cast<uint8_t>(orden_.size()) : 0;
  const uint8_t dotAct = static_cast<uint8_t>(indiceCarrusel_);
  renderer_.pintarBarraSuperior(actual_ ? actual_->nombre() : "", dotAct, nDots);
  if (actual_) actual_->dibujar(msAhora);
}

}  // namespace pantallas
