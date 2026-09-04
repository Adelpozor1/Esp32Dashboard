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
  void dibujar(uint32_t msAhora) override;

 private:
  enum class SubVista : uint8_t { ULTIMOS = 0, PROXIMOS = 1 };
  void dibujarSinDatos(TFT_eSPI& tft);
  void dibujarUltimos(TFT_eSPI& tft);
  void dibujarProximos(TFT_eSPI& tft);
  void dibujarIndicador(TFT_eSPI& tft);
  static std::string truncar(const std::string& s, size_t n);

  const FutbolSnapshot& snap_;
  SubVista sub_ = SubVista::ULTIMOS;
  bool     dirty_ = true;
  uint32_t ultObtenidoMs_ = 0;
};
