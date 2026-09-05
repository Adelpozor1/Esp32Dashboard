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
  enum class SubVista : uint8_t { RESULTADO = 0, AGENDA = 1 };
  void dibujarSinDatos(TFT_eSPI& tft);
  void dibujarResultado(TFT_eSPI& tft, uint32_t msAhora);
  void dibujarAgenda(TFT_eSPI& tft);
  void dibujarIndicador(TFT_eSPI& tft);
  void dibujarBadgeLive(TFT_eSPI& tft, int x, int y, bool encendido);
  static std::string truncar(const std::string& s, size_t n);

  const FutbolSnapshot& snap_;
  SubVista sub_ = SubVista::RESULTADO;
  bool     dirty_ = true;
  uint32_t ultObtenidoMs_ = 0;
  uint32_t ultParpadeoMs_ = 0;
  bool     badgeEncendido_ = true;
};
