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

  if (url.size() > URL_MAX_CHARS) {
    tft.setTextColor(0xF800, COL_FONDO);   // rojo
    tft.setTextFont(2);
    tft.setCursor(10, 40);
    tft.print("URL demasiado larga");
    tft.setCursor(10, 70);
    tft.print("para el QR");
    tft.setTextFont(1);
    tft.setCursor(10, 110);
    tft.print(url.c_str());
    return;
  }

  tft.setTextFont(2);
  tft.setCursor(6, 4);
  tft.print(titulo.c_str());

  // Área izquierda: cuadrado 240x240. QR centrado.
  constexpr int LADO_IZQ = 240;
  // Delegamos la elección de versión al mismo helper que usa pintarSoloQR
  // (evita drift entre el centrado y el QR realmente pintado).
  const int version = longitudUrlAVersion(url.size());
  const int tamModulos = 17 + version * 4;  // regla del QR (4V + 17)
  // Escala adaptativa: 6 px/módulo cabe para v2/v3 (150/174 px); v4 exige 5 y v5
  // exige 4 para no clippear el QR ni la URL bajo el mismo (layout 320×240 con
  // qrY=26 + URL font 1 debajo). Bajar la escala mantiene la legibilidad porque
  // los módulos son cuadrados perfectos sin antialiasing.
  const int escala = (version <= 3) ? 6 : (version == 4) ? 5 : 4;
  const int lado = tamModulos * escala;
  const int qrX = (LADO_IZQ - lado) / 2;
  const int qrY = 26;

  pintarSoloQR(tft, qrX, qrY, escala, url, /*margen=*/6);

  tft.setTextColor(COL_URL, COL_FONDO);
  tft.setTextFont(1);
  tft.setCursor(6, qrY + lado + 12);
  tft.print(url.c_str());

  // Panel derecho: 80 px de ancho (320-240). Font 2 con interlineado 20 px
  // — ~8 chars por línea, hasta ~10 líneas verticales. Guard para no pintar
  // fuera de los 240 px de alto. Los callers son responsables de truncar los
  // strings largos y de no meter más de ~10 líneas.
  int py = 32;
  tft.setTextColor(COL_PANEL, COL_FONDO);
  tft.setTextFont(2);
  for (const auto& l : lineasPanel) {
    if (py > 224) break;
    tft.setCursor(LADO_IZQ + 4, py);
    tft.print(l.c_str());
    py += 20;
  }
}

}  // namespace qr_view
