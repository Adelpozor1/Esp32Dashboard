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

// Colores (RGB565)
constexpr uint16_t COL_FONDO      = 0x0000;   // negro
constexpr uint16_t COL_CIRCULO    = 0x2124;   // gris oscuro
constexpr uint16_t COL_EJE        = 0x39C7;   // gris medio
constexpr uint16_t COL_ETIQUETA   = 0x8410;   // gris claro
constexpr uint16_t COL_CARDINAL   = 0x055F;   // azul
constexpr uint16_t COL_CENTRO     = 0xF800;   // rojo
constexpr uint16_t COL_AVION      = 0x07E0;   // verde
constexpr uint16_t COL_AVION_TXT  = 0xFFFF;   // blanco
constexpr uint16_t COL_STALE      = 0xFD20;   // ámbar
constexpr uint16_t COL_PANEL_TXT  = 0xFFFF;

void dibujarBase(TFT_eSprite& s, int radioKm) {
  s.fillSprite(COL_FONDO);
  const int cx = RADAR_LADO / 2;
  const int cy = RADAR_LADO / 2;
  const int rMax = RADAR_LADO / 2 - 10;
  // círculos concéntricos + etiquetas de radio
  s.setTextColor(COL_ETIQUETA);
  s.setTextFont(1);
  for (int i = 1; i <= 5; ++i) {
    int r = rMax * i / 5;
    s.drawCircle(cx, cy, r, COL_CIRCULO);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%dkm", (radioKm * i + 4) / 5);
    s.drawString(buf, cx + 2, cy - r - 8);
  }
  // ejes N/S/E/W
  s.drawFastVLine(cx, cy - rMax, 2 * rMax, COL_EJE);
  s.drawFastHLine(cx - rMax, cy, 2 * rMax, COL_EJE);
  // etiquetas cardinales
  s.setTextColor(COL_CARDINAL);
  s.drawString("N", cx - 4,       cy - rMax - 8);
  s.drawString("S", cx - 4,       cy + rMax + 2);
  s.drawString("E", cx + rMax + 2, cy - 4);
  s.drawString("O", cx - rMax - 8, cy - 4);
  // punto central
  s.fillCircle(cx, cy, 3, COL_CENTRO);
}

// Dibuja un triángulo pequeño orientado según trackDeg (0° = norte).
void dibujarAvion(TFT_eSprite& s, int x, int y, int trackDeg, const std::string& etiqueta) {
  const double rad = (trackDeg - 90) * M_PI / 180.0;
  const double c = std::cos(rad);
  const double sn = std::sin(rad);
  auto rot = [&](int px, int py, int& xr, int& yr) {
    xr = x + static_cast<int>(px * c - py * sn);
    yr = y + static_cast<int>(px * sn + py * c);
  };
  int x1, y1, x2, y2, x3, y3;
  rot(6, 0, x1, y1);
  rot(-4, -4, x2, y2);
  rot(-4, 4, x3, y3);
  s.fillTriangle(x1, y1, x2, y2, x3, y3, COL_AVION);
  if (!etiqueta.empty()) {
    s.setTextColor(COL_AVION_TXT);
    s.setTextFont(1);
    s.drawString(etiqueta.c_str(), x + 8, y - 4);
  }
}

void pintarPanel(const Snapshot& snap) {
  s_tft.fillRect(PANEL_X, 0, PANEL_ANCHO, PANTALLA_ALTO, COL_FONDO);
  s_tft.setTextColor(COL_PANEL_TXT, COL_FONDO);
  s_tft.setTextFont(2);
  s_tft.setCursor(PANEL_X + 4, 4);
  char buf[24];
  std::snprintf(buf, sizeof(buf), "%d aviones", static_cast<int>(snap.aeronaves.size()));
  s_tft.print(buf);

  if (snap.stale) {
    s_tft.setTextColor(COL_STALE, COL_FONDO);
    s_tft.setCursor(PANEL_X + 4, 22);
    s_tft.print("sin datos");
  }

  // hasta 8 aviones más cercanos, en font 1 (pequeña)
  s_tft.setTextFont(1);
  s_tft.setTextColor(COL_PANEL_TXT, COL_FONDO);
  int y = 44;
  const int maxLineas = 12;
  int mostrados = 0;
  for (const auto& a : snap.aeronaves) {
    if (mostrados >= maxLineas) break;
    s_tft.setCursor(PANEL_X + 4, y);
    std::snprintf(buf, sizeof(buf), "%-7.7s %2dkm",
                  a.callsign.empty() ? a.hex.c_str() : a.callsign.c_str(),
                  static_cast<int>(a.dist_km));
    s_tft.print(buf);
    y += 14;
    ++mostrados;
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
  // Sprite offscreen para el área del radar (240x240, 16bpp = 115 KB en RAM)
  s_sprite.setColorDepth(16);
  s_sprite.createSprite(RADAR_LADO, RADAR_LADO);
  s_sprite.fillSprite(COL_FONDO);
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
  s_tft.setTextColor(COL_ETIQUETA, COL_FONDO);
  s_tft.setTextFont(1);
  s_tft.setCursor(6, qrY + lado + margen + 6);
  s_tft.print(url.c_str());

  // Panel derecho: instrucciones
  const int px = PANEL_X + 4;
  s_tft.setTextColor(COL_PANEL_TXT, COL_FONDO);
  s_tft.setTextFont(2);
  s_tft.setCursor(px, 32);
  s_tft.print("1. WiFi:");
  s_tft.setTextColor(COL_AVION, COL_FONDO);
  s_tft.setTextFont(1);
  s_tft.setCursor(px, 54);
  s_tft.print(ssidAp.c_str());

  s_tft.setTextColor(COL_PANEL_TXT, COL_FONDO);
  s_tft.setTextFont(2);
  s_tft.setCursor(px, 78);
  s_tft.print("2. Escanea");
  s_tft.setCursor(px, 96);
  s_tft.print("   el QR");

  s_tft.setTextColor(COL_ETIQUETA, COL_FONDO);
  s_tft.setTextFont(1);
  s_tft.setCursor(px, 124);
  s_tft.print("o abre la");
  s_tft.setCursor(px, 136);
  s_tft.print("URL a mano");
}

void DisplayRadar::pintarRadar(const Snapshot& snap) {
  if (!s_iniciado) return;
  dibujarBase(s_sprite, snap.radio_km);

  const int cx = RADAR_LADO / 2;
  const int cy = RADAR_LADO / 2;
  const int rMax = RADAR_LADO / 2 - 10;

  for (const auto& a : snap.aeronaves) {
    if (a.dist_km > snap.radio_km) continue;
    const double r = (a.dist_km / snap.radio_km) * rMax;
    const double rad = (a.bearing - 90) * M_PI / 180.0;
    const int x = cx + static_cast<int>(r * std::cos(rad));
    const int y = cy + static_cast<int>(r * std::sin(rad));
    dibujarAvion(s_sprite, x, y, a.track_deg,
                 a.callsign.empty() ? a.hex : a.callsign);
  }

  // Banner stale sobre el propio sprite (área del radar)
  if (snap.stale) {
    s_sprite.fillRect(0, 0, RADAR_LADO, 16, COL_STALE);
    s_sprite.setTextColor(COL_FONDO, COL_STALE);
    s_sprite.setTextFont(1);
    s_sprite.drawString("sin conexion API", 4, 4);
  }

  // Empuja el sprite al display (área izquierda)
  s_sprite.pushSprite(0, 0);
  // Pinta panel derecho directamente en el TFT
  pintarPanel(snap);
}
