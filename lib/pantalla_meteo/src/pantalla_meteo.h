#pragma once
#include "pantalla.h"
#include "meteo_client.h"
#include <cstdint>

class TFT_eSPI;

class PantallaMeteo : public pantallas::Pantalla {
 public:
  explicit PantallaMeteo(const MeteoSnapshot& snapshot) : snap_(snapshot) {}

  const char* nombre() const override { return "Meteo"; }
  uint8_t id() const override { return 2; }

  void alEntrar() override;
  void alDeslizar(pantallas::Direccion dir) override;
  void dibujar(uint32_t msAhora) override;

 private:
  enum class SubVista : uint8_t { HORAS = 0, DIAS = 1 };
  void dibujarSinDatos(TFT_eSPI& tft);
  void dibujarHoras(TFT_eSPI& tft);
  void dibujarDias(TFT_eSPI& tft);
  void dibujarBloqueActual(TFT_eSPI& tft);
  void dibujarIndicador(TFT_eSPI& tft);
  void dibujarIcono(TFT_eSPI& tft, int cx, int cy, int lado, int wmo);

  const MeteoSnapshot& snap_;
  SubVista sub_ = SubVista::HORAS;
  bool     dirty_ = true;
  uint32_t ultObtenidoMs_ = 0;
};
