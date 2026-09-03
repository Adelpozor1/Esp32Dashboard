#pragma once
#include <string>
#include <vector>

class TFT_eSPI;

namespace qr_view {

// Pinta un QR con la url dada en el rect [x, y, ancho, ancho] (cuadrado), sobre un
// cuadrado blanco con `margen` píxeles de quiet zone alrededor. `escala` es el
// tamaño de módulo en píxeles; con url típica (30-60 chars) usar escala 5-6.
// Devuelve el lado total pintado (cuadro blanco incluido).
int pintarSoloQR(TFT_eSPI& tft, int x, int y, int escala,
                 const std::string& url, int margen = 6);

// Pinta la vista completa "portal": título arriba-izq, QR grande a la izquierda
// (240×240), panel derecho con `lineas` (una entrada por línea) y `url` debajo del QR.
// Reutiliza pintarSoloQR internamente.
void pintarPortalConQR(TFT_eSPI& tft,
                       const std::string& titulo,
                       const std::string& url,
                       const std::vector<std::string>& lineasPanel);

}  // namespace qr_view
