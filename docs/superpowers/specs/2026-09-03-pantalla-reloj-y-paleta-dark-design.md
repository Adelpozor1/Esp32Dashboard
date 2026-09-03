# Pantalla Reloj + paleta dark — diseño

- **Fecha:** 2026-09-03
- **Proyecto:** Radar de vuelo (ESP32 + Cheap Yellow Display)
- **Bloque:** 2 de 6 (Pantalla Reloj + fundación de estilo dark)
- **Autor:** Alberto del Pozo (tidehub)
- **Estado:** diseño aprobado conversacionalmente

## Contexto

El bloque 1 dejó el radar migrado a `PantallaRadar` (verde sonar) y placeholders `PantallaProximamente` en las ranuras 1..5 (Reloj, Meteo, Fútbol, MotoGP, F1). El bloque 2 sustituye el placeholder de Reloj por una pantalla real y establece la paleta visual que reutilizarán los bloques 3-6 (Meteo, Fútbol, MotoGP, F1). El radar mantiene su paleta verde sonar por identidad visual.

## Objetivos

- Pantalla Reloj digital con segundos, fecha con día de la semana, zona horaria Europe/Madrid (con DST europeo automático).
- Sincronización NTP no bloqueante inyectada al arranque tras conectar WiFi.
- Nueva **paleta dark** compartida (`lib/paleta_dark/`) como semilla estética para todos los bloques nuevos de datos.
- Sin persistencia adicional en `Config` (todo hardcoded).

## No-objetivos

- Alarmas, cronómetros, temporizadores.
- Cambio manual de hora o de zona horaria.
- Repintado del menú, sub-menú de ajustes o sub-pantallas de ajustes con la nueva paleta (queda para un retrofit futuro).
- Cambio del chrome de la barra superior (`RenderizadorUiReal`) — sigue verde sonar; transición visual con el contenido dark del reloj es aceptada.

## Arquitectura

Se crean dos libs nuevas y se modifica una:

### `lib/paleta_dark/` (nueva)

Header-only, sin `.cpp`. Solo constantes en `namespace paleta_dark` con `constexpr uint16_t` en formato RGB565:

```cpp
namespace paleta_dark {
constexpr uint16_t COL_FONDO       = 0x18E3;   // #0e1116 aprox
constexpr uint16_t COL_TXT_TITULO  = 0xFFFF;   // blanco
constexpr uint16_t COL_TXT_SECUND  = 0x8C71;   // #8b949e aprox
constexpr uint16_t COL_ACENTO      = 0x7CBF;   // #79b8ff aprox
constexpr uint16_t COL_OK          = 0x5EAC;   // #56d364 aprox
constexpr uint16_t COL_WARN        = 0xF3C1;   // #f0883e aprox
constexpr uint16_t COL_ERROR       = 0xFA25;   // #f85149 aprox
}  // namespace paleta_dark
```

Los bloques 3-6 incluyen este header sin nuevas deps.

### `lib/pantalla_reloj/` (nueva)

`PantallaReloj : public pantallas::Pantalla`:

- `nombre() -> "Reloj"`.
- `id() -> 1` (IdVista::RELOJ).
- `alEntrar()` marca dirty y resetea el estado de repintado incremental.
- `dibujar(msAhora)` lee `time(&now)` → `localtime_r`. Si `tm.tm_year + 1900 < 2000` pinta "Sincronizando…". En otro caso decide qué sub-áreas repintar según hayan cambiado el segundo, el minuto o el día desde la última vez.
- Estado interno: `int ultSeg_ = -1, ultMin_ = -1, ultDia_ = -1; bool dirty_ = true;`.

Layout (área de contenido 320×220, `OFFSET_Y=20` bajo la barra):

