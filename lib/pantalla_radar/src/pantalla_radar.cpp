#include "pantalla_radar.h"
#include "tft_driver.h"
#include <TFT_eSPI.h>
#include <cmath>
#include <cstdio>

namespace {
constexpr int LADO_RADAR   = 220;   // sprite cuadrado dentro de 320x220
constexpr int OFFSET_Y     = 20;    // barra superior de 20 px
constexpr int PANEL_X      = LADO_RADAR;
constexpr int PANEL_ANCHO  = 320 - LADO_RADAR;
constexpr int PANEL_ALTO   = 240 - OFFSET_Y;   // altura útil del panel derecho (220 px)

constexpr uint16_t COL_FONDO       = 0x0000;
constexpr uint16_t COL_GRID        = 0x07E0;
constexpr uint16_t COL_EJE         = 0x07E0;
constexpr uint16_t COL_GRID_TXT    = 0x07E0;
constexpr uint16_t COL_CARDINAL    = 0x07E0;
constexpr uint16_t COL_CENTRO      = 0xFC00;
constexpr uint16_t COL_AVION_TENUE = 0x0500;
constexpr uint16_t COL_AVION_TAG   = 0x0680;
constexpr uint16_t COL_AVION_HIT   = 0x07E0;
constexpr uint16_t COL_ENCIMA      = 0xF800;
constexpr uint16_t COL_STALE       = 0xFC00;
constexpr uint16_t COL_PANEL_TXT   = 0x07E0;

void dibujarBaseSonar(TFT_eSprite& s, int radioKm) {
  s.fillSprite(COL_FONDO);
  const int cx = LADO_RADAR / 2, cy = LADO_RADAR / 2;
  const int rMax = LADO_RADAR / 2 - 10;
  s.setTextFont(1);
  s.setTextColor(COL_GRID_TXT);
  for (int i = 1; i <= 4; ++i) {
    int r = rMax * i / 5;
    s.drawCircle(cx, cy, r, COL_GRID);
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%dkm", (radioKm * i + 4) / 5);
    s.drawString(buf, cx + 2, cy - r - 8);
  }
  s.drawCircle(cx, cy, rMax,     COL_GRID);
  s.drawCircle(cx, cy, rMax - 1, COL_GRID);
  s.drawFastVLine(cx, cy - rMax, 2 * rMax, COL_EJE);
  s.drawFastHLine(cx - rMax, cy, 2 * rMax, COL_EJE);
  for (int deg = 0; deg < 360; deg += 30) {
    double a = (deg - 90) * M_PI / 180.0;
    int x1 = cx + int((rMax - 6) * std::cos(a));
    int y1 = cy + int((rMax - 6) * std::sin(a));
    int x2 = cx + int(rMax       * std::cos(a));
    int y2 = cy + int(rMax       * std::sin(a));
    s.drawLine(x1, y1, x2, y2, COL_GRID);
  }
  s.setTextColor(COL_CARDINAL);
  s.drawString("N", cx - 4,         cy - rMax - 10);
  s.drawString("S", cx - 4,         cy + rMax + 2);
  s.drawString("E", cx + rMax + 2,  cy - 4);
  s.drawString("O", cx - rMax - 10, cy - 4);
  s.fillCircle(cx, cy, 3, COL_CENTRO);
}

void dibujarBarrido(TFT_eSprite& s, int angDeg) {
  const int cx = LADO_RADAR / 2, cy = LADO_RADAR / 2;
  const int rMax = LADO_RADAR / 2 - 10;   // igual que en dibujarBaseSonar
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

void dibujarIconoAvion(TFT_eSprite& s, int x, int y, int trackDeg, uint16_t color) {
  const double rad = (trackDeg - 90) * M_PI / 180.0;
  const double c = std::cos(rad), sn = std::sin(rad);
  auto rot = [&](int px, int py, int& xr, int& yr) {
    xr = x + int(px * c - py * sn);
    yr = y + int(px * sn + py * c);
  };
  int x1, y1, x2, y2;
  rot(-4, 0, x1, y1); rot(5, 0, x2, y2); s.drawLine(x1, y1, x2, y2, color);
  rot(0, -4, x1, y1); rot(0, 4, x2, y2); s.drawLine(x1, y1, x2, y2, color);
  rot(-4, -2, x1, y1); rot(-4, 2, x2, y2); s.drawLine(x1, y1, x2, y2, color);
}

void dibujarAvionSonar(TFT_eSprite& s, const Aeronave& a, int radioKm, int angBarrido) {
  if (a.dist_km > radioKm) return;
  const int cx = LADO_RADAR / 2, cy = LADO_RADAR / 2;
  const int rMax = LADO_RADAR / 2 - 10;
  const double r = (a.dist_km / radioKm) * rMax;
  const double rad = (a.bearing - 90) * M_PI / 180.0;
  const int x = cx + int(r * std::cos(rad));
  const int y = cy + int(r * std::sin(rad));
  const bool encima = (a.dist_km < 3.0);
  int diff = ((angBarrido - a.bearing) % 360 + 360) % 360;
  const bool iluminado = (diff < 40);
  uint16_t colorIcono = encima ? COL_ENCIMA : iluminado ? COL_AVION_HIT : COL_AVION_TENUE;
  uint16_t colorTag   = encima ? COL_ENCIMA : iluminado ? COL_AVION_HIT : COL_AVION_TAG;
  dibujarIconoAvion(s, x, y, a.track_deg, colorIcono);
  const std::string& etiq = a.callsign.empty() ? a.hex : a.callsign;
  s.setTextColor(colorTag);
  s.setTextFont(1);
  s.drawString(etiq.c_str(), x + 7, y - 6);
}

void pintarPanelSonar(TFT_eSPI& tft, const Snapshot& snap) {
  tft.fillRect(PANEL_X, OFFSET_Y, PANEL_ANCHO, PANEL_ALTO, COL_FONDO);
  char buf[24];
  tft.setTextColor(COL_PANEL_TXT, COL_FONDO);
  tft.setTextFont(1);
  tft.setCursor(PANEL_X + 4, OFFSET_Y + 4);
  std::snprintf(buf, sizeof(buf), "%d aviones", (int)snap.aeronaves.size());
  tft.print(buf);
  if (snap.stale) {
    tft.setTextColor(COL_STALE, COL_FONDO);
    tft.setCursor(PANEL_X + 4, OFFSET_Y + 16);
    tft.print("stale");
  }
  const Aeronave* mc = nullptr;
  for (const auto& a : snap.aeronaves) if (!mc || a.dist_km < mc->dist_km) mc = &a;
  if (!mc) {
    tft.setTextColor(COL_GRID_TXT, COL_FONDO);
    tft.setTextFont(2);
    tft.setCursor(PANEL_X + 4, OFFSET_Y + 36);
    tft.print("sin");
    tft.setCursor(PANEL_X + 4, OFFSET_Y + 54);
    tft.print("aviones");
    return;
  }
  const bool encima = (mc->dist_km < 3.0);
  const uint16_t col = encima ? COL_ENCIMA : COL_PANEL_TXT;
  // Callsign
  tft.setTextColor(col, COL_FONDO);
  tft.setTextFont(2);
  tft.setCursor(PANEL_X + 4, OFFSET_Y + 34);
  tft.print((mc->callsign.empty() ? mc->hex : mc->callsign).c_str());
  // Distancia (grande)
  tft.setTextFont(4);
  tft.setCursor(PANEL_X + 4, OFFSET_Y + 56);
  if (mc->dist_km < 10.0) std::snprintf(buf, sizeof(buf), "%.1f", mc->dist_km);
  else                    std::snprintf(buf, sizeof(buf), "%d", (int)mc->dist_km);
  tft.print(buf);
  tft.setTextFont(2);
  tft.setCursor(PANEL_X + 4, OFFSET_Y + 88);
  tft.print("km");
  // Altitud y rumbo
  tft.setTextColor(COL_PANEL_TXT, COL_FONDO);
  tft.setTextFont(2);
  tft.setCursor(PANEL_X + 4, OFFSET_Y + 112);
  std::snprintf(buf, sizeof(buf), "%dft", mc->alt_ft);
  tft.print(buf);
  tft.setCursor(PANEL_X + 4, OFFSET_Y + 132);
  std::snprintf(buf, sizeof(buf), "%d\xB0", mc->bearing);
  tft.print(buf);
  // Alerta ENCIMA — bien despegada del borde inferior
  if (encima) {
    tft.setTextColor(COL_ENCIMA, COL_FONDO);
    tft.setCursor(PANEL_X + 4, OFFSET_Y + 180);
    tft.print("ENCIMA");
  }
}

}  // namespace

PantallaRadar::PantallaRadar(RadarState& estado) : estado_(estado) {}
PantallaRadar::~PantallaRadar() { alSalir(); }

void PantallaRadar::alEntrar() {
  if (sprite_) return;
  sprite_ = new TFT_eSprite(&tft_driver::obtenerTft());
  sprite_->setColorDepth(8);
  void* p = sprite_->createSprite(LADO_RADAR, LADO_RADAR);
  // Requiere Serial.begin(...) previo (main.cpp lo hace en setup).
  Serial.printf("[radar] sprite %dx%d -> %s (heap %u)\n",
                LADO_RADAR, LADO_RADAR, p ? "OK" : "FAIL",
                (unsigned)ESP.getFreeHeap());
  if (!p) {
    // Sin heap para el bitmap: liberamos el objeto y dejamos sprite_ = nullptr
    // para que dibujar() salga por su guardia y no pinte a hueco.
    delete sprite_;
    sprite_ = nullptr;
    return;
  }
  sprite_->fillSprite(COL_FONDO);
}

void PantallaRadar::alSalir() {
  if (!sprite_) return;
  sprite_->deleteSprite();
  delete sprite_;
  sprite_ = nullptr;
}

void PantallaRadar::dibujar(uint32_t) {
  if (!sprite_) return;
  Snapshot snap = estado_.snapshot();
  dibujarBaseSonar(*sprite_, snap.radio_km);
  dibujarBarrido(*sprite_, angBarrido_);
  for (const auto& a : snap.aeronaves) {
    dibujarAvionSonar(*sprite_, a, snap.radio_km, angBarrido_);
  }
  sprite_->pushSprite(0, OFFSET_Y);
  pintarPanelSonar(tft_driver::obtenerTft(), snap);
  angBarrido_ = (angBarrido_ + 10) % 360;
}
