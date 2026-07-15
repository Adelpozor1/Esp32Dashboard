#include "display_radar.h"
#include <TFT_eSPI.h>
#include <qrcode.h>
#include <cmath>
#include <cstdio>
#include <algorithm>

namespace {

TFT_eSPI       s_tft;
TFT_eSprite    s_sprite(&s_tft);  // buffer offscreen 240x240 para el área del radar
bool           s_iniciado = false;

// Layout landscape (rotation 1) → 320x240
constexpr int PANTALLA_ANCHO = 320;
constexpr int PANTALLA_ALTO  = 240;
constexpr int RADAR_LADO     = 240;         // cuadrado a la izquierda
constexpr int PANEL_X        = RADAR_LADO;  // panel info empieza aquí
constexpr int PANEL_ANCHO    = PANTALLA_ANCHO - RADAR_LADO;

// Paleta sonar clásico (RGB565) — verde fosforito sobre negro, alto contraste.
constexpr uint16_t COL_FONDO       = 0x0000;   // negro
constexpr uint16_t COL_GRID        = 0x07E0;   // círculos internos VERDE MAX
constexpr uint16_t COL_GRID_BORDE  = 0x07E0;   // círculo exterior VERDE MAX
constexpr uint16_t COL_EJE         = 0x07E0;   // ejes NSEW VERDE MAX
constexpr uint16_t COL_GRID_TXT    = 0x07E0;   // etiquetas radio y grados
constexpr uint16_t COL_CARDINAL    = 0x07E0;   // N/S/E/W VERDE MAX
constexpr uint16_t COL_CENTRO      = 0xFC00;   // amarillo (observador)
constexpr uint16_t COL_AVION_TENUE = 0x0500;   // avión no iluminado por barrido
constexpr uint16_t COL_AVION_TAG   = 0x0680;   // etiqueta callsign no iluminada
constexpr uint16_t COL_AVION_HIT   = 0x07E0;   // avión iluminado por el barrido
constexpr uint16_t COL_ENCIMA      = 0xF800;   // rojo cuando dist<3km
constexpr uint16_t COL_STALE       = 0xFC00;   // amarillo
constexpr uint16_t COL_PANEL_TXT   = 0x07E0;   // verde brillante panel

void dibujarBaseSonar(TFT_eSprite& s, int radioKm) {
  s.fillSprite(COL_FONDO);
  const int cx = RADAR_LADO / 2;
  const int cy = RADAR_LADO / 2;
  const int rMax = RADAR_LADO / 2 - 12;
  s.setTextFont(1);
  // Círculos internos verde medio
  s.setTextColor(COL_GRID_TXT);
  for (int i = 1; i <= 4; ++i) {
    int r = rMax * i / 5;
    s.drawCircle(cx, cy, r, COL_GRID);
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%dkm", (radioKm * i + 4) / 5);
    s.drawString(buf, cx + 2, cy - r - 8);
  }
  // Círculo exterior más brillante (borde del sonar)
  s.drawCircle(cx, cy, rMax,     COL_GRID_BORDE);
  s.drawCircle(cx, cy, rMax - 1, COL_GRID_BORDE);
  // Ejes N/S/E/W
  s.drawFastVLine(cx, cy - rMax, 2 * rMax, COL_EJE);
  s.drawFastHLine(cx - rMax, cy, 2 * rMax, COL_EJE);
  // Marcas de grado cada 30° en el borde (radios pequeños)
  for (int deg = 0; deg < 360; deg += 30) {
    double a = (deg - 90) * M_PI / 180.0;
    int x1 = cx + int((rMax - 6) * std::cos(a));
    int y1 = cy + int((rMax - 6) * std::sin(a));
    int x2 = cx + int(rMax       * std::cos(a));
    int y2 = cy + int(rMax       * std::sin(a));
    s.drawLine(x1, y1, x2, y2, COL_GRID_BORDE);
  }
  // Etiquetas cardinales grandes
  s.setTextColor(COL_CARDINAL);
  s.drawString("N", cx - 4,        cy - rMax - 10);
  s.drawString("S", cx - 4,        cy + rMax + 2);
  s.drawString("E", cx + rMax + 2, cy - 4);
  s.drawString("O", cx - rMax - 10, cy - 4);
  // Punto central (observador)
  s.fillCircle(cx, cy, 3, COL_CENTRO);
}

// Sweep del sonar: línea del centro al borde con estela decreciente.
void dibujarBarrido(TFT_eSprite& s, int angDeg) {
  const int cx = RADAR_LADO / 2;
  const int cy = RADAR_LADO / 2;
  const int rMax = RADAR_LADO / 2 - 10;
  // 10 líneas de estela cada 4° con brillo decreciente
  static const uint16_t stele[10] = {
    0x07E0, 0x06E0, 0x05E0, 0x04E0, 0x0400,
    0x0340, 0x0280, 0x01C0, 0x0140, 0x00A0
  };
  for (int i = 9; i >= 0; --i) {
    int ang = angDeg - i * 4;
    double rad = (ang - 90) * M_PI / 180.0;
    int x2 = cx + int(rMax * std::cos(rad));
    int y2 = cy + int(rMax * std::sin(rad));
    s.drawLine(cx, cy, x2, y2, stele[i]);
  }
}

// Silueta de avión (cruz avión-vista-cenital) orientada según trackDeg.
// trackDeg: 0 = norte, 90 = este.
void dibujarIconoAvion(TFT_eSprite& s, int x, int y, int trackDeg, uint16_t color) {
  const double rad = (trackDeg - 90) * M_PI / 180.0;
  const double c = std::cos(rad);
  const double sn = std::sin(rad);
  auto rot = [&](int px, int py, int& xr, int& yr) {
    xr = x + int(px * c - py * sn);
    yr = y + int(px * sn + py * c);
  };
  int x1, y1, x2, y2;
  // Fuselaje (largo hacia el frente)
  rot(-4, 0, x1, y1);
  rot(5, 0, x2, y2);
  s.drawLine(x1, y1, x2, y2, color);
  // Alas
  rot(0, -4, x1, y1);
  rot(0, 4, x2, y2);
  s.drawLine(x1, y1, x2, y2, color);
  // Cola (estabilizador vertical más pequeño, atrás)
  rot(-4, -2, x1, y1);
  rot(-4, 2, x2, y2);
  s.drawLine(x1, y1, x2, y2, color);
}

// Un avión sobre el sonar: silueta orientada según su rumbo + tag SIEMPRE visible.
// Se realza cuando el barrido acaba de pasar por él, y se pinta en rojo si dist<3km.
void dibujarAvionSonar(TFT_eSprite& s, const Aeronave& a, int radioKm, int angBarrido) {
  if (a.dist_km > radioKm) return;
  const int cx = RADAR_LADO / 2;
  const int cy = RADAR_LADO / 2;
  const int rMax = RADAR_LADO / 2 - 12;

  const double r = (a.dist_km / radioKm) * rMax;
  const double rad = (a.bearing - 90) * M_PI / 180.0;
  const int x = cx + int(r * std::cos(rad));
  const int y = cy + int(r * std::sin(rad));

  const bool encima = (a.dist_km < 3.0);
  int diff = ((angBarrido - a.bearing) % 360 + 360) % 360;
  const bool iluminado = (diff < 40);

  uint16_t colorIcono, colorTag;
  if (encima) {
    colorIcono = COL_ENCIMA;
    colorTag   = COL_ENCIMA;
  } else if (iluminado) {
    colorIcono = COL_AVION_HIT;
    colorTag   = COL_AVION_HIT;
  } else {
    colorIcono = COL_AVION_TENUE;
    colorTag   = COL_AVION_TAG;
  }
  dibujarIconoAvion(s, x, y, a.track_deg, colorIcono);

  // Tag: callsign SIEMPRE visible al lado del avión
  const std::string& etiq = a.callsign.empty() ? a.hex : a.callsign;
  s.setTextColor(colorTag);
  s.setTextFont(1);
  s.drawString(etiq.c_str(), x + 7, y - 6);
}

// Panel derecho: nº aviones + destacado del más cercano (callsign, distancia,
// altitud). Si dist<3km, alerta en rojo "!ENCIMA!".
void pintarPanelSonar(const Snapshot& snap) {
  s_tft.fillRect(PANEL_X, 0, PANEL_ANCHO, PANTALLA_ALTO, COL_FONDO);
  char buf[24];

  s_tft.setTextColor(COL_PANEL_TXT, COL_FONDO);
  s_tft.setTextFont(1);
  s_tft.setCursor(PANEL_X + 4, 4);
  std::snprintf(buf, sizeof(buf), "%d aviones", (int)snap.aeronaves.size());
  s_tft.print(buf);

  if (snap.stale) {
    s_tft.setTextColor(COL_STALE, COL_FONDO);
    s_tft.setCursor(PANEL_X + 4, 16);
    s_tft.print("stale");
  }

  // Encontrar el avión más cercano
  const Aeronave* mc = nullptr;
  for (const auto& a : snap.aeronaves) {
    if (!mc || a.dist_km < mc->dist_km) mc = &a;
  }
  if (!mc) {
    s_tft.setTextColor(COL_GRID_TXT, COL_FONDO);
    s_tft.setTextFont(2);
    s_tft.setCursor(PANEL_X + 4, 40);
    s_tft.print("sin");
    s_tft.setCursor(PANEL_X + 4, 58);
    s_tft.print("aviones");
    return;
  }

  const bool encima = (mc->dist_km < 3.0);
  const uint16_t col = encima ? COL_ENCIMA : COL_PANEL_TXT;

  // Callsign en font 2
  s_tft.setTextColor(col, COL_FONDO);
  s_tft.setTextFont(2);
  s_tft.setCursor(PANEL_X + 4, 38);
  s_tft.print((mc->callsign.empty() ? mc->hex : mc->callsign).c_str());

  // Distancia en font 4 (grande)
  s_tft.setTextFont(4);
  s_tft.setCursor(PANEL_X + 4, 62);
  if (mc->dist_km < 10.0) std::snprintf(buf, sizeof(buf), "%.1f", mc->dist_km);
  else                    std::snprintf(buf, sizeof(buf), "%d", (int)mc->dist_km);
  s_tft.print(buf);
  s_tft.setTextFont(2);
  s_tft.setCursor(PANEL_X + 4, 96);
  s_tft.print("km");

  // Altitud
  s_tft.setTextColor(COL_PANEL_TXT, COL_FONDO);
  s_tft.setTextFont(2);
  s_tft.setCursor(PANEL_X + 4, 122);
  std::snprintf(buf, sizeof(buf), "%dft", mc->alt_ft);
  s_tft.print(buf);

  // Rumbo
  s_tft.setCursor(PANEL_X + 4, 144);
  std::snprintf(buf, sizeof(buf), "%d\xB0", mc->bearing);
  s_tft.print(buf);

  // Alerta
  if (encima) {
    s_tft.setTextColor(COL_ENCIMA, COL_FONDO);
    s_tft.setTextFont(2);
    s_tft.setCursor(PANEL_X + 4, 200);
    s_tft.print("ENCIMA");
  }
}

}  // namespace

