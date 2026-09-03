# Plan — Pantalla Reloj + paleta dark (Bloque 2)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Sustituir el placeholder de Reloj por una `PantallaReloj` funcional con NTP + zona Europe/Madrid + paleta dark reutilizable en bloques posteriores.

**Architecture:** Dos libs nuevas (`lib/paleta_dark/` header-only con la paleta compartida y `lib/pantalla_reloj/` con la vista), un flag de build (`LOAD_FONT7` para el reloj tipo LCD), y dos cambios quirúrgicos en `src/main.cpp` (NTP + swap del placeholder). Sin nueva persistencia en `Config`.

**Tech Stack:** ESP32 Arduino, PlatformIO, TFT_eSPI (font 7 LCD-style 48 px + font 4 + font 2), `<time.h>` + `configTime()` + POSIX TZ string.

**Spec:** [`docs/superpowers/specs/2026-09-03-pantalla-reloj-y-paleta-dark-design.md`](../specs/2026-09-03-pantalla-reloj-y-paleta-dark-design.md)

---

## Mapa de ficheros

| Fichero | Responsabilidad | Estado |
|---|---|---|
| `lib/paleta_dark/library.json` | Manifest header-only | crear |
| `lib/paleta_dark/src/paleta_dark.h` | Constantes RGB565 de la paleta | crear |
| `lib/pantalla_reloj/library.json` | Manifest | crear |
| `lib/pantalla_reloj/src/pantalla_reloj.h` | Interfaz `PantallaReloj : Pantalla` | crear |
| `lib/pantalla_reloj/src/pantalla_reloj.cpp` | Implementación (layout + throttle) | crear |
| `platformio.ini` | Añadir `-DLOAD_FONT7` | modificar |
| `src/main.cpp` | NTP + setenv/tzset + swap placeholder | modificar |

---

## Task 1: `lib/paleta_dark/` — paleta compartida

**Files:**
- Create: `lib/paleta_dark/library.json`
- Create: `lib/paleta_dark/src/paleta_dark.h`

- [ ] **Step 1: Crear `library.json`**

```json
{
  "name": "paleta_dark",
  "version": "0.1.0",
  "description": "Paleta de color dark UI compartida por las vistas de datos"
}
```

- [ ] **Step 2: Crear `paleta_dark.h`**

```cpp
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
}  // namespace paleta_dark
```

- [ ] **Step 3: Compilar y verificar**

Run: `~/.platformio/penv/bin/pio run -e esp32dev -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos"`
Expected: `[SUCCESS]`. Header no referenciado aún, sólo comprobamos catálogo.

- [ ] **Step 4: Commit**

```bash
git add lib/paleta_dark
git commit -m "feat(paleta_dark): paleta compartida de vistas de datos"
```

---

## Task 2: `platformio.ini` — habilitar font 7 (LCD 7-seg 48 px)

**Files:**
- Modify: `platformio.ini`

- [ ] **Step 1: Añadir el flag**

En la sección `build_flags` del `[env:esp32dev]`, dentro del bloque de fonts existente (`-DLOAD_GLCD=1`, `-DLOAD_FONT2=1`, `-DLOAD_FONT4=1`), añadir:

```ini
  -DLOAD_FONT7=1
```

Sin tocar los demás flags.

- [ ] **Step 2: Compilar**

Run: `~/.platformio/penv/bin/pio run -e esp32dev -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos"`
Expected: `[SUCCESS]`. Reporta el nuevo % de Flash (esperado ~88%).

- [ ] **Step 3: Commit**

```bash
git add platformio.ini
git commit -m "feat: habilitar LOAD_FONT7 para el reloj LCD grande"
```

---

## Task 3: `lib/pantalla_reloj/` — vista Reloj

**Files:**
- Create: `lib/pantalla_reloj/library.json`
- Create: `lib/pantalla_reloj/src/pantalla_reloj.h`
- Create: `lib/pantalla_reloj/src/pantalla_reloj.cpp`

- [ ] **Step 1: Crear `library.json`**

```json
{
  "name": "pantalla_reloj",
  "version": "0.1.0",
  "description": "Vista Reloj digital con NTP y zona Europe/Madrid",
  "dependencies": {
    "bodmer/TFT_eSPI": "^2.5.43"
  }
}
```

- [ ] **Step 2: Crear `pantalla_reloj.h`**

```cpp
#pragma once
#include "pantalla.h"
#include <cstdint>

class PantallaReloj : public pantallas::Pantalla {
 public:
  PantallaReloj() = default;

  const char* nombre() const override { return "Reloj"; }
  uint8_t id() const override { return 1; }

  void alEntrar() override;
  void dibujar(uint32_t msAhora) override;

 private:
  int  ultSeg_ = -1;
  int  ultMin_ = -1;
  int  ultDia_ = -1;
  bool dirty_  = true;   // fuerza repintado completo (fondo + todo)
};
```

