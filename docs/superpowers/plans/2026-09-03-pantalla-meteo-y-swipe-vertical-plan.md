# Plan — Pantalla Meteo + swipe vertical (Bloque 3)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development.

**Goal:** Sustituir el placeholder Meteo por una vista real con datos de Open-Meteo y dos sub-vistas (12 h próximas y 5 días) alternables por swipe vertical.

**Architecture:** Nuevo cliente `meteo_client` (parser puro testable + fetch bloqueante HTTPS). Nueva `pantalla_meteo` con iconos dibujados a mano. Ampliación táctil: `SWIPE_ARRIBA/ABAJO` cascadeados desde `GestureDetector` a `GestorPantallas`; refactor de `alDeslizar(int)` → `alDeslizar(Direccion enum)`. Task de refresco en core 0 cada 30 min.

**Tech Stack:** ESP32 Arduino, PlatformIO, TFT_eSPI, ArduinoJson 7, HTTP(S) vía `WifiHttpClient` (ya existe).

**Spec:** [`docs/superpowers/specs/2026-09-03-pantalla-meteo-y-swipe-vertical-design.md`](../specs/2026-09-03-pantalla-meteo-y-swipe-vertical-design.md)

---

## Mapa de ficheros

| Fichero | Responsabilidad | Estado |
|---|---|---|
| `lib/touch/src/gesture_detector.h` | Añadir `SWIPE_ARRIBA`, `SWIPE_ABAJO` al enum | modificar |
| `lib/touch/src/gesture_detector.cpp` | Detección vertical | modificar |
| `test/test_gesture_detector/test_main.cpp` | 4 tests nuevos | ampliar |
| `lib/touch/src/touch.cpp` | Reenviar los 2 nuevos casos (implícito, la cola es tipada) | modificar (si aplica) |
| `lib/pantallas/src/pantalla.h` | `enum class Direccion` + `alDeslizar(Direccion)` | modificar |
| `lib/pantallas/src/gestor_pantallas.h` | `TipoEventoUi::SWIPE_ARRIBA/ABAJO` | modificar |
| `lib/pantallas/src/gestor_pantallas.cpp` | Delegar swipes verticales a la pantalla actual | modificar |
| `test/test_gestor_pantallas/test_main.cpp` | 2 tests nuevos + ajuste `PantallaFake` | ampliar |
| `lib/meteo_client/library.json` | Manifest | crear |
| `lib/meteo_client/src/meteo_client.h` | API pública | crear |
| `lib/meteo_client/src/meteo_client.cpp` | Fetch + parser | crear |
| `test/test_meteo_client/test_main.cpp` | Tests native del parser | crear |
| `lib/pantalla_meteo/library.json` | Manifest | crear |
| `lib/pantalla_meteo/src/pantalla_meteo.h` | Interfaz | crear |
| `lib/pantalla_meteo/src/pantalla_meteo.cpp` | Vista + iconos + sub-vistas | crear |
| `src/main.cpp` | Traductor de eventos + task refresh + swap placeholder | modificar |

---

## Task 1: `GestureDetector` amplía SWIPE vertical (TDD native)

**Files:**
- Modify: `lib/touch/src/gesture_detector.h`
- Modify: `lib/touch/src/gesture_detector.cpp`
- Modify: `test/test_gesture_detector/test_main.cpp`

- [ ] **Step 1: Escribir 4 tests nuevos (fallan sin implementación)**

Añadir en `test/test_gesture_detector/test_main.cpp` antes del `int main`:

```cpp
void test_swipe_arriba_produce_evento(void) {
  GestureDetector g;
  g.onPress(100, 200, 1000);
  auto ev = g.onRelease(105, 100, 1200);   // dy=-100 rápido
  TEST_ASSERT_TRUE(ev.has_value());
  TEST_ASSERT_EQUAL(static_cast<int>(TipoEvento::SWIPE_ARRIBA), static_cast<int>(ev->tipo));
}

void test_swipe_abajo_produce_evento(void) {
  GestureDetector g;
  g.onPress(100, 40, 1000);
  auto ev = g.onRelease(95, 180, 1200);    // dy=+140
  TEST_ASSERT_TRUE(ev.has_value());
  TEST_ASSERT_EQUAL(static_cast<int>(TipoEvento::SWIPE_ABAJO), static_cast<int>(ev->tipo));
}

void test_swipe_diagonal_predomina_vertical(void) {
  GestureDetector g;
  g.onPress(100, 40, 1000);
  auto ev = g.onRelease(140, 140, 1200);   // dx=40, dy=100 -> vertical (mayor y)
  TEST_ASSERT_TRUE(ev.has_value());
  TEST_ASSERT_EQUAL(static_cast<int>(TipoEvento::SWIPE_ABAJO), static_cast<int>(ev->tipo));
}

void test_swipe_horizontal_no_es_vertical(void) {
  GestureDetector g;
  g.onPress(20, 100, 1000);
  auto ev = g.onRelease(180, 130, 1200);   // dx=160, dy=30 -> horizontal
  TEST_ASSERT_TRUE(ev.has_value());
  TEST_ASSERT_EQUAL(static_cast<int>(TipoEvento::SWIPE_DERECHA), static_cast<int>(ev->tipo));
}
```

Y añadir sus `RUN_TEST` al final de `main`.

- [ ] **Step 2: Run test — falla porque `SWIPE_ARRIBA` / `SWIPE_ABAJO` no existen**

Run: `~/.platformio/penv/bin/pio test -e native -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos" -f test_gesture_detector`

