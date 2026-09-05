#pragma once
#include <cstdint>

// Paleta "dark UI moderno" compartida por Reloj, Meteo, Fútbol, MotoGP, F1.
// Valores RGB565 aproximados a los hexadecimales indicados; se ajustarán en
// placa si algún tono queda ilegible. Radar mantiene su paleta verde sonar aparte.
namespace paleta_dark {
constexpr uint16_t COL_FONDO       = 0x18E3;   // ~ #0e1116
constexpr uint16_t COL_TXT_TITULO  = 0xFFFF;   // blanco
constexpr uint16_t COL_TXT_SECUND  = 0x8C71;   // ~ #8b949e
constexpr uint16_t COL_ACENTO      = 0x7CBF;   // ~ #79b8ff
constexpr uint16_t COL_OK          = 0x5EAC;   // ~ #56d364
constexpr uint16_t COL_WARN        = 0xF3C1;   // ~ #f0883e
constexpr uint16_t COL_ERROR       = 0xFA25;   // ~ #f85149
constexpr uint16_t COL_LIVE        = 0xF800;   // rojo puro para badge EN VIVO
constexpr uint16_t COL_CAJA        = 0x30E5;   // gris azulado ~ #333844
}  // namespace paleta_dark