- [ ] **Step 3: Crear `pantalla_reloj.cpp`**

```cpp
#include "pantalla_reloj.h"
#include "tft_driver.h"
#include "paleta_dark.h"
#include <TFT_eSPI.h>
#include <time.h>
#include <cstdio>

namespace {
constexpr int OFFSET_Y = 20;
constexpr int LARGO_HORA_PX = 168;   // ancho aprox de "HH:MM" en font 7 (7*24 - lo justamos empíricamente)
constexpr int Y_HORA        = OFFSET_Y + 60;
constexpr int Y_FECHA       = OFFSET_Y + 160;
constexpr int Y_SYNC        = OFFSET_Y + 80;

const char* diasSemana[7] = {
  "domingo", "lunes", "martes", "miércoles", "jueves", "viernes", "sábado",
};
const char* meses[12] = {
  "enero", "febrero", "marzo", "abril", "mayo", "junio",
  "julio", "agosto", "septiembre", "octubre", "noviembre", "diciembre",
};

void pintarFondo(TFT_eSPI& tft) {
  tft.fillRect(0, OFFSET_Y, 320, 220, paleta_dark::COL_FONDO);
}

void pintarSincronizando(TFT_eSPI& tft) {
  pintarFondo(tft);
  tft.setTextColor(paleta_dark::COL_ACENTO, paleta_dark::COL_FONDO);
  tft.setTextFont(4);
  const char* txt = "Sincronizando...";
  int16_t ancho = tft.textWidth(txt);
  tft.setCursor((320 - ancho) / 2, Y_SYNC);
  tft.print(txt);
}

void pintarHoraMinuto(TFT_eSPI& tft, int h, int m) {
  char buf[8];
  std::snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
  tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
  tft.setTextFont(7);
  // Rect que cubre la zona antigua: ~192 px de ancho, 56 px de alto, centrado.
  const int x = (320 - LARGO_HORA_PX) / 2;
  tft.fillRect(x - 4, Y_HORA - 4, LARGO_HORA_PX + 8, 56, paleta_dark::COL_FONDO);
  tft.setCursor(x, Y_HORA);
  tft.print(buf);
}

void pintarSegundos(TFT_eSPI& tft, int s) {
  char buf[8];
  std::snprintf(buf, sizeof(buf), ":%02d", s);
  tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
  tft.setTextFont(4);
  // Colocados a la derecha del bloque HH:MM.
  const int xIni = (320 + LARGO_HORA_PX) / 2 + 4;
  tft.fillRect(xIni - 2, Y_HORA + 2, 60, 30, paleta_dark::COL_FONDO);
  tft.setCursor(xIni, Y_HORA + 4);
  tft.print(buf);
}

void pintarFecha(TFT_eSPI& tft, const struct tm& tm) {
  char buf[48];
  std::snprintf(buf, sizeof(buf), "%s %d %s",
                diasSemana[tm.tm_wday % 7],
                tm.tm_mday,
                meses[tm.tm_mon % 12]);
  tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
  tft.setTextFont(2);
  int16_t ancho = tft.textWidth(buf);
  tft.fillRect(0, Y_FECHA - 2, 320, 22, paleta_dark::COL_FONDO);
  tft.setCursor((320 - ancho) / 2, Y_FECHA);
  tft.print(buf);
}
}  // namespace

void PantallaReloj::alEntrar() {
  dirty_ = true;
  ultSeg_ = -1;
  ultMin_ = -1;
  ultDia_ = -1;
}

void PantallaReloj::dibujar(uint32_t) {
  auto& tft = tft_driver::obtenerTft();

  time_t now = time(nullptr);
  struct tm tm;
  localtime_r(&now, &tm);
  const bool sincronizado = (tm.tm_year + 1900) >= 2000;

  if (!sincronizado) {
    if (dirty_) {
      pintarSincronizando(tft);
      dirty_ = false;
    }
    return;
  }

  if (dirty_) {
    pintarFondo(tft);
    pintarHoraMinuto(tft, tm.tm_hour, tm.tm_min);
    pintarSegundos(tft, tm.tm_sec);
    pintarFecha(tft, tm);
    ultSeg_ = tm.tm_sec;
    ultMin_ = tm.tm_min;
    ultDia_ = tm.tm_mday;
    dirty_ = false;
    return;
  }
  if (tm.tm_min != ultMin_) {
    pintarHoraMinuto(tft, tm.tm_hour, tm.tm_min);
    ultMin_ = tm.tm_min;
  }
  if (tm.tm_sec != ultSeg_) {
    pintarSegundos(tft, tm.tm_sec);
    ultSeg_ = tm.tm_sec;
  }
  if (tm.tm_mday != ultDia_) {
    pintarFecha(tft, tm);
    ultDia_ = tm.tm_mday;
  }
}
```