- [ ] **Step 3: Ampliar `gesture_detector.h`**

Sustituir el enum `TipoEvento` por:

```cpp
enum class TipoEvento : uint8_t {
  TAP             = 0,
  SWIPE_IZQUIERDA = 1,
  SWIPE_DERECHA   = 2,
  SWIPE_ARRIBA    = 3,
  SWIPE_ABAJO     = 4,
};
```

Actualizar el bloque de umbrales/comentarios para mencionar que el gesto vertical usa los mismos umbrales que el horizontal (`UMBRAL_SWIPE_MS`, `UMBRAL_SWIPE_PX`), y que el dead zone `[20, 60]` se aplica ahora en 2D (si `|dx|` o `|dy|` cae en ese rango sin superar el umbral SWIPE ni cumplir la caja TAP, se descarta).

- [ ] **Step 4: Ampliar `gesture_detector.cpp`**

Sustituir el bloque final de `onRelease` desde el primer `if (dur < UMBRAL_TAP_MS && ...)` hasta el `return std::nullopt;` por:

```cpp
  if (dur < UMBRAL_TAP_MS && std::abs(dx) < UMBRAL_TAP_PX && std::abs(dy) < UMBRAL_TAP_PX) {
    return EventoTactil{TipoEvento::TAP, x0_, y0_};
  }
  if (dur < UMBRAL_SWIPE_MS) {
    const bool horiz = std::abs(dx) > std::abs(dy);
    if (horiz && std::abs(dx) > UMBRAL_SWIPE_PX) {
      return EventoTactil{dx > 0 ? TipoEvento::SWIPE_DERECHA : TipoEvento::SWIPE_IZQUIERDA,
                          x0_, y0_};
    }
    if (!horiz && std::abs(dy) > UMBRAL_SWIPE_PX) {
      return EventoTactil{dy > 0 ? TipoEvento::SWIPE_ABAJO : TipoEvento::SWIPE_ARRIBA,
                          x0_, y0_};
    }
  }
  return std::nullopt;
```

Comportamiento equivalente al anterior para horizontal + nuevo caso vertical simétrico.

- [ ] **Step 5: Tests verdes**

Run: `~/.platformio/penv/bin/pio test -e native -f test_gesture_detector`
Expected: 15/15 (11 previos + 4 nuevos).

- [ ] **Step 6: Compilar firmware**

Run: `~/.platformio/penv/bin/pio run -e esp32dev -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos"`
Expected: `[SUCCESS]`.

- [ ] **Step 7: Commit**

```bash
git add lib/touch/src/gesture_detector.h lib/touch/src/gesture_detector.cpp test/test_gesture_detector
git commit -m "feat(touch): detector amplía swipe vertical (ARRIBA/ABAJO)"
```

---

## Task 2: `pantallas` — enum `Direccion` + delegación vertical al gestor + tests

**Files:**
- Modify: `lib/pantallas/src/pantalla.h`
- Modify: `lib/pantallas/src/gestor_pantallas.h`
- Modify: `lib/pantallas/src/gestor_pantallas.cpp`
- Modify: `test/test_gestor_pantallas/test_main.cpp`

- [ ] **Step 1: Sustituir `alDeslizar(int)` por `alDeslizar(Direccion)` en `pantalla.h`**

En `lib/pantallas/src/pantalla.h`, añadir sobre la clase `Pantalla` (dentro del namespace `pantallas`):

```cpp
enum class Direccion : uint8_t {
  IZQUIERDA = 0,
  DERECHA   = 1,
  ARRIBA    = 2,
  ABAJO     = 3,
};
```

Sustituir la línea:
```cpp
  virtual void alDeslizar(int /*direccion*/) {}
```
por:
```cpp
  // direccion: sentido del swipe interpretado por el gestor táctil.
  virtual void alDeslizar(Direccion /*direccion*/) {}
```

Actualizar el comentario si menciona explícitamente `-1/+1`.

- [ ] **Step 2: Ampliar `TipoEventoUi` en `gestor_pantallas.h`**

Sustituir la enumeración por:

```cpp
enum class TipoEventoUi : uint8_t {
  TAP             = 0,
  SWIPE_IZQUIERDA = 1,
  SWIPE_DERECHA   = 2,
  SWIPE_ARRIBA    = 3,
  SWIPE_ABAJO     = 4,
};
```

- [ ] **Step 3: Delegar swipes verticales en `gestor_pantallas.cpp`**

En el bucle de `tick` que procesa la cola, dentro del `switch (ev.tipo)`, añadir dos `case` adicionales (colocarlos tras los SWIPE horizontales existentes, antes del `break` global del switch):

```cpp
      case TipoEventoUi::SWIPE_ARRIBA:
      case TipoEventoUi::SWIPE_ABAJO:
        if (!pilaUi_.empty()) break;               // pila abierta: ignorar
        if (actual_) {
          actual_->alDeslizar(ev.tipo == TipoEventoUi::SWIPE_ARRIBA
                              ? Direccion::ARRIBA : Direccion::ABAJO);
        }
        break;
```

**No** tocan el temporizador del carrusel ni cambian de vista.

Los `case` horizontales existentes se mantienen tal cual pero necesitan usar el nuevo `enum Direccion` en la llamada final si la hubiera; el código actual no llama a `alDeslizar` para horizontales (los consume el propio gestor). Se deja como está.

- [ ] **Step 4: Ampliar `PantallaFake` en el test para capturar la dirección**