void DisplayRadar::iniciar() {
  if (s_iniciado) return;
  s_tft.init();
  s_tft.setRotation(1);   // landscape 320x240
  s_tft.fillScreen(COL_FONDO);
  // Backlight: en la CYD el BL va en GPIO 21, active-high
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);
  // Sprite offscreen para el área del radar en 8bpp (240x240 = 57 KB).
  // 16bpp = 115 KB no cabe en el heap con WiFi+AsyncWebServer levantados.
  s_sprite.setColorDepth(8);
  void* p = s_sprite.createSprite(RADAR_LADO, RADAR_LADO);
  Serial.printf("[display] createSprite %dx%d 8bpp → %s (heap libre: %u)\n",
                RADAR_LADO, RADAR_LADO,
                p ? "OK" : "FALLO",
                (unsigned)ESP.getFreeHeap());
  if (p) s_sprite.fillSprite(COL_FONDO);
  s_iniciado = true;
}

void DisplayRadar::pintarMensaje(const std::string& titulo, const std::string& detalle) {
  if (!s_iniciado) return;
  s_tft.fillScreen(COL_FONDO);
  s_tft.setTextColor(COL_PANEL_TXT, COL_FONDO);
  s_tft.setTextFont(4);
  s_tft.setCursor(10, 60);
  s_tft.print(titulo.c_str());
  s_tft.setTextFont(2);
  s_tft.setCursor(10, 110);
  s_tft.print(detalle.c_str());
}