- [ ] **Step 4: Compilar**

Run: `~/.platformio/penv/bin/pio run -e esp32dev -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos"`
Expected: `[SUCCESS]`. Todavía no está registrada, pero debe enlazar.

- [ ] **Step 5: Commit**

```bash
git add lib/pantalla_reloj
git commit -m "feat(pantalla_reloj): vista digital con throttle y fondo dark"
```

---

## Task 4: `src/main.cpp` — arrancar NTP y activar la vista real

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Añadir el include de la nueva pantalla**

Reemplazar la línea:
```cpp
#include "pantalla_proximamente.h"
```
por dos líneas (dejar la existente y añadir la nueva justo debajo):
```cpp
#include "pantalla_proximamente.h"
#include "pantalla_reloj.h"
```

- [ ] **Step 2: Inicializar NTP tras conectar WiFi**

Localizar el bloque de `setup()` donde tras `conectarWifi()` se llama a `modoRadar()`. En `conectarWifi()`, justo antes del `return true;` final (cuando el WiFi ya está conectado), añadir:

```cpp
  Serial.println("[wifi] arrancando NTP + TZ Europe/Madrid");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
```

(NOTA: `configTime`, `setenv`, `tzset` viven en `<time.h>` que ya trae la toolchain de arduino-esp32.)

- [ ] **Step 3: Reemplazar el placeholder de Reloj**

Localizar en `modoRadar()` la línea:
```cpp
  auto* reloj   = new PantallaProximamente(1, "Reloj");
```
y sustituirla por:
```cpp
  auto* reloj   = new PantallaReloj();
```

Los demás `PantallaProximamente` (Meteo, Fútbol, MotoGP, F1) se dejan intactos.

- [ ] **Step 4: Compilar**

Run: `~/.platformio/penv/bin/pio run -e esp32dev -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos"`
Expected: `[SUCCESS]`. Reporta el % de Flash final.

- [ ] **Step 5: Tests native como control**

Run: `~/.platformio/penv/bin/pio test -e native -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos"`
Expected: 44/44 PASSED.

- [ ] **Step 6: Commit**

```bash
git add src/main.cpp
git commit -m "feat(main): arrancar NTP Europe/Madrid y activar PantallaReloj"
```

---

## Task 5: Verificación en placa

**Files:** ninguno.

- [ ] **Step 1: Subir firmware**

Puerto probable: `/dev/cu.usbserial-1110` (comprobar con `ls /dev/cu.usbserial-*`).

Run: `~/.platformio/penv/bin/pio run -e esp32dev -t upload --upload-port /dev/cu.usbserial-1110 -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos"`
Expected: `[SUCCESS]`.

- [ ] **Step 2: Checklist manual**

- [ ] La placa arranca en Radar (o vista inicial del carrusel según config).
- [ ] Al llegar la vista Reloj: si NTP aún no sincronizó, se ve "Sincronizando..." en font 4, color azul acento, sobre fondo dark.
- [ ] Cuando NTP sincroniza (≤ 30 s tras conectar WiFi), la vista pasa a mostrar HH:MM grande, :SS pequeño al lado y la fecha en formato "jueves 3 septiembre" en la parte inferior.
- [ ] Los segundos tickean cada segundo sin parpadear el bloque HH:MM.
- [ ] Al cambiar el minuto, HH:MM se repinta limpio (sin restos del valor anterior).
- [ ] Volver al menú y volver al Reloj: se ve "dirty" repintado completo.
- [ ] Radar, menú y ajustes siguen en verde sonar sin cambios visuales.

- [ ] **Step 3: Reportar y actualizar memoria del proyecto**

Actualizar `~/.claude/projects/-Users-albertodelpozo-Documents-Proyectos-PlatformIO-Projects-Radar-vuelos/memory/project_radar_vuelo.md` con el hito del bloque 2 cerrado.

---

## Self-Review

- **Spec coverage:** Paleta dark → Task 1. LOAD_FONT7 → Task 2. Reloj (nombre, id, throttle, fecha en español, "Sincronizando…") → Task 3. NTP + TZ + swap placeholder → Task 4. Verificación en placa + checklist → Task 5.
- **Placeholder scan:** todos los steps traen código completo o comandos exactos. Ningún TODO/TBD.
- **Consistencia:** el header `pantalla_reloj.h` declara `alEntrar()` y `dibujar()`; el `.cpp` implementa ambos y define solo funciones locales en el anonymous namespace. `main.cpp` no toca nada más del ensamble.