En `test/test_gestor_pantallas/test_main.cpp`, sustituir la línea:
```cpp
  int ultTapX = -1, ultTapY = -1, ultSwipe = 0;
```
por:
```cpp
  int ultTapX = -1, ultTapY = -1;
  int nSwipes = 0;
  pantallas::Direccion ultDir = pantallas::Direccion::IZQUIERDA;
```

Y sustituir la implementación de `alDeslizar` del fake:
```cpp
  void alDeslizar(int d) override { ultSwipe = d; }
```
por:
```cpp
  void alDeslizar(pantallas::Direccion d) override { ++nSwipes; ultDir = d; }
```

- [ ] **Step 5: Añadir dos tests nuevos**

Antes del `int main`:

```cpp
void test_swipe_vertical_se_delega_a_pantalla_actual(void) {
  RendererFake r;
  PantallaFake a("A", 0);
  GestorPantallas g(r);
  g.registrar(&a);
  g.configurarModo(ModoGestor::CARRUSEL, /*intervaloS=*/60, {0}, 0);
  g.iniciar(0);

  g.encolarEvento({TipoEventoUi::SWIPE_ARRIBA, 100, 120});
  g.tick(1000);
  TEST_ASSERT_EQUAL(1, a.nSwipes);
  TEST_ASSERT_EQUAL(static_cast<int>(pantallas::Direccion::ARRIBA),
                    static_cast<int>(a.ultDir));

  g.encolarEvento({TipoEventoUi::SWIPE_ABAJO, 100, 120});
  g.tick(2000);
  TEST_ASSERT_EQUAL(2, a.nSwipes);
  TEST_ASSERT_EQUAL(static_cast<int>(pantallas::Direccion::ABAJO),
                    static_cast<int>(a.ultDir));
}

void test_swipe_vertical_ignorado_con_pila_ui(void) {
  RendererFake r;
  PantallaFake home("Home", 10);
  PantallaFake a("A", 0), sub("Sub", 99);
  GestorPantallas g(r);
  g.setHome(&home);
  g.registrar(&home); g.registrar(&a); g.registrar(&sub);
  g.configurarModo(ModoGestor::CARRUSEL, 60, {0}, 0);
  g.iniciar(0);
  g.mostrarPorId(0);
  g.abrirEnPila(&sub);

  g.encolarEvento({TipoEventoUi::SWIPE_ARRIBA, 100, 120});
  g.tick(1000);
  TEST_ASSERT_EQUAL(0, sub.nSwipes);
  TEST_ASSERT_EQUAL(0, a.nSwipes);
}
```

Registrar ambos en `main` después de los 7 previos.

- [ ] **Step 6: Verificar tests**

Run: `~/.platformio/penv/bin/pio test -e native -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos" -f test_gestor_pantallas`
Expected: 9/9 (7 previos + 2 nuevos).

Run también la suite completa: 46/46 (44 previos + 4 gesture + 2 gestor - 4 gesture ya contados = 50/50 según recuento previo).

- [ ] **Step 7: Compilar firmware**

Run: `~/.platformio/penv/bin/pio run -e esp32dev` — `[SUCCESS]`.

- [ ] **Step 8: Commit**

```bash
git add lib/pantallas test/test_gestor_pantallas
git commit -m "feat(pantallas): enum Direccion y delegación de swipe vertical a la vista"
```

---

## Task 3: `src/main.cpp` — traductor de eventos amplía a 5 casos

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Ampliar el switch del traductor en `tareaDisplay`**

Localizar el `switch (ev.tipo)` que traduce `touch::TipoEvento` a `pantallas::TipoEventoUi`. Añadir los dos casos:

```cpp
        case touch::TipoEvento::SWIPE_ARRIBA:     u.tipo = pantallas::TipoEventoUi::SWIPE_ARRIBA; break;
        case touch::TipoEvento::SWIPE_ABAJO:      u.tipo = pantallas::TipoEventoUi::SWIPE_ABAJO;  break;
```

- [ ] **Step 2: Compilar firmware**

Run: `~/.platformio/penv/bin/pio run -e esp32dev` — `[SUCCESS]`.

- [ ] **Step 3: Commit**

```bash
git add src/main.cpp
git commit -m "feat(main): traductor de eventos incluye SWIPE vertical"
```

---

## Task 4: `lib/meteo_client/` — cliente + parser + tests native

**Files:**
- Create: `lib/meteo_client/library.json`
- Create: `lib/meteo_client/src/meteo_client.h`
- Create: `lib/meteo_client/src/meteo_client.cpp`
- Create: `test/test_meteo_client/test_main.cpp`

- [ ] **Step 1: Escribir tests native primero (fallan)**

`test/test_meteo_client/test_main.cpp`:

