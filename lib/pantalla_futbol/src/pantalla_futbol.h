#pragma once
#include "pantalla.h"
#include "futbol_client.h"
#include <cstdint>
#include <functional>

class TFT_eSPI;

class PantallaFutbol : public pantallas::Pantalla {
 public:
  // onToggleChampions se llama cuando el usuario tapea el botón "Champions"
  // (o "LaLiga" si estamos en Champions). El main la usa para cambiar la
  // competición en el FutbolClient y disparar un refresh inmediato.
  explicit PantallaFutbol(const FutbolSnapshot& snapshot,
                          std::function<void()> onToggleChampions = nullptr)
      : snap_(snapshot), onToggle_(std::move(onToggleChampions)) {}

  const char* nombre() const override { return "Futbol"; }
  uint8_t id() const override { return 3; }

  void alEntrar() override;
  void alDeslizar(pantallas::Direccion dir) override;
  void alTocar(int x, int y) override;
  void dibujar(uint32_t msAhora) override;

 private:
  enum class SubVista : uint8_t { JORNADA_ACTUAL = 0, PROXIMA_JORNADA = 1 };
  void dibujarSinDatos(TFT_eSPI& tft);
  void dibujarJornadaActual(TFT_eSPI& tft);
  void dibujarProximaJornada(TFT_eSPI& tft);
  void dibujarCabecera(TFT_eSPI& tft);
  void dibujarIndicador(TFT_eSPI& tft);
  bool tapEnBotonToggle(int x, int y) const;
  static std::string truncar(const std::string& s, size_t n);

  const FutbolSnapshot& snap_;
  std::function<void()> onToggle_;
  SubVista sub_ = SubVista::JORNADA_ACTUAL;
  bool     dirty_ = true;
  uint32_t ultObtenidoMs_ = 0;
};
