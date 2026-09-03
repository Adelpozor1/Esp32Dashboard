#include "qr_view.h"
#include <TFT_eSPI.h>
#include <qrcode.h>
#include <vector>

namespace qr_view {

namespace {
constexpr uint16_t COL_FONDO = 0x0000;   // negro
constexpr uint16_t COL_TITULO = 0x07E0;  // verde brillante
constexpr uint16_t COL_PANEL = 0x07E0;
constexpr uint16_t COL_URL   = 0x07E0;

int longitudUrlAVersion(size_t n) {
  // Reglas conservadoras con ECC_LOW y modo byte:
  //  v2 (25x25) ≤ 32 chars, v3 (29x29) ≤ 53, v4 (33x33) ≤ 78.
  if (n <= 32)  return 2;
  if (n <= 53)  return 3;
  if (n <= 78)  return 4;
  return 5;
}
}  // namespace

int pintarSoloQR(TFT_eSPI& tft, int x, int y, int escala,
                 const std::string& url, int margen) {
  const int version = longitudUrlAVersion(url.size());
  const int bufSize = qrcode_getBufferSize(version);
  std::vector<uint8_t> buffer(bufSize);
  QRCode qr;
  qrcode_initText(&qr, buffer.data(), version, ECC_LOW, url.c_str());
  const int lado = qr.size * escala;
  tft.fillRect(x - margen, y - margen, lado + 2 * margen, lado + 2 * margen,
               TFT_WHITE);
  for (int qy = 0; qy < qr.size; ++qy) {
    for (int qx = 0; qx < qr.size; ++qx) {
      if (qrcode_getModule(&qr, qx, qy)) {
        tft.fillRect(x + qx * escala, y + qy * escala, escala, escala, TFT_BLACK);
      }
    }
  }
  return lado + 2 * margen;
}

void pintarPortalConQR(TFT_eSPI& tft,
                       const std::string& titulo,
                       const std::string& url,
                       const std::vector<std::string>& lineasPanel) {
  tft.fillScreen(COL_FONDO);
  tft.setTextColor(COL_TITULO, COL_FONDO);
  tft.setTextFont(2);
  tft.setCursor(6, 4);
  tft.print(titulo.c_str());

  // Área izquierda: cuadrado 240x240. QR centrado con escala 6.
  constexpr int LADO_IZQ = 240;
  constexpr int ESCALA = 6;
  // Estimación del tamaño para centrar (v2..v5 → 25..37 módulos).
  const int version = (url.size() <= 32) ? 2 : (url.size() <= 53) ? 3 : (url.size() <= 78) ? 4 : 5;
  const int tamModulos = 17 + version * 4;  // regla del QR
  const int lado = tamModulos * ESCALA;
  const int qrX = (LADO_IZQ - lado) / 2;
  const int qrY = 26;

  pintarSoloQR(tft, qrX, qrY, ESCALA, url, /*margen=*/6);

  tft.setTextColor(COL_URL, COL_FONDO);
  tft.setTextFont(1);
  tft.setCursor(6, qrY + lado + 12);
  tft.print(url.c_str());

  // Panel derecho: cada línea 18 px de alto en font 2.
  int py = 32;
  tft.setTextColor(COL_PANEL, COL_FONDO);
  tft.setTextFont(2);
  for (const auto& l : lineasPanel) {
    tft.setCursor(LADO_IZQ + 4, py);
    tft.print(l.c_str());
    py += 20;
  }
}

}  // namespace qr_view