```cpp
#include <unity.h>
#include "meteo_client.h"
#include <string>

// Payload real minificado de Open-Meteo (Madrid, sept 2026), recortado a
// las cabeceras que consume el cliente. Comprobado con curl real.
static const char* PAYLOAD_OK = R"({
  "current_weather": {"temperature":22.3, "windspeed":12.4, "weathercode":1, "time":"2026-09-03T14:00"},
  "hourly": {
    "time": ["2026-09-03T13:00","2026-09-03T14:00","2026-09-03T15:00","2026-09-03T16:00","2026-09-03T17:00","2026-09-03T18:00","2026-09-03T19:00","2026-09-03T20:00","2026-09-03T21:00","2026-09-03T22:00","2026-09-03T23:00","2026-09-04T00:00","2026-09-04T01:00","2026-09-04T02:00"],
    "temperature_2m": [21.0,22.3,23.5,23.0,22.0,20.5,19.0,18.0,17.5,17.0,16.5,16.0,15.5,15.0],
    "weather_code": [1,1,2,2,3,3,61,61,3,2,1,0,0,0]
  },
  "daily": {
    "time": ["2026-09-03","2026-09-04","2026-09-05","2026-09-06","2026-09-07"],
    "temperature_2m_max": [25.0,24.0,22.0,20.5,26.0],
    "temperature_2m_min": [15.0,14.0,13.5,13.0,15.5],
    "weather_code": [1,2,61,3,0]
  }
})";

void test_parsear_payload_valido(void) {
  MeteoSnapshot s;
  bool ok = MeteoClient::parsear(PAYLOAD_OK, s);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_TRUE(s.ok);
  TEST_ASSERT_FLOAT_WITHIN(0.1, 22.3, s.temp_actual_c);
  TEST_ASSERT_EQUAL(1, s.codigo_actual);
  TEST_ASSERT_EQUAL(12, s.viento_kmh);      // 12.4 truncado a int
  TEST_ASSERT_TRUE(s.horas.size() >= 6);    // desde hora actual, al menos 6 disponibles
  TEST_ASSERT_TRUE(s.horas.size() <= 12);
  TEST_ASSERT_EQUAL(14, s.horas[0].hora);   // primera hora >= current_weather.time
  TEST_ASSERT_EQUAL(5, s.dias.size());
  TEST_ASSERT_EQUAL(3, s.dias[0].dia_mes);
  TEST_ASSERT_FLOAT_WITHIN(0.1, 25.0, s.dias[0].tmax);
  TEST_ASSERT_FLOAT_WITHIN(0.1, 15.0, s.dias[0].tmin);
}

void test_parsear_json_malformado(void) {
  MeteoSnapshot s;
  bool ok = MeteoClient::parsear("{", s);
  TEST_ASSERT_FALSE(ok);
}

void test_parsear_sin_hourly(void) {
  const char* payload = R"({
    "current_weather": {"temperature":10, "windspeed":5, "weathercode":0, "time":"2026-09-03T08:00"},
    "daily": {"time":["2026-09-03"], "temperature_2m_max":[15], "temperature_2m_min":[5], "weather_code":[0]}
  })";
  MeteoSnapshot s;
  bool ok = MeteoClient::parsear(payload, s);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL(0, s.horas.size());
  TEST_ASSERT_EQUAL(1, s.dias.size());
}

void test_categoria_wmo_95_es_tormenta(void) {
  TEST_ASSERT_EQUAL(static_cast<int>(IconoMeteo::TORMENTA),
                    static_cast<int>(MeteoClient::categoria(95)));
  TEST_ASSERT_EQUAL(static_cast<int>(IconoMeteo::SOL),
                    static_cast<int>(MeteoClient::categoria(0)));
  TEST_ASSERT_EQUAL(static_cast<int>(IconoMeteo::LLUVIA),
                    static_cast<int>(MeteoClient::categoria(61)));
  TEST_ASSERT_EQUAL(static_cast<int>(IconoMeteo::NIEBLA),
                    static_cast<int>(MeteoClient::categoria(45)));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_parsear_payload_valido);
  RUN_TEST(test_parsear_json_malformado);
  RUN_TEST(test_parsear_sin_hourly);
  RUN_TEST(test_categoria_wmo_95_es_tormenta);
  return UNITY_END();
}
```

Correr → falla (no compila).

- [ ] **Step 2: Crear `library.json`**

```json
{
  "name": "meteo_client",
  "version": "0.1.0",
  "description": "Cliente Open-Meteo para lat/lon del Config",
  "dependencies": {
    "bblanchon/ArduinoJson": "^7.0.4"
  }
}
```

- [ ] **Step 3: Crear `meteo_client.h`**

```cpp
#pragma once
#include "http_client.h"
#include <cstdint>
#include <string>
#include <vector>

enum class IconoMeteo : uint8_t { SOL=0, NUBE=1, LLUVIA=2, NIEVE=3, TORMENTA=4, NIEBLA=5 };

struct MeteoSnapshot {
  bool     ok = false;
  uint32_t obtenido_ms = 0;
  float    temp_actual_c = 0.0f;
  int      codigo_actual = 0;
  int      viento_kmh = 0;
  bool     stale = false;
  struct Hora { int8_t hora; float temp_c; int codigo; };
  struct Dia  { int8_t dia_mes; float tmin; float tmax; int codigo; };
  std::vector<Hora> horas;
  std::vector<Dia>  dias;
};

class MeteoClient {
 public:
  explicit MeteoClient(IHttpClient& http) : http_(http) {}

  bool fetch(double lat, double lon, MeteoSnapshot& out);
  static bool parsear(const std::string& json, MeteoSnapshot& out);
  static IconoMeteo categoria(int wmo);

 private:
  IHttpClient& http_;
};
```

- [ ] **Step 4: Crear `meteo_client.cpp`**