void DisplayRadar::pintarPortalQR(const std::string& ssidAp, const std::string& url) {
  if (!s_iniciado) return;
  s_tft.fillScreen(COL_FONDO);

  // Título
  s_tft.setTextColor(COL_CARDINAL, COL_FONDO);
  s_tft.setTextFont(2);
  s_tft.setCursor(6, 4);
  s_tft.print("Modo Portal");

  // Generar QR versión 3 (29x29 módulos), ECC_LOW → cabe una URL corta con margen.
  QRCode qr;
  constexpr uint8_t QR_VERSION = 3;
  uint8_t buffer[qrcode_getBufferSize(QR_VERSION)];
  qrcode_initText(&qr, buffer, QR_VERSION, ECC_LOW, url.c_str());

  // Escala 6 → 29*6 = 174 px; centrado en el cuadrante izquierdo (0..240).
  constexpr int ESCALA = 6;
  const int lado = qr.size * ESCALA;
  const int qrX = (RADAR_LADO - lado) / 2;   // 33
  const int qrY = 26;
  // Fondo blanco alrededor del QR (quiet zone y contraste con fondo negro).
  const int margen = 6;
  s_tft.fillRect(qrX - margen, qrY - margen,
                 lado + 2 * margen, lado + 2 * margen, TFT_WHITE);
  for (int y = 0; y < qr.size; ++y) {
    for (int x = 0; x < qr.size; ++x) {
      if (qrcode_getModule(&qr, x, y)) {
        s_tft.fillRect(qrX + x * ESCALA, qrY + y * ESCALA,
                       ESCALA, ESCALA, TFT_BLACK);
      }
    }
  }
  // URL debajo del QR
  s_tft.setTextColor(COL_GRID_TXT, COL_FONDO);
  s_tft.setTextFont(1);
  s_tft.setCursor(6, qrY + lado + margen + 6);
  s_tft.print(url.c_str());

  // Panel derecho: instrucciones
  const int px = PANEL_X + 4;
  s_tft.setTextColor(COL_PANEL_TXT, COL_FONDO);
  s_tft.setTextFont(2);
  s_tft.setCursor(px, 32);
  s_tft.print("1. WiFi:");
  s_tft.setTextColor(COL_AVION_HIT, COL_FONDO);
  s_tft.setTextFont(1);
  s_tft.setCursor(px, 54);
  s_tft.print(ssidAp.c_str());

  s_tft.setTextColor(COL_PANEL_TXT, COL_FONDO);
  s_tft.setTextFont(2);
  s_tft.setCursor(px, 78);
  s_tft.print("2. Escanea");
  s_tft.setCursor(px, 96);
  s_tft.print("   el QR");

  s_tft.setTextColor(COL_GRID_TXT, COL_FONDO);
  s_tft.setTextFont(1);
  s_tft.setCursor(px, 124);
  s_tft.print("o abre la");
  s_tft.setCursor(px, 136);
  s_tft.print("URL a mano");
}

void DisplayRadar::pintarRadar(const Snapshot& snap, int anguloBarridoDeg) {
  if (!s_iniciado) return;

  dibujarBaseSonar(s_sprite, snap.radio_km);
  dibujarBarrido(s_sprite, anguloBarridoDeg);

  for (const auto& a : snap.aeronaves) {
    dibujarAvionSonar(s_sprite, a, snap.radio_km, anguloBarridoDeg);
  }

  s_sprite.pushSprite(0, 0);
  pintarPanelSonar(snap);
}
