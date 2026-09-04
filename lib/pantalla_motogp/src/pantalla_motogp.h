#pragma once
#include "pantalla.h"
#include "motogp_client.h"
#include <cstdint>

class TFT_eSPI;

class PantallaMotogp : public pantallas::Pantalla {
 public:
  explicit PantallaMotogp(const MotogpSnapshot& snap) : snap_(snap) {}

  const char* nombre() const override { return "MotoGP"; }
  uint8_t id() const override { return 4; }

  void alEntrar() override;
  void alDeslizar(pantallas::Direccion dir) override;
  void dibujar(uint32_t msAhora) override;

 private:
  enum class SubVista : uint8_t { ULTIMOS = 0, CALENDARIO = 1 };
  void dibujarSinDatos(TFT_eSPI& tft);
  void dibujarUltimos(TFT_eSPI& tft);
  void dibujarCalendario(TFT_eSPI& tft);
  void dibujarIndicador(TFT_eSPI& tft);
  static std::string truncar(const std::string& s, size_t n);

  const MotogpSnapshot& snap_;
  SubVista sub_ = SubVista::ULTIMOS;
  bool     dirty_ = true;
  uint32_t ultObtenidoMs_ = 0;
};