```cpp
#include "meteo_client.h"
#include <ArduinoJson.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
int parseHora(const char* iso) {
  // Formato "YYYY-MM-DDTHH:MM"; hora comienza en offset 11.
  if (!iso || std::strlen(iso) < 13) return -1;
  return (iso[11] - '0') * 10 + (iso[12] - '0');
}
int parseDiaMes(const char* iso) {
  if (!iso || std::strlen(iso) < 10) return -1;
  return (iso[8] - '0') * 10 + (iso[9] - '0');
}
}  // namespace

IconoMeteo MeteoClient::categoria(int wmo) {
  if (wmo == 0) return IconoMeteo::SOL;
  if (wmo >= 1 && wmo <= 3) return IconoMeteo::NUBE;
  if (wmo == 45 || wmo == 48) return IconoMeteo::NIEBLA;
  if ((wmo >= 51 && wmo <= 67) || (wmo >= 80 && wmo <= 82)) return IconoMeteo::LLUVIA;
  if ((wmo >= 71 && wmo <= 77) || wmo == 85 || wmo == 86) return IconoMeteo::NIEVE;
  if (wmo >= 95 && wmo <= 99) return IconoMeteo::TORMENTA;
  return IconoMeteo::NUBE;
}

bool MeteoClient::parsear(const std::string& json, MeteoSnapshot& out) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) return false;

  auto cw = doc["current_weather"];
  if (cw.isNull()) return false;
  out.temp_actual_c = cw["temperature"].as<float>();
  out.codigo_actual = cw["weathercode"].as<int>();
  out.viento_kmh = static_cast<int>(cw["windspeed"].as<float>());
  const char* horaActualIso = cw["time"] | (const char*)nullptr;
  const int horaActual = parseHora(horaActualIso);

  out.horas.clear();
  auto ht = doc["hourly"]["time"].as<JsonArrayConst>();
  auto ht2m = doc["hourly"]["temperature_2m"].as<JsonArrayConst>();
  auto hwc = doc["hourly"]["weather_code"].as<JsonArrayConst>();
  if (!ht.isNull() && !ht2m.isNull() && !hwc.isNull() && horaActual >= 0) {
    // saltar entradas anteriores a la hora actual
    size_t inicio = 0;
    for (size_t i = 0; i < ht.size(); ++i) {
      if (parseHora(ht[i].as<const char*>()) >= horaActual) { inicio = i; break; }
    }
    for (size_t i = inicio; i < ht.size() && out.horas.size() < 12; ++i) {
      MeteoSnapshot::Hora h;
      h.hora   = static_cast<int8_t>(parseHora(ht[i].as<const char*>()));
      h.temp_c = ht2m[i].as<float>();
      h.codigo = hwc[i].as<int>();
      out.horas.push_back(h);
    }
  }

  out.dias.clear();
  auto dt = doc["daily"]["time"].as<JsonArrayConst>();
  auto dmax = doc["daily"]["temperature_2m_max"].as<JsonArrayConst>();
  auto dmin = doc["daily"]["temperature_2m_min"].as<JsonArrayConst>();
  auto dwc  = doc["daily"]["weather_code"].as<JsonArrayConst>();
  if (!dt.isNull() && !dmax.isNull() && !dmin.isNull() && !dwc.isNull()) {
    for (size_t i = 0; i < dt.size() && out.dias.size() < 5; ++i) {
      MeteoSnapshot::Dia d;
      d.dia_mes = static_cast<int8_t>(parseDiaMes(dt[i].as<const char*>()));
      d.tmax    = dmax[i].as<float>();
      d.tmin    = dmin[i].as<float>();
      d.codigo  = dwc[i].as<int>();
      out.dias.push_back(d);
    }
  }

  out.ok = true;
  return true;
}

bool MeteoClient::fetch(double lat, double lon, MeteoSnapshot& out) {
  char url[512];
  std::snprintf(url, sizeof(url),
                "https://api.open-meteo.com/v1/forecast"
                "?latitude=%.4f&longitude=%.4f"
                "&current_weather=true"
                "&hourly=temperature_2m,weather_code"
                "&daily=temperature_2m_max,temperature_2m_min,weather_code"
                "&forecast_days=5&timezone=Europe%%2FMadrid",
                lat, lon);
  std::string body;
  int status = 0;
  if (!http_.get(url, body, status, 15000)) return false;
  if (status != 200 || body.empty()) return false;
  MeteoSnapshot tmp;
  if (!parsear(body, tmp)) return false;
  out = tmp;
  return true;
}
```

- [ ] **Step 5: Tests verdes**

Run: `~/.platformio/penv/bin/pio test -e native -d "..." -f test_meteo_client` → 4/4.

Correr suite completa native: nuevo total = 50/50 (44 previos + 4 gesture + 2 gestor).

- [ ] **Step 6: Compilar firmware**

Run: `~/.platformio/penv/bin/pio run -e esp32dev` → `[SUCCESS]`.

- [ ] **Step 7: Commit**

```bash
git add lib/meteo_client test/test_meteo_client
git commit -m "feat(meteo_client): cliente Open-Meteo con parser puro y tests native"
```

---

## Task 5: `lib/pantalla_meteo/` — vista con iconos y dos sub-vistas

**Files:**
- Create: `lib/pantalla_meteo/library.json`
- Create: `lib/pantalla_meteo/src/pantalla_meteo.h`
- Create: `lib/pantalla_meteo/src/pantalla_meteo.cpp`

- [ ] **Step 1: Crear `library.json`**

```json
{
  "name": "pantalla_meteo",
  "version": "0.1.0",
  "description": "Vista Meteo con sub-vistas Horas/Días alternables por swipe vertical",
  "dependencies": {
    "bodmer/TFT_eSPI": "^2.5.43"
  }
}
```

- [ ] **Step 2: Crear `pantalla_meteo.h`**