- Fondo: `paleta_dark::COL_FONDO` en todo el rect `(0, 20, 320, 220)`.
- `HH:MM` centrado horizontal en font 7 (LCD 7-seg 48 px), `y ≈ OFFSET_Y + 60`, color `COL_TXT_TITULO`.
- `:SS` a la derecha del bloque HH:MM en font 4 (~26 px), alineado a la baseline superior, color `COL_TXT_SECUND`.
- Fecha `"jueves 3 septiembre"` centrada horizontal en font 2, `y ≈ OFFSET_Y + 160`, color `COL_TXT_SECUND`.
- Estado "Sincronizando…": centrado en font 4, color `COL_ACENTO`.

### `platformio.ini` (modificar)

Añadir `-DLOAD_FONT7` en `build_flags` para poder usar el font 7-segmentos grande. Coste ~7 KB flash sobre el 87.4% actual.

### `src/main.cpp` (modificar)

Dos cambios:

1. Justo después de `conectarWifi() == true` y antes de `modoRadar()`:
   ```cpp
   configTime(0, 0, "pool.ntp.org", "time.nist.gov");
   setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
   tzset();
   ```
   La cadena POSIX corresponde a Europe/Madrid con DST europeo (marzo último domingo → +2, octubre último domingo → +1).

2. En `modoRadar()`, sustituir la línea:
   ```cpp
   auto* reloj   = new PantallaProximamente(1, "Reloj");
   ```
   por:
   ```cpp
   auto* reloj   = new PantallaReloj();
   ```

Todo lo demás de `main.cpp` (registro en gestor, orden en `vistas_orden`, sub-pantallas, tasks) sigue igual.

## Diseño de color y contraste

Los valores RGB565 son aproximaciones. Se ajustarán empíricamente si algún tono resulta ilegible en placa. Regla de bolsillo: nunca combinar `COL_TXT_SECUND` sobre `COL_ACENTO` (bajo contraste); usar siempre secundario sobre fondo, o título sobre fondo, o acento sobre fondo.

## Throttling / repintado incremental

- El tick del gestor de pantallas llega ~5 Hz (task display cada 200 ms).
- `PantallaReloj::dibujar` lee `time()` una vez por tick. Solo repinta:
  - `:SS` si `tm.tm_sec != ultSeg_`.
  - `HH:MM` si `tm.tm_min != ultMin_`.
  - Fecha si `tm.tm_mday != ultDia_`.
  - Todo el fondo + full-repaint si `dirty_` (primer entrar o volver del menú).

Sin sprites — con TFT_eSPI directo repintando solo rects es suficiente.

## Nombres de meses y días

Los nombres en español los generamos localmente (los locales C del ESP32 no traen es_ES): dos arrays `constexpr const char*` de 7 días y 12 meses en `pantalla_reloj.cpp`.

## Testing

- Sin tests native para este bloque: `time()`, `configTime`, `setenv/tzset`, TFT_eSPI son dependencias Arduino que no compilan en `native`.
- Verificación en placa (Task 21 del plan):
  - Sin WiFi al arrancar → Reloj muestra "Sincronizando…".
  - Tras conectar → hora local correcta (comparar con reloj del móvil).
  - Segunder tickea suave.
  - Cambio de minuto no parpadea el bloque grande.

## Riesgos y decisiones asumidas

- **Flash 87.4% actual**. Añadir `LOAD_FONT7` sube a ~88.0%. Aún dentro del margen (~13% libre). Los bloques 3-6 tienen que caber ahí también; si nos ajustamos, `LOAD_FONT7` se puede sustituir por texto renderizado a mano o por `LOAD_FONT6` (7-seg más pequeño).
- **Chrome de la barra superior sigue verde sonar**. Transición visual con contenido dark en Reloj asumida. Retrofit para más adelante.
- **Zona horaria fija**: si el aparato viaja el reloj marca hora española. No es problema hoy.
- **Servidor NTP público (`pool.ntp.org`)**. Sin cache ni retries a mano — si NTP falla, el reloj se queda en "Sincronizando…"; `configTime()` sigue reintentando en background.

## Fuera de alcance de este bloque

- Bloque 3 (Meteo) — Open-Meteo, siguiente en la cola.
- Retrofit visual de menú/ajustes/sub-pantallas.
- Reloj analógico o mixto.
