#pragma once
#include "pantalla.h"
#include "radar_state.h"
#include <cstdint>

class TFT_eSprite;

class PantallaRadar : public pantallas::Pantalla {
 public:
  explicit PantallaRadar(RadarState& estado);
  ~PantallaRadar() override;

  const char* nombre() const override { return "Radar"; }
  uint8_t id() const override { return 0; }

  void alEntrar() override;
  void alSalir() override;
  void dibujar(uint32_t msAhora) override;

 private:
  RadarState& estado_;
  TFT_eSprite* sprite_ = nullptr;
  int angBarrido_ = 0;
};