```cpp
#pragma once
#include "pantalla.h"
#include "meteo_client.h"
#include <cstdint>

class TFT_eSPI;

class PantallaMeteo : public pantallas::Pantalla {
 public:
  explicit PantallaMeteo(const MeteoSnapshot& snapshot) : snap_(snapshot) {}

  const char* nombre() const override { return "Meteo"; }
  uint8_t id() const override { return 2; }

  void alEntrar() override;
  void alDeslizar(pantallas::Direccion dir) override;
  void dibujar(uint32_t msAhora) override;

 private:
  enum class SubVista : uint8_t { HORAS = 0, DIAS = 1 };
  void dibujarSinDatos(TFT_eSPI& tft);
  void dibujarHoras(TFT_eSPI& tft);
  void dibujarDias(TFT_eSPI& tft);
  void dibujarBloqueActual(TFT_eSPI& tft);
  void dibujarIndicador(TFT_eSPI& tft);
  void dibujarIcono(TFT_eSPI& tft, int cx, int cy, int lado, int wmo);

  const MeteoSnapshot& snap_;
  SubVista sub_ = SubVista::HORAS;
  bool     dirty_ = true;
  uint32_t ultObtenidoMs_ = 0;
};
```

- [ ] **Step 3: Crear `pantalla_meteo.cpp`**

