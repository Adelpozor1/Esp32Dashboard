#include "pantalla_config_localizacion.h"
#include "tft_driver.h"
#include "qr_view.h"
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <string>
#include <vector>

void PantallaConfigLocalizacion::alTocar(int x, int y) {
  // Volver: [10, 90] × [210, 240].
  if (x >= 10 && x <= 90 && y >= 210 && y <= 240) {
    gestor_.volverAtras();
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
  std::vector<std::string> lineas = {
    "Escanea con",
    "el movil (en",
    "la misma WiFi)",
    "",
    "Podras cambiar",
    "WiFi, direccion",
    "y radio.",
    "",
    "URL:",
    url,
  };
  qr_view::pintarPortalConQR(tft, "Cambiar WiFi/lugar", url, lineas);
  tft.setTextFont(2);
  tft.setTextColor(0x07E0, 0x0000);
  tft.setCursor(10, 222);
  tft.print("< Volver");
  dirty_ = false;
}
