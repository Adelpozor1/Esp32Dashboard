#pragma once
#include "pantalla.h"
#include "f1_client.h"
#include <cstdint>

class TFT_eSPI;

class PantallaF1 : public pantallas::Pantalla {
 public:
  explicit PantallaF1(const F1Snapshot& snap) : snap_(snap) {}

  const char* nombre() const override { return "F1"; }
  uint8_t id() const override { return 5; }

  void alEntrar() override;
  void alDeslizar(pantallas::Direccion dir) override;
  void alTocar(int x, int y) override;
  void dibujar(uint32_t msAhora) override;

 private:
  enum class SubVista : uint8_t { CALENDARIO = 0, CLASIFICACION = 1 };
  void dibujarSinDatos(TFT_eSPI& tft);
  void dibujarClasificacion(TFT_eSPI& tft);
  void dibujarCalendario(TFT_eSPI& tft);
  void dibujarIndicador(TFT_eSPI& tft);
  void alternarSubVista();
  static std::string truncar(const std::string& s, size_t n);

  const F1Snapshot& snap_;
  SubVista sub_ = SubVista::CALENDARIO;
  bool     dirty_ = true;
  uint32_t ultObtenidoMs_ = 0;
};