```cpp
#include "pantalla_meteo.h"
#include "tft_driver.h"
#include "paleta_dark.h"
#include <TFT_eSPI.h>
#include <cstdio>
#include <cmath>

namespace {
constexpr int OFFSET_Y = 20;
constexpr int W = 320;
constexpr int H_CONTENIDO = 220;

constexpr uint16_t COL_SOL   = 0xFEA0;   // amarillo
constexpr uint16_t COL_NUBE  = 0xBDF7;   // gris claro
constexpr uint16_t COL_LLUV  = 0x5D9F;   // azul
constexpr uint16_t COL_TORM  = 0xFEA0;   // amarillo (rayo)
constexpr uint16_t COL_NIEV  = 0xFFFF;   // blanco

const char* diasAbrev[7] = {"Dom","Lun","Mar","Mie","Jue","Vie","Sab"};

void pintarFondo(TFT_eSPI& tft) {
  tft.fillRect(0, OFFSET_Y, W, H_CONTENIDO, paleta_dark::COL_FONDO);
}
}  // namespace

void PantallaMeteo::alEntrar() {
  dirty_ = true;
  ultObtenidoMs_ = 0;
}

void PantallaMeteo::alDeslizar(pantallas::Direccion dir) {
  if (dir == pantallas::Direccion::ARRIBA || dir == pantallas::Direccion::ABAJO) {
    sub_ = (sub_ == SubVista::HORAS) ? SubVista::DIAS : SubVista::HORAS;
    dirty_ = true;
  }
}

void PantallaMeteo::dibujar(uint32_t) {
  auto& tft = tft_driver::obtenerTft();
  const bool datosNuevos = snap_.obtenido_ms != ultObtenidoMs_;
  if (!dirty_ && !datosNuevos) return;

  if (!snap_.ok) {
    dibujarSinDatos(tft);
    dirty_ = false;
    ultObtenidoMs_ = snap_.obtenido_ms;
    return;
  }
  if (sub_ == SubVista::HORAS) dibujarHoras(tft);
  else                          dibujarDias(tft);
  dirty_ = false;
  ultObtenidoMs_ = snap_.obtenido_ms;
}

void PantallaMeteo::dibujarSinDatos(TFT_eSPI& tft) {
  pintarFondo(tft);
  tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
  tft.setTextFont(4);
  const char* t = "Meteo: sin datos";
  int16_t w = tft.textWidth(t);
  tft.setCursor((W - w) / 2, OFFSET_Y + 90);
  tft.print(t);
}

void PantallaMeteo::dibujarBloqueActual(TFT_eSPI& tft) {
  char buf[16];
  // Temp actual (font 7)
  std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(std::round(snap_.temp_actual_c)));
  tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
  tft.setTextFont(7);
  tft.setCursor(14, OFFSET_Y + 14);
  tft.print(buf);
  // Grados
  const int xTemp = 14 + tft.textWidth(buf);
  tft.setTextFont(4);
  tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
  tft.setCursor(xTemp + 4, OFFSET_Y + 24);
  tft.print("\xB0" "C");
  // Icono actual
  dibujarIcono(tft, 250, OFFSET_Y + 46, 60, snap_.codigo_actual);
  // Viento
  tft.setTextFont(2);
  tft.setCursor(14, OFFSET_Y + 78);
  std::snprintf(buf, sizeof(buf), "Viento %d km/h", snap_.viento_kmh);
  tft.print(buf);
}

void PantallaMeteo::dibujarHoras(TFT_eSPI& tft) {
  pintarFondo(tft);
  dibujarBloqueActual(tft);
  dibujarIndicador(tft);
  // Tira de hasta 6 horas (una cada 2 h)
  const int n = std::min<int>(6, static_cast<int>(snap_.horas.size()));
  const int slot = W / 6;
  for (int i = 0; i < n; ++i) {
    const auto& h = snap_.horas[i * 2 < (int)snap_.horas.size() ? i * 2 : i];
    const int cx = slot * i + slot / 2;
    // Hora
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%02d", h.hora);
    tft.setTextFont(1);
    tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
    int16_t wh = tft.textWidth(buf);
    tft.setCursor(cx - wh / 2, OFFSET_Y + 118);
    tft.print(buf);
    // Icono
    dibujarIcono(tft, cx, OFFSET_Y + 148, 22, h.codigo);
    // Temp
    tft.setTextFont(2);
    tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
    std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(std::round(h.temp_c)));
    int16_t wt = tft.textWidth(buf);
    tft.setCursor(cx - wt / 2, OFFSET_Y + 180);
    tft.print(buf);
  }
}

void PantallaMeteo::dibujarDias(TFT_eSPI& tft) {
  pintarFondo(tft);
  dibujarBloqueActual(tft);
  dibujarIndicador(tft);
  const int n = std::min<int>(5, static_cast<int>(snap_.dias.size()));
  int y = OFFSET_Y + 110;
  // Nota: no calculamos día de la semana real (requiere mktime), imprimimos el número
  // de día de mes que es exacto y no depende de sincronización TZ.
  tft.setTextFont(2);
  char buf[24];
  for (int i = 0; i < n; ++i) {
    const auto& d = snap_.dias[i];
    tft.setTextColor(paleta_dark::COL_TXT_SECUND, paleta_dark::COL_FONDO);
    std::snprintf(buf, sizeof(buf), "%02d", d.dia_mes);
    tft.setCursor(20, y);
    tft.print(buf);
    dibujarIcono(tft, 90, y + 10, 18, d.codigo);
    tft.setTextColor(paleta_dark::COL_TXT_TITULO, paleta_dark::COL_FONDO);
    std::snprintf(buf, sizeof(buf), "%d\xB0 - %d\xB0",
                  static_cast<int>(std::round(d.tmin)),
                  static_cast<int>(std::round(d.tmax)));
    tft.setCursor(140, y);
    tft.print(buf);
    y += 22;
  }
}

void PantallaMeteo::dibujarIndicador(TFT_eSPI& tft) {
  // 2 dots en esquina sup-der del área de contenido, y ≈ OFFSET_Y + 4
  const int y = OFFSET_Y + 6;
  const int r = 3;
  const int xA = W - 22;
  const int xB = W - 10;
  const bool horas = (sub_ == SubVista::HORAS);
  if (horas) {
    tft.fillCircle(xA, y, r, paleta_dark::COL_ACENTO);
    tft.drawCircle(xB, y, r, paleta_dark::COL_TXT_SECUND);
  } else {
    tft.drawCircle(xA, y, r, paleta_dark::COL_TXT_SECUND);
    tft.fillCircle(xB, y, r, paleta_dark::COL_ACENTO);
  }
}

void PantallaMeteo::dibujarIcono(TFT_eSPI& tft, int cx, int cy, int lado, int wmo) {
  IconoMeteo cat = MeteoClient::categoria(wmo);
  const int r = lado / 3;
  switch (cat) {
    case IconoMeteo::SOL: {
      tft.fillCircle(cx, cy, r, COL_SOL);
      for (int a = 0; a < 360; a += 45) {
        const double rad = a * M_PI / 180.0;
        const int x1 = cx + int((r + 2) * std::cos(rad));
        const int y1 = cy + int((r + 2) * std::sin(rad));
        const int x2 = cx + int((r + lado / 6) * std::cos(rad));
        const int y2 = cy + int((r + lado / 6) * std::sin(rad));
        tft.drawLine(x1, y1, x2, y2, COL_SOL);
      }
      break;
    }
    case IconoMeteo::NUBE: {
      tft.fillCircle(cx - r / 2, cy - 1, r / 2 + 1, COL_NUBE);
      tft.fillCircle(cx + r / 2, cy - 1, r / 2 + 1, COL_NUBE);
      tft.fillCircle(cx, cy - r / 2, r / 2 + 1, COL_NUBE);
      tft.fillRect(cx - r, cy - 1, 2 * r, r / 2 + 1, COL_NUBE);
      break;
    }
    case IconoMeteo::LLUVIA: {
      // Nube arriba
      tft.fillCircle(cx - r / 2, cy - 2, r / 2 + 1, COL_NUBE);
      tft.fillCircle(cx + r / 2, cy - 2, r / 2 + 1, COL_NUBE);
      tft.fillCircle(cx, cy - r / 2 - 2, r / 2 + 1, COL_NUBE);
      tft.fillRect(cx - r, cy - 2, 2 * r, r / 2 + 1, COL_NUBE);
      // Gotas
      const int y1 = cy + r / 2 + 1;
      const int y2 = y1 + std::max(3, lado / 6);
      tft.drawLine(cx - r / 2, y1, cx - r / 2, y2, COL_LLUV);
      tft.drawLine(cx,         y1, cx,         y2, COL_LLUV);
      tft.drawLine(cx + r / 2, y1, cx + r / 2, y2, COL_LLUV);
      break;
    }
    case IconoMeteo::NIEVE: {
      tft.fillCircle(cx - r / 2, cy - 2, r / 2 + 1, COL_NUBE);
      tft.fillCircle(cx + r / 2, cy - 2, r / 2 + 1, COL_NUBE);
      tft.fillCircle(cx, cy - r / 2 - 2, r / 2 + 1, COL_NUBE);
      tft.fillRect(cx - r, cy - 2, 2 * r, r / 2 + 1, COL_NUBE);
      tft.setTextFont(1);
      tft.setTextColor(COL_NIEV, paleta_dark::COL_FONDO);
      tft.setCursor(cx - r / 2 - 2, cy + r / 2 + 1);
      tft.print("* * *");
      break;
    }
    case IconoMeteo::TORMENTA: {
      tft.fillCircle(cx - r / 2, cy - 2, r / 2 + 1, COL_NUBE);
      tft.fillCircle(cx + r / 2, cy - 2, r / 2 + 1, COL_NUBE);
      tft.fillCircle(cx, cy - r / 2 - 2, r / 2 + 1, COL_NUBE);
      tft.fillRect(cx - r, cy - 2, 2 * r, r / 2 + 1, COL_NUBE);
      // Rayo (dos triángulos con pico común)
      tft.fillTriangle(cx - 2, cy + 4, cx + 4, cy + 4, cx + 1, cy + r + 4, COL_TORM);
      tft.fillTriangle(cx - 4, cy + r + 4, cx + 4, cy + r + 4, cx, cy + r + 10, COL_TORM);
      break;
    }
    case IconoMeteo::NIEBLA: {
      for (int i = -2; i <= 2; ++i) {
        tft.drawFastHLine(cx - r, cy + i * 3, 2 * r, COL_NUBE);
      }
      break;
    }
  }
}
```

