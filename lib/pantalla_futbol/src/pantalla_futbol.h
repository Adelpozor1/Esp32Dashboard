#pragma once
#include "pantalla.h"
#include "futbol_client.h"
#include <cstdint>

class TFT_eSPI;

class PantallaFutbol : public pantallas::Pantalla {
 public:
  explicit PantallaFutbol(const FutbolSnapshot& snapshot) : snap_(snapshot) {}

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
  void alternarSubVista();
  static std::string truncar(const std::string& s, size_t n);

  const FutbolSnapshot& snap_;
  SubVista sub_ = SubVista::JORNADA_ACTUAL;
  bool     dirty_ = true;
  uint32_t ultObtenidoMs_ = 0;
};
