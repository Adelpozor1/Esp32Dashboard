#include "pantalla_config_localizacion.h"
#include "tft_driver.h"
#include "qr_view.h"
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <Arduino.h>
#include <string>
#include <vector>

void PantallaConfigLocalizacion::alTocar(int x, int y) {
  // Volver: [10, 90] × [210, 240].
  if (x >= 10 && x <= 90 && y >= 210 && y <= 240) {
    gestor_.volverAtras();
    return;
  }
  // Portal AP: [210, 316] × [208, 238]. Borra SOLO las credenciales WiFi
  // (conserva dirección, radio, ajustes de UI) y reinicia — al no poder
  // conectarse, la placa cae al portal AP para reconfigurar desde el móvil.
  if (x >= 210 && x <= 316 && y >= 208 && y <= 238) {
    Config nueva = cfg_;
    nueva.ssid.clear();
    nueva.password.clear();
    ConfigStore::guardar(nueva);
    Serial.println("[cfg] credenciales WiFi borradas, reiniciando en portal AP");
    delay(300);
    ESP.restart();
  }
}

void PantallaConfigLocalizacion::dibujar(uint32_t) {
  if (!dirty_) return;
  auto& tft = tft_driver::obtenerTft();
  tft.fillRect(0, 20, 320, 220, 0x0000);

  if (WiFi.status() != WL_CONNECTED) {
    tft.setTextFont(2);
    tft.setTextColor(0xF800, 0x0000);
    tft.setCursor(10, 40);
    tft.print("Sin conexion WiFi ahora.");
    tft.setTextColor(0x07E0, 0x0000);
    tft.setCursor(10, 70);
    tft.print("Usa Reset total para");
    tft.setCursor(10, 88);
    tft.print("volver al portal AP.");
    tft.setCursor(10, 220);
    tft.print("< Volver");
    dirty_ = false;
    return;
  }

  std::string url = std::string("http://") + WiFi.localIP().toString().c_str() + "/config";
  // Panel derecho: font 2 (~8 chars por línea, ~10 líneas). Truncamos strings
  // largos con "." para no salirnos.
  auto trunc = [](const std::string& s, size_t n) {
    if (s.size() <= n) return s;
    return s.substr(0, n - 1) + ".";
  };
  char radioBuf[16];
  std::snprintf(radioBuf, sizeof(radioBuf), "Radio %dkm", cfg_.radio_km);
  std::vector<std::string> lineas = {
    "ACTUAL:",
    trunc(cfg_.ssid, 8),
    trunc(cfg_.direccion, 8),
    radioBuf,
    "",
    "IP:",
    std::string(WiFi.localIP().toString().c_str()),
    "",
    "Escanea",
    "el QR",
  };
  qr_view::pintarPortalConQR(tft, "Cambiar WiFi/lugar", url, lineas);
  // Botón < Volver (izq).
  tft.setTextFont(2);
  tft.setTextColor(0x07E0, 0x0000);
  tft.setCursor(10, 222);
  tft.print("< Volver");
  // Botón "Portal AP" (der) — borra credenciales WiFi y reinicia en portal
  // para poder reconfigurar desde el móvil. Hitbox: [210, 320] × [210, 240].
  constexpr int BX = 210, BY = 208, BW = 106, BH = 30;
  tft.drawRect(BX, BY, BW, BH, 0xFEA0);   // borde ámbar
  tft.setTextColor(0xFEA0, 0x0000);
  const char* etq = "Portal AP >";
  int16_t tw = tft.textWidth(etq);
  tft.setCursor(BX + (BW - tw) / 2, BY + 8);
  tft.print(etq);
  dirty_ = false;
}
