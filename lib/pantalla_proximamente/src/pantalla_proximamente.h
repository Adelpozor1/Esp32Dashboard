#pragma once
#include "pantalla.h"
#include <string>

class PantallaProximamente : public pantallas::Pantalla {
 public:
  PantallaProximamente(uint8_t id, std::string nombre) : id_(id), nombre_(std::move(nombre)) {}
  const char* nombre() const override { return nombre_.c_str(); }
  uint8_t id() const override { return id_; }
  void alEntrar() override { dirty_ = true; }
  void dibujar(uint32_t) override;

 private:
  uint8_t id_;
  std::string nombre_;
  bool dirty_ = true;
};