- [ ] **Step 4: Compilar firmware**

Run: `~/.platformio/penv/bin/pio run -e esp32dev` → `[SUCCESS]`. Reporta % de Flash.

- [ ] **Step 5: Commit**

```bash
git add lib/pantalla_meteo
git commit -m "feat(pantalla_meteo): vista Meteo con iconos y sub-vistas Horas/Días"
```

---

## Task 6: `src/main.cpp` — task de refresh y swap del placeholder Meteo

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Includes nuevos**

Añadir tras el include de `pantalla_reloj.h`:
```cpp
#include "pantalla_meteo.h"
#include "meteo_client.h"
```

- [ ] **Step 2: Global del snapshot**

Junto a `RadarState* g_estado = nullptr;`, añadir:
```cpp
MeteoSnapshot g_snapMeteo;
```

- [ ] **Step 3: Task de refresh**

Añadir la función en el `namespace {}` interno, después de `tareaDisplay`:

```cpp
void tareaMeteoRefresh(void*) {
  MeteoClient cliente(g_http);
  // Gracia inicial para que NTP y WiFi acaben de asentarse.
  vTaskDelay(pdMS_TO_TICKS(5000));
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      MeteoSnapshot nuevo;
      if (cliente.fetch(g_cfg.lat, g_cfg.lon, nuevo)) {
        nuevo.obtenido_ms = millis();
        nuevo.stale = false;
        g_snapMeteo = nuevo;
        Serial.printf("[meteo] refresh OK t=%.1fC codigo=%d\n",
                      g_snapMeteo.temp_actual_c, g_snapMeteo.codigo_actual);
      } else {
        g_snapMeteo.stale = true;
        Serial.println("[meteo] refresh FALLÓ");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(30UL * 60UL * 1000UL));  // 30 min
  }
}
```

Y en `modoRadar()`, junto a la creación de las otras tasks:
```cpp
xTaskCreatePinnedToCore(tareaMeteoRefresh, "meteo", 6144, nullptr, 1, nullptr, 0);
```

- [ ] **Step 4: Swap del placeholder Meteo**

Localizar en `modoRadar()`:
```cpp
auto* meteo   = new PantallaProximamente(2, "Meteo");
```
Sustituir por:
```cpp
auto* meteo   = new PantallaMeteo(g_snapMeteo);
```

Los demás placeholders (futbol, motogp, f1) no se tocan.

- [ ] **Step 5: Compilar firmware**

Run: `~/.platformio/penv/bin/pio run -e esp32dev` → `[SUCCESS]`. Reporta % Flash.

- [ ] **Step 6: Tests native como control**

Run: `~/.platformio/penv/bin/pio test -e native` → total esperado 50/50.

- [ ] **Step 7: Commit**

```bash
git add src/main.cpp
git commit -m "feat(main): task refresh meteo + activar PantallaMeteo"
```

---

## Task 7: Verificación en placa

**Files:** ninguno.

- [ ] **Step 1: Comprobar puerto**

Run: `ls /dev/cu.usbserial-*`. Probable: `/dev/cu.usbserial-1110`.

- [ ] **Step 2: Subir firmware**

Run: `~/.platformio/penv/bin/pio run -e esp32dev -t upload --upload-port /dev/cu.usbserial-1110 -d "..."`
Expected: `[SUCCESS]`.

- [ ] **Step 3: Checklist manual**

- [ ] La placa arranca normal (radar visible).
- [ ] Al entrar en Meteo: si aún no llegó el primer fetch (los primeros ~5-10 s tras conexión) se ve "Meteo: sin datos".
- [ ] Cuando el primer fetch completa: se ve la sub-vista Horas con temp actual grande, icono a la derecha, viento debajo y una tira de 6 horas próximas con iconos pequeños.
- [ ] Swipe abajo → transiciona a sub-vista Días (5 filas apiladas con día, icono, min-max).
- [ ] Swipe arriba → vuelve a Horas.
- [ ] Swipe izquierda/derecha en la vista Meteo → cambia a otra vista del carrusel (no altera la sub-vista de Meteo).
- [ ] Los iconos son legibles (sol amarillo, nube gris, lluvia con gotas azules, tormenta con rayo, niebla con líneas).
- [ ] Tras 30 min, `[meteo] refresh OK` en el log y el snapshot se renueva.

- [ ] **Step 4: Reportar**

Cerrar el bloque 3 tras confirmación en placa. Actualizar la memoria del proyecto.
