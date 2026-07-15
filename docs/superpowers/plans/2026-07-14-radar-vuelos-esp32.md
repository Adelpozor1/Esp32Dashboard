# Radar de vuelos ESP32 — Plan de implementación

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Firmware ESP32 modular (Arduino + PlatformIO) que expone un radar polar de aviones cercanos vía web, con portal WiFi de primer arranque y geocoding de la ubicación por dirección postal.

**Architecture:** Módulos independientes con interfaces limpias. Cliente HTTP abstraído tras `IHttpClient` para permitir tests unitarios en `native` con `MockHttpClient`. Dos tasks FreeRTOS (poller ADSB.lol en core 0, web server en core 1) compartiendo `RadarState` protegido con mutex. Portal WiFi como AP separado activado por ausencia de config o fallo persistente de WiFi.

**Tech Stack:** ESP32 dev board (D0WD-V3, 4 MB flash, CH340 USB), PlatformIO `platform=espressif32` + `framework=arduino`, LittleFS para servir frontend, ESPAsyncWebServer + AsyncTCP, ArduinoJson v7 para parseo streaming, NVS para persistencia, Unity + ArduinoFake para tests en `native`.

**Spec:** `docs/superpowers/specs/2026-07-14-radar-vuelos-esp32-design.md`

---

## Corrección de convención (post-Task 1)

Durante Task 1 se descubrió que el layout `src/` con `main.cpp` incluyendo `Arduino.h` rompe la compilación del entorno `native` (`main.cpp` se arrastra y falla por dependencias de Arduino). La convención efectiva para módulos testeables es:

- **Cada módulo va en `lib/<nombre>/src/`** (patrón idiomático PlatformIO — el LDF lo descubre automáticamente en cualquier env).
- Los includes siguen siendo `#include "modulo.h"` sin cambios (el LDF resuelve rutas).
- `main.cpp` se queda en `src/` (solo se compila para `esp32dev`).
- **ArduinoFake se descarta** — no compila con clang moderno de macOS y ninguno de los tests lo usa realmente (todos con `std::string`/`std::vector` puros).

**Aplicar a partir de Task 2:** cada vez que el plan diga `src/<modulo>.{h,cpp}`, leer como `lib/<modulo>/src/<modulo>.{h,cpp}`. Excepción: `src/main.cpp` (se queda donde está).

---

## Estructura de archivos

**Archivos a crear:**

```
platformio.ini                              # env esp32dev + env native + lib_deps + LittleFS
partitions_custom.csv                       # OPCIONAL: si el default no acomoda LittleFS + OTA-less

src/main.cpp                                # (sobrescribir) orquestador
src/config_store.h                          # Config struct + interfaz ConfigStore
src/config_store.cpp                        # implementación con Preferences (NVS)
src/geo_math.h                              # haversine + bearing (funciones libres)
src/geo_math.cpp
src/http_client.h                           # interfaz IHttpClient
src/http_client.cpp                         # implementación real WifiHttpClient
src/geocoder.h                              # Geocoder (usa IHttpClient)
src/geocoder.cpp
src/adsb_client.h                           # Aircraft + AdsbClient (usa IHttpClient)
src/adsb_client.cpp
src/radar_state.h                           # Snapshot + RadarState (thread-safe)
src/radar_state.cpp
src/wifi_portal.h                           # WifiPortal
src/wifi_portal.cpp
src/web_server.h                            # RadarWebServer (rutas HTTP Modo Radar)
src/web_server.cpp
src/status_led.h                            # StatusLed enum + funciones
src/status_led.cpp

data/index.html                             # radar + tabla
data/config.html                            # reconfig
data/portal.html                            # primer arranque
data/app.js                                 # frontend logic
data/style.css

test/mocks/mock_http_client.h               # MockHttpClient (compartido por tests)
test/test_geo_math/test_main.cpp
test/test_geocoder/test_main.cpp
test/test_adsb_client/test_main.cpp
test/test_config_store/test_main.cpp
```

**Archivos a modificar:**

- `platformio.ini` — completar con dependencias, filesystem, envs
- `src/main.cpp` — reemplazar template por orquestador
- `.gitignore` — añadir `data/*.gz` si en el futuro precomprimimos

**Notas de convenciones:**

- **Todo en español:** identificadores, comentarios, mensajes de log, textos de UI.
- **Idioma del código:** aunque los identificadores estén en español (`Config`, `Aeronave`), mantenemos nombres de librerías, tipos estándar (`std::vector`, `String`) y APIs de Arduino en su idioma original.
- **Estilo de commits:** `<tipo>: <descripción corta en español>`. Tipos: `feat`, `fix`, `refactor`, `test`, `docs`, `chore`.
- **TDD:** para módulos puros (`GeoMath`, `Geocoder`, `AdsbClient`, `ConfigStore`) — test primero, ver fallo, implementar mínimo, ver pasar, commit. Los módulos con hardware (`WifiPortal`, `WebServer`, `StatusLed`, `WifiHttpClient` real) se validan manualmente con la placa.
- **Interfaces internas** usan `std::string`, `double`, `int` (portables). Los adaptadores en los bordes convierten a `String` de Arduino cuando lo pide la API. `ArduinoFake` en tests provee `String` stub para poder usar structs tal cual.

---

## Task 0: Preparar `platformio.ini` con envs y dependencias

**Files:**
- Modify: `platformio.ini`

- [ ] **Step 1: Sobrescribir `platformio.ini`**

Reemplaza el contenido de `platformio.ini` con:

```ini
; PlatformIO Project Configuration File — Radar de vuelos
; https://docs.platformio.org/page/projectconf.html

[platformio]
default_envs = esp32dev

; ------------- entorno para la placa -------------
[env:esp32dev]
platform      = espressif32@^6.5.0
board         = esp32dev
framework     = arduino
monitor_speed = 115200
board_build.filesystem = littlefs
lib_deps =
  bblanchon/ArduinoJson@^7.0.4
  esphome/ESPAsyncWebServer-esphome@^3.2.2
  esphome/AsyncTCP-esphome@^2.1.4
build_flags =
  -DCORE_DEBUG_LEVEL=3           ; INFO
  -std=gnu++17
build_unflags =
  -std=gnu++11

; ------------- entorno para tests en desktop -------------
[env:native]
platform  = native
test_framework = unity
build_flags =
  -std=gnu++17
  -DUNIT_TEST
  -I src
lib_deps =
  fabiobatsilva/ArduinoFake@^0.4.0
  bblanchon/ArduinoJson@^7.0.4
test_ignore =
```

- [ ] **Step 2: Verificar que PlatformIO parsea la config sin errores**

Run:
```
~/.platformio/penv/bin/pio project config --path .
```
Expected: imprime el JSON de la config sin errores.

- [ ] **Step 3: Commit**

```
git add platformio.ini
git commit -m "chore: configurar entornos esp32dev y native + dependencias"
```

---

## Task 1: GeoMath — haversine y bearing (TDD puro)

**Files:**
- Create: `src/geo_math.h`, `src/geo_math.cpp`
- Test: `test/test_geo_math/test_main.cpp`

- [ ] **Step 1: Escribir el test (falla porque no existe la unidad)**

Crea `test/test_geo_math/test_main.cpp`:

```cpp
#include <unity.h>
#include "geo_math.h"
#include <cmath>

// Madrid (Puerta del Sol) → Barcelona (Sagrada Familia): ~505 km, bearing ~62°
constexpr double MAD_LAT = 40.4168;
constexpr double MAD_LON = -3.7038;
constexpr double BCN_LAT = 41.4036;
constexpr double BCN_LON = 2.1744;

void test_distancia_madrid_barcelona(void) {
  double d = geo::distanciaKm(MAD_LAT, MAD_LON, BCN_LAT, BCN_LON);
  TEST_ASSERT_FLOAT_WITHIN(10.0, 505.0, d);
}

void test_distancia_punto_sobre_si_mismo(void) {
  double d = geo::distanciaKm(MAD_LAT, MAD_LON, MAD_LAT, MAD_LON);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0, d);
}

void test_bearing_norte_puro(void) {
  int b = geo::bearingGrados(0.0, 0.0, 1.0, 0.0);
  TEST_ASSERT_INT_WITHIN(1, 0, b);
}

void test_bearing_este_puro(void) {
  int b = geo::bearingGrados(0.0, 0.0, 0.0, 1.0);
  TEST_ASSERT_INT_WITHIN(1, 90, b);
}

void test_bearing_sur_puro(void) {
  int b = geo::bearingGrados(0.0, 0.0, -1.0, 0.0);
  TEST_ASSERT_INT_WITHIN(1, 180, b);
}

void test_bearing_oeste_puro(void) {
  int b = geo::bearingGrados(0.0, 0.0, 0.0, -1.0);
  TEST_ASSERT_INT_WITHIN(1, 270, b);
}

void test_bearing_madrid_barcelona(void) {
  int b = geo::bearingGrados(MAD_LAT, MAD_LON, BCN_LAT, BCN_LON);
  TEST_ASSERT_INT_WITHIN(3, 62, b);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_distancia_madrid_barcelona);
  RUN_TEST(test_distancia_punto_sobre_si_mismo);
  RUN_TEST(test_bearing_norte_puro);
  RUN_TEST(test_bearing_este_puro);
  RUN_TEST(test_bearing_sur_puro);
  RUN_TEST(test_bearing_oeste_puro);
  RUN_TEST(test_bearing_madrid_barcelona);
  return UNITY_END();
}
```

- [ ] **Step 2: Ejecutar test y verificar que falla por unidad ausente**

Run:
```
~/.platformio/penv/bin/pio test -e native -f test_geo_math
```
Expected: FAIL — `fatal error: geo_math.h: No such file or directory`.

- [ ] **Step 3: Implementar `geo_math.h`**

Crea `src/geo_math.h`:

```cpp
#pragma once

namespace geo {

// Distancia great-circle entre dos puntos (fórmula haversine), en kilómetros.
double distanciaKm(double lat1, double lon1, double lat2, double lon2);

// Rumbo inicial desde (lat1,lon1) hacia (lat2,lon2), en grados 0..359 (0 = norte).
int bearingGrados(double lat1, double lon1, double lat2, double lon2);

}  // namespace geo
```

- [ ] **Step 4: Implementar `geo_math.cpp`**

Crea `src/geo_math.cpp`:

```cpp
#include "geo_math.h"
#include <cmath>

namespace geo {

static constexpr double R_TIERRA_KM = 6371.0;
static constexpr double PI = 3.14159265358979323846;

static inline double aRadianes(double grados) {
  return grados * PI / 180.0;
}

static inline double aGrados(double radianes) {
  return radianes * 180.0 / PI;
}

double distanciaKm(double lat1, double lon1, double lat2, double lon2) {
  double dLat = aRadianes(lat2 - lat1);
  double dLon = aRadianes(lon2 - lon1);
  double a = std::sin(dLat / 2.0) * std::sin(dLat / 2.0) +
             std::cos(aRadianes(lat1)) * std::cos(aRadianes(lat2)) *
             std::sin(dLon / 2.0) * std::sin(dLon / 2.0);
  double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
  return R_TIERRA_KM * c;
}

int bearingGrados(double lat1, double lon1, double lat2, double lon2) {
  double phi1 = aRadianes(lat1);
  double phi2 = aRadianes(lat2);
  double dLambda = aRadianes(lon2 - lon1);
  double y = std::sin(dLambda) * std::cos(phi2);
  double x = std::cos(phi1) * std::sin(phi2) -
             std::sin(phi1) * std::cos(phi2) * std::cos(dLambda);
  double theta = std::atan2(y, x);
  double grados = std::fmod(aGrados(theta) + 360.0, 360.0);
  return static_cast<int>(std::round(grados)) % 360;
}

}  // namespace geo
```

- [ ] **Step 5: Ejecutar tests y verificar que pasan**

Run:
```
~/.platformio/penv/bin/pio test -e native -f test_geo_math
```
Expected: `7 Tests 0 Failures 0 Ignored`.

- [ ] **Step 6: Commit**

```
git add src/geo_math.h src/geo_math.cpp test/test_geo_math/
git commit -m "feat: implementar geo_math con haversine y bearing (TDD)"
```

---

## Task 2: IHttpClient + MockHttpClient (para poder testear módulos que hacen HTTP)

**Files:**
- Create: `src/http_client.h`, `test/mocks/mock_http_client.h`

- [ ] **Step 1: Definir la interfaz `IHttpClient`**

Crea `src/http_client.h`:

```cpp
#pragma once
#include <string>

class IHttpClient {
 public:
  virtual ~IHttpClient() = default;

  // GET síncrono. Rellena `bodyOut` con la respuesta y `statusOut` con el código HTTP.
  // Devuelve true si el request se completó (con cualquier código); false si hubo timeout
  // o error de conexión.
  virtual bool get(const std::string& url,
                   std::string& bodyOut,
                   int& statusOut,
                   int timeoutMs = 5000) = 0;
};
```

- [ ] **Step 2: Crear el mock (usable por todos los tests)**

Crea `test/mocks/mock_http_client.h`:

```cpp
#pragma once
#include "http_client.h"
#include <string>
#include <vector>
#include <utility>

// Mock deterministico: se le programa una lista de (url_substring, body, status) y
// cada get() busca la primera entrada cuyo substring aparezca en la url solicitada.
class MockHttpClient : public IHttpClient {
 public:
  struct Respuesta {
    std::string urlSubstring;
    std::string body;
    int status;
    bool exito;   // false → simula fallo de conexión / timeout
  };

  std::vector<Respuesta> respuestas;
  std::vector<std::string> urlsLlamadas;

  bool get(const std::string& url,
           std::string& bodyOut,
           int& statusOut,
           int /*timeoutMs*/ = 5000) override {
    urlsLlamadas.push_back(url);
    for (const auto& r : respuestas) {
      if (url.find(r.urlSubstring) != std::string::npos) {
        bodyOut = r.body;
        statusOut = r.status;
        return r.exito;
      }
    }
    bodyOut = "";
    statusOut = 0;
    return false;
  }
};
```

- [ ] **Step 3: Añadir include path para los mocks en `platformio.ini`**

Modificar el `[env:native]` en `platformio.ini` para que encuentre `test/mocks/`:

```ini
[env:native]
platform  = native
test_framework = unity
build_flags =
  -std=gnu++17
  -DUNIT_TEST
  -I src
  -I test/mocks
lib_deps =
  fabiobatsilva/ArduinoFake@^0.4.0
  bblanchon/ArduinoJson@^7.0.4
test_ignore =
```

- [ ] **Step 4: Verificar que compila (aún sin tests que lo usen)**

Run:
```
~/.platformio/penv/bin/pio test -e native -f test_geo_math
```
Expected: sigue pasando `7 Tests 0 Failures`.

- [ ] **Step 5: Commit**

```
git add src/http_client.h test/mocks/mock_http_client.h platformio.ini
git commit -m "feat: interfaz IHttpClient + MockHttpClient para tests"
```

---

## Task 3: Geocoder (usa IHttpClient + Nominatim, TDD con mock)

**Files:**
- Create: `src/geocoder.h`, `src/geocoder.cpp`
- Test: `test/test_geocoder/test_main.cpp`

- [ ] **Step 1: Escribir tests con MockHttpClient**

Crea `test/test_geocoder/test_main.cpp`:

```cpp
#include <unity.h>
#include "geocoder.h"
#include "mock_http_client.h"

// Respuesta típica de Nominatim con resultado
static const char* RESP_MADRID = R"([
  {
    "place_id": 1,
    "lat": "40.4167047",
    "lon": "-3.7035825",
    "display_name": "Madrid, España"
  }
])";

// Respuesta vacía (dirección no encontrada)
static const char* RESP_VACIA = R"([])";

// Respuesta malformada
static const char* RESP_MAL = "esto no es json";

void test_geocoding_direccion_encontrada(void) {
  MockHttpClient http;
  http.respuestas.push_back({"nominatim", RESP_MADRID, 200, true});
  Geocoder g(http);
  double lat = 0, lon = 0;
  bool ok = g.resolver("Madrid, España", lat, lon);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 40.4167, lat);
  TEST_ASSERT_FLOAT_WITHIN(0.01, -3.7036, lon);
}

void test_geocoding_direccion_no_encontrada(void) {
  MockHttpClient http;
  http.respuestas.push_back({"nominatim", RESP_VACIA, 200, true});
  Geocoder g(http);
  double lat = 99, lon = 99;
  bool ok = g.resolver("Asdfghjkl", lat, lon);
  TEST_ASSERT_FALSE(ok);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 99.0, lat);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 99.0, lon);
}

void test_geocoding_respuesta_malformada(void) {
  MockHttpClient http;
  http.respuestas.push_back({"nominatim", RESP_MAL, 200, true});
  Geocoder g(http);
  double lat = 0, lon = 0;
  bool ok = g.resolver("Cualquier cosa", lat, lon);
  TEST_ASSERT_FALSE(ok);
}

void test_geocoding_fallo_http(void) {
  MockHttpClient http;
  http.respuestas.push_back({"nominatim", "", 0, false});
  Geocoder g(http);
  double lat = 0, lon = 0;
  bool ok = g.resolver("Madrid", lat, lon);
  TEST_ASSERT_FALSE(ok);
}

void test_geocoding_incluye_direccion_url_encoded(void) {
  MockHttpClient http;
  http.respuestas.push_back({"nominatim", RESP_MADRID, 200, true});
  Geocoder g(http);
  double lat = 0, lon = 0;
  g.resolver("Calle Mayor 1, Madrid", lat, lon);
  TEST_ASSERT_EQUAL(1u, http.urlsLlamadas.size());
  const std::string& url = http.urlsLlamadas[0];
  TEST_ASSERT_TRUE(url.find("Calle%20Mayor%201") != std::string::npos ||
                   url.find("Calle+Mayor+1") != std::string::npos);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_geocoding_direccion_encontrada);
  RUN_TEST(test_geocoding_direccion_no_encontrada);
  RUN_TEST(test_geocoding_respuesta_malformada);
  RUN_TEST(test_geocoding_fallo_http);
  RUN_TEST(test_geocoding_incluye_direccion_url_encoded);
  return UNITY_END();
}
```

- [ ] **Step 2: Verificar que los tests fallan por unidad ausente**

Run:
```
~/.platformio/penv/bin/pio test -e native -f test_geocoder
```
Expected: FAIL — `fatal error: geocoder.h: No such file or directory`.

- [ ] **Step 3: Implementar `geocoder.h`**

Crea `src/geocoder.h`:

```cpp
#pragma once
#include "http_client.h"
#include <string>

class Geocoder {
 public:
  explicit Geocoder(IHttpClient& http) : http_(http) {}

  // Resuelve una dirección postal a coordenadas usando Nominatim (OpenStreetMap).
  // Devuelve true si encontró resultado, false si no o hubo error de red/parseo.
  // No modifica latOut/lonOut si devuelve false.
  bool resolver(const std::string& direccion, double& latOut, double& lonOut);

 private:
  IHttpClient& http_;
};
```

- [ ] **Step 4: Implementar `geocoder.cpp`**

Crea `src/geocoder.cpp`:

```cpp
#include "geocoder.h"
#include <ArduinoJson.h>
#include <cctype>
#include <cstdio>

namespace {

std::string urlEncode(const std::string& in) {
  std::string out;
  out.reserve(in.size() * 3);
  char buf[4];
  for (unsigned char c : in) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out.push_back(c);
    } else if (c == ' ') {
      out += "%20";
    } else {
      std::snprintf(buf, sizeof(buf), "%%%02X", c);
      out += buf;
    }
  }
  return out;
}

}  // namespace

bool Geocoder::resolver(const std::string& direccion, double& latOut, double& lonOut) {
  std::string url = "https://nominatim.openstreetmap.org/search?format=json&limit=1&q=";
  url += urlEncode(direccion);

  std::string body;
  int status = 0;
  if (!http_.get(url, body, status, 5000) || status != 200 || body.empty()) {
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) return false;
  if (!doc.is<JsonArray>() || doc.size() == 0) return false;

  auto primero = doc[0];
  const char* latStr = primero["lat"];
  const char* lonStr = primero["lon"];
  if (!latStr || !lonStr) return false;

  char* endLat = nullptr;
  char* endLon = nullptr;
  double lat = std::strtod(latStr, &endLat);
  double lon = std::strtod(lonStr, &endLon);
  if (endLat == latStr || endLon == lonStr) return false;

  latOut = lat;
  lonOut = lon;
  return true;
}
```

- [ ] **Step 5: Ejecutar tests y verificar que pasan**

Run:
```
~/.platformio/penv/bin/pio test -e native -f test_geocoder
```
Expected: `5 Tests 0 Failures 0 Ignored`.

- [ ] **Step 6: Commit**

```
git add src/geocoder.h src/geocoder.cpp test/test_geocoder/
git commit -m "feat: implementar Geocoder con Nominatim (TDD)"
```

---

## Task 4: AdsbClient (parseo respuestas ADSB.lol, TDD)

**Files:**
- Create: `src/adsb_client.h`, `src/adsb_client.cpp`
- Test: `test/test_adsb_client/test_main.cpp`

- [ ] **Step 1: Escribir tests con MockHttpClient**

Crea `test/test_adsb_client/test_main.cpp`:

```cpp
#include <unity.h>
#include "adsb_client.h"
#include "mock_http_client.h"

// Muestra reducida de la respuesta de ADSB.lol /v2/point/{lat}/{lon}/{radio}
// Estructura real: { "ac": [ {...}, {...} ], "msg": "No error", ... }
static const char* RESP_DOS_AVIONES = R"({
  "ac": [
    {
      "hex": "4b1806",
      "flight": "IBE3456 ",
      "lat": 40.45,
      "lon": -3.68,
      "alt_baro": 8500,
      "gs": 320.5,
      "track": 85.2
    },
    {
      "hex": "406b1a",
      "flight": "BAW789  ",
      "lat": 40.30,
      "lon": -3.60,
      "alt_baro": 12000,
      "gs": 410,
      "track": 190
    }
  ]
})";

static const char* RESP_VACIA = R"({"ac":[]})";
static const char* RESP_MAL = "no es json";

// Punto de referencia: Madrid Puerta del Sol.
constexpr double CENTRO_LAT = 40.4168;
constexpr double CENTRO_LON = -3.7038;

void test_adsb_parseo_normal(void) {
  MockHttpClient http;
  http.respuestas.push_back({"adsb.lol", RESP_DOS_AVIONES, 200, true});
  AdsbClient c(http);
  std::vector<Aeronave> aviones;
  bool ok = c.fetchCerca(CENTRO_LAT, CENTRO_LON, 25, aviones);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL(2u, aviones.size());
  TEST_ASSERT_EQUAL_STRING("4b1806", aviones[0].hex.c_str());
  TEST_ASSERT_EQUAL_STRING("IBE3456", aviones[0].callsign.c_str());   // trimmed
  TEST_ASSERT_EQUAL(8500, aviones[0].alt_ft);
  TEST_ASSERT_EQUAL(320, aviones[0].gs_kt);
  TEST_ASSERT_EQUAL(85, aviones[0].track_deg);
  TEST_ASSERT_FLOAT_WITHIN(1.0, 4.5, aviones[0].dist_km);   // aproximado
  TEST_ASSERT_INT_WITHIN(15, 66, aviones[0].bearing);       // NE aprox.
}

void test_adsb_respuesta_vacia(void) {
  MockHttpClient http;
  http.respuestas.push_back({"adsb.lol", RESP_VACIA, 200, true});
  AdsbClient c(http);
  std::vector<Aeronave> aviones;
  bool ok = c.fetchCerca(CENTRO_LAT, CENTRO_LON, 25, aviones);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL(0u, aviones.size());
}

void test_adsb_respuesta_malformada(void) {
  MockHttpClient http;
  http.respuestas.push_back({"adsb.lol", RESP_MAL, 200, true});
  AdsbClient c(http);
  std::vector<Aeronave> aviones;
  bool ok = c.fetchCerca(CENTRO_LAT, CENTRO_LON, 25, aviones);
  TEST_ASSERT_FALSE(ok);
}

void test_adsb_fallo_http(void) {
  MockHttpClient http;
  http.respuestas.push_back({"adsb.lol", "", 0, false});
  AdsbClient c(http);
  std::vector<Aeronave> aviones;
  bool ok = c.fetchCerca(CENTRO_LAT, CENTRO_LON, 25, aviones);
  TEST_ASSERT_FALSE(ok);
}

void test_adsb_avion_sin_posicion_se_descarta(void) {
  static const char* SIN_POS = R"({"ac":[{"hex":"aaa","flight":"XXX"}]})";
  MockHttpClient http;
  http.respuestas.push_back({"adsb.lol", SIN_POS, 200, true});
  AdsbClient c(http);
  std::vector<Aeronave> aviones;
  bool ok = c.fetchCerca(CENTRO_LAT, CENTRO_LON, 25, aviones);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL(0u, aviones.size());
}

void test_adsb_trunca_a_50_por_distancia(void) {
  // Genera 60 aviones a distancias crecientes; espera 50, ordenados por dist.
  std::string body = "{\"ac\":[";
  for (int i = 0; i < 60; ++i) {
    if (i > 0) body += ",";
    char buf[128];
    // cada avión a 0.01° más al norte → distancias crecientes en km
    std::snprintf(buf, sizeof(buf),
                  "{\"hex\":\"a%03d\",\"flight\":\"F%d\",\"lat\":%f,\"lon\":%f,\"alt_baro\":10000,\"gs\":300,\"track\":0}",
                  i, i, CENTRO_LAT + 0.01 * (i + 1), CENTRO_LON);
    body += buf;
  }
  body += "]}";
  MockHttpClient http;
  http.respuestas.push_back({"adsb.lol", body, 200, true});
  AdsbClient c(http);
  std::vector<Aeronave> aviones;
  bool ok = c.fetchCerca(CENTRO_LAT, CENTRO_LON, 100, aviones);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL(50u, aviones.size());
  // deben estar ordenados por distancia asc
  for (size_t i = 1; i < aviones.size(); ++i) {
    TEST_ASSERT_TRUE(aviones[i - 1].dist_km <= aviones[i].dist_km);
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_adsb_parseo_normal);
  RUN_TEST(test_adsb_respuesta_vacia);
  RUN_TEST(test_adsb_respuesta_malformada);
  RUN_TEST(test_adsb_fallo_http);
  RUN_TEST(test_adsb_avion_sin_posicion_se_descarta);
  RUN_TEST(test_adsb_trunca_a_50_por_distancia);
  return UNITY_END();
}
```

- [ ] **Step 2: Verificar que los tests fallan por unidad ausente**

Run:
```
~/.platformio/penv/bin/pio test -e native -f test_adsb_client
```
Expected: FAIL — `fatal error: adsb_client.h: No such file or directory`.

- [ ] **Step 3: Implementar `adsb_client.h`**

Crea `src/adsb_client.h`:

```cpp
#pragma once
#include "http_client.h"
#include <string>
#include <vector>

struct Aeronave {
  std::string hex;
  std::string callsign;
  double lat = 0.0;
  double lon = 0.0;
  int    alt_ft = 0;
  int    gs_kt = 0;
  int    track_deg = 0;
  double dist_km = 0.0;
  int    bearing = 0;
};

class AdsbClient {
 public:
  static constexpr size_t MAX_AVIONES = 50;

  explicit AdsbClient(IHttpClient& http) : http_(http) {}

  // Consulta ADSB.lol y devuelve los aviones dentro de `radioKm` de (lat,lon).
  // Rellena dist_km y bearing por cada uno; ordena por distancia ascendente y
  // trunca a MAX_AVIONES. Devuelve false si hubo error de red o parseo.
  bool fetchCerca(double lat, double lon, int radioKm, std::vector<Aeronave>& out);

 private:
  IHttpClient& http_;
};
```

- [ ] **Step 4: Implementar `adsb_client.cpp`**

Crea `src/adsb_client.cpp`:

```cpp
#include "adsb_client.h"
#include "geo_math.h"
#include <ArduinoJson.h>
#include <algorithm>
#include <cstdio>

namespace {

std::string trimEspacios(const char* s) {
  if (!s) return "";
  std::string out(s);
  while (!out.empty() && out.back() == ' ') out.pop_back();
  while (!out.empty() && out.front() == ' ') out.erase(0, 1);
  return out;
}

}  // namespace

bool AdsbClient::fetchCerca(double lat, double lon, int radioKm,
                            std::vector<Aeronave>& out) {
  out.clear();

  char url[192];
  std::snprintf(url, sizeof(url),
                "https://api.adsb.lol/v2/point/%.6f/%.6f/%d",
                lat, lon, radioKm);

  std::string body;
  int status = 0;
  if (!http_.get(url, body, status, 5000) || status != 200 || body.empty()) {
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) return false;

  JsonArrayConst ac = doc["ac"].as<JsonArrayConst>();
  if (ac.isNull()) return true;   // sin campo "ac" pero JSON válido → 0 aviones

  std::vector<Aeronave> temp;
  temp.reserve(ac.size());

  for (JsonObjectConst obj : ac) {
    if (!obj["lat"].is<double>() || !obj["lon"].is<double>()) continue;
    Aeronave a;
    a.hex       = obj["hex"] | "";
    a.callsign  = trimEspacios(obj["flight"] | "");
    a.lat       = obj["lat"].as<double>();
    a.lon       = obj["lon"].as<double>();
    a.alt_ft    = obj["alt_baro"] | 0;
    a.gs_kt     = static_cast<int>(obj["gs"] | 0.0);
    a.track_deg = static_cast<int>(obj["track"] | 0.0);
    a.dist_km   = geo::distanciaKm(lat, lon, a.lat, a.lon);
    a.bearing   = geo::bearingGrados(lat, lon, a.lat, a.lon);
    temp.push_back(std::move(a));
  }

  std::sort(temp.begin(), temp.end(),
            [](const Aeronave& x, const Aeronave& y) { return x.dist_km < y.dist_km; });

  if (temp.size() > MAX_AVIONES) temp.resize(MAX_AVIONES);
  out = std::move(temp);
  return true;
}
```

- [ ] **Step 5: Ejecutar tests y verificar que pasan**

Run:
```
~/.platformio/penv/bin/pio test -e native -f test_adsb_client
```
Expected: `6 Tests 0 Failures 0 Ignored`.

- [ ] **Step 6: Commit**

```
git add src/adsb_client.h src/adsb_client.cpp test/test_adsb_client/
git commit -m "feat: implementar AdsbClient con parseo + trunc por distancia (TDD)"
```

---

## Task 5: ConfigStore (NVS con Preferences)

**Files:**
- Create: `src/config_store.h`, `src/config_store.cpp`
- Test: `test/test_config_store/test_main.cpp` (test de serialización pura, sin NVS)

**Nota de diseño:** `Preferences` de Arduino no está disponible en `native`. Testeamos la **serialización binaria** de `Config` a un buffer de bytes (formato usado tal cual al guardar el blob en NVS). La escritura/lectura real de NVS se valida manualmente en la placa.

- [ ] **Step 1: Escribir tests de serialización**

Crea `test/test_config_store/test_main.cpp`:

```cpp
#include <unity.h>
#include "config_store.h"
#include <vector>
#include <cstdint>

void test_serializar_y_deserializar_roundtrip(void) {
  Config in;
  in.ssid = "MiWifi";
  in.password = "secreto123";
  in.direccion = "Calle Mayor 1, Madrid";
  in.lat = 40.4168;
  in.lon = -3.7038;
  in.radio_km = 25;

  std::vector<uint8_t> buf;
  ConfigStore::serializar(in, buf);
  TEST_ASSERT_TRUE(buf.size() > 0);

  Config out;
  bool ok = ConfigStore::deserializar(buf.data(), buf.size(), out);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL_STRING("MiWifi", out.ssid.c_str());
  TEST_ASSERT_EQUAL_STRING("secreto123", out.password.c_str());
  TEST_ASSERT_EQUAL_STRING("Calle Mayor 1, Madrid", out.direccion.c_str());
  TEST_ASSERT_FLOAT_WITHIN(0.0001, 40.4168, out.lat);
  TEST_ASSERT_FLOAT_WITHIN(0.0001, -3.7038, out.lon);
  TEST_ASSERT_EQUAL(25, out.radio_km);
}

void test_deserializar_magic_byte_erroneo(void) {
  std::vector<uint8_t> buf(64, 0xFF);   // simula NVS vacía o corrupta
  Config out;
  bool ok = ConfigStore::deserializar(buf.data(), buf.size(), out);
  TEST_ASSERT_FALSE(ok);
}

void test_deserializar_buffer_muy_pequeno(void) {
  uint8_t buf[2] = {0xC0, 0xDE};
  Config out;
  bool ok = ConfigStore::deserializar(buf, sizeof(buf), out);
  TEST_ASSERT_FALSE(ok);
}

void test_deserializar_buffer_nulo(void) {
  Config out;
  bool ok = ConfigStore::deserializar(nullptr, 0, out);
  TEST_ASSERT_FALSE(ok);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_serializar_y_deserializar_roundtrip);
  RUN_TEST(test_deserializar_magic_byte_erroneo);
  RUN_TEST(test_deserializar_buffer_muy_pequeno);
  RUN_TEST(test_deserializar_buffer_nulo);
  return UNITY_END();
}
```

- [ ] **Step 2: Verificar que los tests fallan por unidad ausente**

Run:
```
~/.platformio/penv/bin/pio test -e native -f test_config_store
```
Expected: FAIL — `fatal error: config_store.h: No such file or directory`.

- [ ] **Step 3: Implementar `config_store.h`**

Crea `src/config_store.h`:

```cpp
#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct Config {
  std::string ssid;
  std::string password;
  std::string direccion;
  double lat = 0.0;
  double lon = 0.0;
  int    radio_km = 25;
};

// Persistencia en NVS mediante Preferences. La serialización a bytes se expone
// como funciones estáticas para poder testearla en native (sin depender de NVS).
//
// Formato binario (little-endian):
//   [0..1]   magic 0xC0DE
//   [2]      versión (1)
//   [3..10]  lat (double, 8 bytes)
//   [11..18] lon (double, 8 bytes)
//   [19..20] radio_km (uint16)
//   [21..22] len_ssid (uint16) | ssid bytes
//   [..]     len_pass (uint16) | pass bytes
//   [..]     len_dir  (uint16) | dir bytes
class ConfigStore {
 public:
  static constexpr uint16_t MAGIC = 0xC0DE;
  static constexpr uint8_t  VERSION = 1;
  static constexpr size_t   MAX_STR = 128;

  // Serialización pura (testeable en native).
  static void serializar(const Config& in, std::vector<uint8_t>& out);
  static bool deserializar(const uint8_t* data, size_t size, Config& out);

#ifndef UNIT_TEST
  // API con NVS (solo compila en la placa).
  static bool cargar(Config& out);
  static bool guardar(const Config& cfg);
  static void borrar();
#endif
};
```

- [ ] **Step 4: Implementar `config_store.cpp`**

Crea `src/config_store.cpp`:

```cpp
#include "config_store.h"
#include <cstring>

#ifndef UNIT_TEST
#include <Preferences.h>
static const char* NVS_NAMESPACE = "radarvuelos";
static const char* NVS_KEY = "cfg";
#endif

namespace {

void escribirU16(std::vector<uint8_t>& v, uint16_t x) {
  v.push_back(static_cast<uint8_t>(x & 0xFF));
  v.push_back(static_cast<uint8_t>((x >> 8) & 0xFF));
}

void escribirString(std::vector<uint8_t>& v, const std::string& s) {
  uint16_t n = static_cast<uint16_t>(s.size());
  if (n > ConfigStore::MAX_STR) n = ConfigStore::MAX_STR;
  escribirU16(v, n);
  v.insert(v.end(), s.begin(), s.begin() + n);
}

void escribirDouble(std::vector<uint8_t>& v, double d) {
  uint8_t buf[sizeof(double)];
  std::memcpy(buf, &d, sizeof(double));
  v.insert(v.end(), buf, buf + sizeof(double));
}

bool leerU16(const uint8_t* data, size_t size, size_t& pos, uint16_t& out) {
  if (pos + 2 > size) return false;
  out = static_cast<uint16_t>(data[pos]) |
        (static_cast<uint16_t>(data[pos + 1]) << 8);
  pos += 2;
  return true;
}

bool leerDouble(const uint8_t* data, size_t size, size_t& pos, double& out) {
  if (pos + sizeof(double) > size) return false;
  std::memcpy(&out, data + pos, sizeof(double));
  pos += sizeof(double);
  return true;
}

bool leerString(const uint8_t* data, size_t size, size_t& pos, std::string& out) {
  uint16_t n;
  if (!leerU16(data, size, pos, n)) return false;
  if (n > ConfigStore::MAX_STR) return false;
  if (pos + n > size) return false;
  out.assign(reinterpret_cast<const char*>(data + pos), n);
  pos += n;
  return true;
}

}  // namespace

void ConfigStore::serializar(const Config& in, std::vector<uint8_t>& out) {
  out.clear();
  escribirU16(out, MAGIC);
  out.push_back(VERSION);
  escribirDouble(out, in.lat);
  escribirDouble(out, in.lon);
  escribirU16(out, static_cast<uint16_t>(in.radio_km));
  escribirString(out, in.ssid);
  escribirString(out, in.password);
  escribirString(out, in.direccion);
}

bool ConfigStore::deserializar(const uint8_t* data, size_t size, Config& out) {
  if (!data || size < 4) return false;
  size_t pos = 0;
  uint16_t magic;
  if (!leerU16(data, size, pos, magic) || magic != MAGIC) return false;
  if (data[pos++] != VERSION) return false;
  double lat, lon;
  if (!leerDouble(data, size, pos, lat)) return false;
  if (!leerDouble(data, size, pos, lon)) return false;
  uint16_t radio;
  if (!leerU16(data, size, pos, radio)) return false;
  std::string ssid, pass, dir;
  if (!leerString(data, size, pos, ssid)) return false;
  if (!leerString(data, size, pos, pass)) return false;
  if (!leerString(data, size, pos, dir))  return false;
  out.ssid = std::move(ssid);
  out.password = std::move(pass);
  out.direccion = std::move(dir);
  out.lat = lat;
  out.lon = lon;
  out.radio_km = radio;
  return true;
}

#ifndef UNIT_TEST

bool ConfigStore::cargar(Config& out) {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, /*readOnly=*/true)) return false;
  size_t sz = prefs.getBytesLength(NVS_KEY);
  if (sz == 0 || sz > 4096) { prefs.end(); return false; }
  std::vector<uint8_t> buf(sz);
  size_t leido = prefs.getBytes(NVS_KEY, buf.data(), sz);
  prefs.end();
  if (leido != sz) return false;
  return deserializar(buf.data(), sz, out);
}

bool ConfigStore::guardar(const Config& cfg) {
  std::vector<uint8_t> buf;
  serializar(cfg, buf);
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, /*readOnly=*/false)) return false;
  size_t escrito = prefs.putBytes(NVS_KEY, buf.data(), buf.size());
  prefs.end();
  return escrito == buf.size();
}

void ConfigStore::borrar() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, /*readOnly=*/false)) return;
  prefs.remove(NVS_KEY);
  prefs.end();
}

#endif
```

- [ ] **Step 5: Ejecutar tests y verificar que pasan**

Run:
```
~/.platformio/penv/bin/pio test -e native -f test_config_store
```
Expected: `4 Tests 0 Failures 0 Ignored`.

- [ ] **Step 6: Verificar que sigue compilando para la placa**

Run:
```
~/.platformio/penv/bin/pio run -e esp32dev
```
Expected: compila hasta un `undefined reference to main` (aún no hemos escrito `main.cpp`) o link error inofensivo. Lo importante: sin errores de compilación de `config_store.cpp`.

- [ ] **Step 7: Commit**

```
git add src/config_store.h src/config_store.cpp test/test_config_store/
git commit -m "feat: implementar ConfigStore con serialización binaria y NVS (TDD)"
```

---

## Task 6: RadarState (snapshot en memoria, thread-safe)

**Files:**
- Create: `src/radar_state.h`, `src/radar_state.cpp`

**Nota:** No test unitario formal — la lógica es un simple wrapper con mutex. Se valida por integración cuando arranquen `poller` y `web` juntos. En `native` los mutex de FreeRTOS no existen; usamos `#ifdef ARDUINO` para condicionar el sincronismo.

- [ ] **Step 1: Escribir `radar_state.h`**

Crea `src/radar_state.h`:

```cpp
#pragma once
#include "adsb_client.h"
#include <vector>
#include <cstdint>

#ifdef ARDUINO
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

struct Snapshot {
  double lat = 0.0;
  double lon = 0.0;
  int    radio_km = 25;
  uint32_t ts = 0;         // millis() del último update exitoso
  bool   stale = true;
  std::vector<Aeronave> aeronaves;
};

class RadarState {
 public:
  RadarState(double lat, double lon, int radioKm);
  ~RadarState();

  // Sustituye la lista y marca no-stale con ts actual.
  void actualizar(const std::vector<Aeronave>& aviones, uint32_t timestampMs);

  // Mantiene la lista pero marca stale=true.
  void marcarStale();

  // Devuelve una copia atómica del snapshot.
  Snapshot snapshot();

 private:
  Snapshot s_;
#ifdef ARDUINO
  SemaphoreHandle_t mutex_;
#endif
};
```

- [ ] **Step 2: Escribir `radar_state.cpp`**

Crea `src/radar_state.cpp`:

```cpp
#include "radar_state.h"

RadarState::RadarState(double lat, double lon, int radioKm) {
  s_.lat = lat;
  s_.lon = lon;
  s_.radio_km = radioKm;
#ifdef ARDUINO
  mutex_ = xSemaphoreCreateMutex();
#endif
}

RadarState::~RadarState() {
#ifdef ARDUINO
  if (mutex_) vSemaphoreDelete(mutex_);
#endif
}

void RadarState::actualizar(const std::vector<Aeronave>& aviones, uint32_t timestampMs) {
#ifdef ARDUINO
  xSemaphoreTake(mutex_, portMAX_DELAY);
#endif
  s_.aeronaves = aviones;
  s_.ts = timestampMs;
  s_.stale = false;
#ifdef ARDUINO
  xSemaphoreGive(mutex_);
#endif
}

void RadarState::marcarStale() {
#ifdef ARDUINO
  xSemaphoreTake(mutex_, portMAX_DELAY);
#endif
  s_.stale = true;
#ifdef ARDUINO
  xSemaphoreGive(mutex_);
#endif
}

Snapshot RadarState::snapshot() {
#ifdef ARDUINO
  xSemaphoreTake(mutex_, portMAX_DELAY);
  Snapshot copia = s_;
  xSemaphoreGive(mutex_);
  return copia;
#else
  return s_;
#endif
}
```

- [ ] **Step 3: Verificar compilación en placa**

Run:
```
~/.platformio/penv/bin/pio run -e esp32dev
```
Expected: sin errores de compilación en `radar_state.cpp` (el link puede fallar por falta de `main`, ignora).

- [ ] **Step 4: Commit**

```
git add src/radar_state.h src/radar_state.cpp
git commit -m "feat: implementar RadarState con snapshot thread-safe (mutex FreeRTOS)"
```

---

## Task 7: StatusLed (patrones de parpadeo)

**Files:**
- Create: `src/status_led.h`, `src/status_led.cpp`

- [ ] **Step 1: Escribir `status_led.h`**

Crea `src/status_led.h`:

```cpp
#pragma once
#include <cstdint>

enum class EstadoLed {
  PORTAL,           // parpadeo 1 Hz
  CONECTANDO_WIFI,  // parpadeo 5 Hz
  RADAR_OK,         // fijo encendido
  RADAR_ERROR       // encendido, se apaga 2 s cada minuto
};

class StatusLed {
 public:
  static void iniciar(int pin = 2);          // GPIO 2 = LED interno de la mayoría de ESP32 dev
  static void setEstado(EstadoLed nuevo);
};
```

- [ ] **Step 2: Escribir `status_led.cpp`**

Crea `src/status_led.cpp`:

```cpp
#include "status_led.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

int             s_pin = 2;
EstadoLed       s_estado = EstadoLed::PORTAL;
TaskHandle_t    s_task = nullptr;

void tareaLed(void*) {
  bool encendido = false;
  uint32_t contadorMs = 0;
  for (;;) {
    switch (s_estado) {
      case EstadoLed::PORTAL:
        encendido = !encendido;
        digitalWrite(s_pin, encendido ? HIGH : LOW);
        vTaskDelay(pdMS_TO_TICKS(500));  // 1 Hz
        break;
      case EstadoLed::CONECTANDO_WIFI:
        encendido = !encendido;
        digitalWrite(s_pin, encendido ? HIGH : LOW);
        vTaskDelay(pdMS_TO_TICKS(100));  // 5 Hz
        break;
      case EstadoLed::RADAR_OK:
        digitalWrite(s_pin, HIGH);
        vTaskDelay(pdMS_TO_TICKS(200));
        break;
      case EstadoLed::RADAR_ERROR:
        // encendido, con corte de 2 s cada 60 s
        if (contadorMs >= 60000) {
          digitalWrite(s_pin, LOW);
          vTaskDelay(pdMS_TO_TICKS(2000));
          contadorMs = 0;
        } else {
          digitalWrite(s_pin, HIGH);
          vTaskDelay(pdMS_TO_TICKS(200));
          contadorMs += 200;
        }
        break;
    }
  }
}

}  // namespace

void StatusLed::iniciar(int pin) {
  s_pin = pin;
  pinMode(s_pin, OUTPUT);
  digitalWrite(s_pin, LOW);
  if (!s_task) {
    xTaskCreatePinnedToCore(tareaLed, "led", 2048, nullptr, 1, &s_task, 1);
  }
}

void StatusLed::setEstado(EstadoLed nuevo) {
  s_estado = nuevo;
}
```

- [ ] **Step 3: Verificar compilación en placa**

Run:
```
~/.platformio/penv/bin/pio run -e esp32dev
```
Expected: sin errores en `status_led.cpp`.

- [ ] **Step 4: Commit**

```
git add src/status_led.h src/status_led.cpp
git commit -m "feat: implementar StatusLed con task FreeRTOS y 4 patrones"
```

---

## Task 8: WifiHttpClient (implementación real de IHttpClient)

**Files:**
- Modify: `src/http_client.h` (añadir la clase real)
- Create: `src/http_client.cpp`

- [ ] **Step 1: Añadir la clase real al header**

Modifica `src/http_client.h` — añade al final del archivo:

```cpp
// -----------------------------------------------------------------------------
// Implementación real usando WiFiClientSecure / HTTPClient (Arduino ESP32)
// -----------------------------------------------------------------------------
#ifdef ARDUINO
class WifiHttpClient : public IHttpClient {
 public:
  bool get(const std::string& url,
           std::string& bodyOut,
           int& statusOut,
           int timeoutMs = 5000) override;
};
#endif
```

- [ ] **Step 2: Escribir `http_client.cpp`**

Crea `src/http_client.cpp`:

```cpp
#include "http_client.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

bool WifiHttpClient::get(const std::string& url,
                         std::string& bodyOut,
                         int& statusOut,
                         int timeoutMs) {
  bodyOut.clear();
  statusOut = 0;
  HTTPClient http;
  http.setTimeout(timeoutMs);
  // Nominatim exige User-Agent identificable.
  http.setUserAgent("radar-vuelos-esp32/1.0 (albertodelpozo)");
  bool ok;
  if (url.rfind("https://", 0) == 0) {
    WiFiClientSecure client;
    client.setInsecure();  // Sin validación de certificado (aceptable para APIs públicas de solo lectura).
    ok = http.begin(client, url.c_str());
  } else {
    ok = http.begin(url.c_str());
  }
  if (!ok) return false;
  int code = http.GET();
  statusOut = code;
  if (code <= 0) { http.end(); return false; }
  String payload = http.getString();
  bodyOut.assign(payload.c_str(), payload.length());
  http.end();
  return true;
}
#endif
```

- [ ] **Step 3: Verificar compilación en placa**

Run:
```
~/.platformio/penv/bin/pio run -e esp32dev
```
Expected: sin errores en `http_client.cpp`.

- [ ] **Step 4: Verificar que los tests native no rompen**

Run:
```
~/.platformio/penv/bin/pio test -e native
```
Expected: todos los tests siguen pasando (`test_geo_math`, `test_geocoder`, `test_adsb_client`, `test_config_store`).

- [ ] **Step 5: Commit**

```
git add src/http_client.h src/http_client.cpp
git commit -m "feat: implementar WifiHttpClient (HTTPClient sobre WiFi)"
```

---

## Task 9: Frontend estático (data/ para LittleFS)

**Files:**
- Create: `data/portal.html`, `data/config.html`, `data/index.html`, `data/app.js`, `data/style.css`

- [ ] **Step 1: `data/style.css`**

Crea `data/style.css`:

```css
* { box-sizing: border-box; margin: 0; padding: 0; }
body {
  font-family: -apple-system, system-ui, sans-serif;
  background: #0e1116;
  color: #d0d7de;
  padding: 16px;
  max-width: 900px;
  margin: 0 auto;
}
h1 { font-size: 20px; margin-bottom: 12px; }
h2 { font-size: 15px; margin: 12px 0 6px; color: #79b8ff; }
form { display: grid; gap: 10px; }
label { display: block; font-size: 13px; color: #8b949e; }
input, button {
  width: 100%;
  padding: 8px 10px;
  background: #161b22;
  color: #d0d7de;
  border: 1px solid #30363d;
  border-radius: 6px;
  font-size: 14px;
}
button {
  background: #1f6feb;
  color: white;
  border: none;
  cursor: pointer;
}
button:hover { background: #388bfd; }
.msg-error { color: #f85149; padding: 8px; background: #2d1315; border-radius: 6px; }
.msg-ok    { color: #56d364; padding: 8px; background: #12261a; border-radius: 6px; }

/* Radar layout */
.radar-wrap { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; }
@media (max-width: 700px) { .radar-wrap { grid-template-columns: 1fr; } }
canvas#radar { width: 100%; aspect-ratio: 1; background: #161b22; border-radius: 8px; }
table { width: 100%; border-collapse: collapse; font-size: 13px; }
th, td { padding: 6px 4px; text-align: left; border-bottom: 1px solid #21262d; }
th { color: #8b949e; font-weight: normal; }
.banner-stale { background: #3d2810; color: #f9a825; padding: 6px 10px; border-radius: 4px; margin-bottom: 8px; font-size: 12px; }
```

- [ ] **Step 2: `data/portal.html`**

Crea `data/portal.html`:

```html
<!doctype html>
<html lang="es">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Configurar Radar de vuelos</title>
  <link rel="stylesheet" href="/style.css">
</head>
<body>
  <h1>Radar de vuelos — configuración inicial</h1>
  <p style="margin-bottom:12px; color:#8b949e;">Introduce los datos de tu WiFi y la dirección desde la que quieres vigilar.</p>
  <div id="msg"></div>
  <form id="f">
    <div>
      <label>SSID (nombre de la red WiFi)</label>
      <input name="ssid" required maxlength="128">
    </div>
    <div>
      <label>Password</label>
      <input name="password" type="password" required maxlength="128">
    </div>
    <div>
      <label>Dirección postal (ej: "Calle Mayor 1, Madrid, España")</label>
      <input name="direccion" required maxlength="128">
    </div>
    <div>
      <label>Radio de vigilancia (km)</label>
      <input name="radio_km" type="number" value="25" min="1" max="500" required>
    </div>
    <button type="submit">Guardar y reiniciar</button>
  </form>
  <script>
    document.getElementById('f').addEventListener('submit', async (e) => {
      e.preventDefault();
      const msg = document.getElementById('msg');
      msg.innerHTML = '<div class="msg-ok">Resolviendo dirección y guardando...</div>';
      const data = Object.fromEntries(new FormData(e.target));
      const r = await fetch('/api/save', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(data)
      });
      if (r.ok) {
        msg.innerHTML = '<div class="msg-ok">¡Guardado! La placa se reiniciará en unos segundos.</div>';
      } else {
        const err = await r.text();
        msg.innerHTML = '<div class="msg-error">Error: ' + err + '</div>';
      }
    });
  </script>
</body>
</html>
```

- [ ] **Step 3: `data/index.html`**

Crea `data/index.html`:

```html
<!doctype html>
<html lang="es">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Radar de vuelos</title>
  <link rel="stylesheet" href="/style.css">
</head>
<body>
  <h1>Radar de vuelos</h1>
  <div id="stale" style="display:none" class="banner-stale">Sin conexión con la API — mostrando último snapshot conocido.</div>
  <div class="radar-wrap">
    <canvas id="radar" width="500" height="500"></canvas>
    <div>
      <h2>Aviones detectados (<span id="n">0</span>)</h2>
      <table>
        <thead>
          <tr><th>Callsign</th><th>Alt (ft)</th><th>Dist (km)</th><th>Rumbo</th></tr>
        </thead>
        <tbody id="lista"></tbody>
      </table>
    </div>
  </div>
  <p style="margin-top:16px; font-size:12px; color:#8b949e;">
    <a href="/config" style="color:#79b8ff;">Reconfigurar</a> ·
    Datos: <a href="https://adsb.lol" style="color:#79b8ff;">ADSB.lol</a>
  </p>
  <script src="/app.js"></script>
</body>
</html>
```

- [ ] **Step 4: `data/config.html`**

Crea `data/config.html`:

```html
<!doctype html>
<html lang="es">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Reconfigurar Radar</title>
  <link rel="stylesheet" href="/style.css">
</head>
<body>
  <h1>Reconfigurar Radar</h1>
  <p style="margin-bottom:12px; color:#8b949e;">Al guardar cambios la placa se reiniciará (~3 s).</p>
  <div id="msg"></div>
  <form id="f">
    <div><label>SSID</label><input name="ssid" required maxlength="128"></div>
    <div><label>Password</label><input name="password" type="password" required maxlength="128"></div>
    <div><label>Dirección postal</label><input name="direccion" required maxlength="128"></div>
    <div><label>Radio (km)</label><input name="radio_km" type="number" min="1" max="500" required></div>
    <button type="submit">Guardar y reiniciar</button>
  </form>
  <hr style="border:none; border-top:1px solid #21262d; margin:20px 0;">
  <h2>Reset total</h2>
  <button id="reset" style="background:#da3633;">Borrar config y volver al portal</button>
  <script>
    async function precargar() {
      const r = await fetch('/api/config');
      if (!r.ok) return;
      const cfg = await r.json();
      for (const k of ['ssid', 'password', 'direccion', 'radio_km']) {
        const el = document.querySelector('[name=' + k + ']');
        if (el && cfg[k] !== undefined) el.value = cfg[k];
      }
    }
    precargar();
    document.getElementById('f').addEventListener('submit', async (e) => {
      e.preventDefault();
      const msg = document.getElementById('msg');
      msg.innerHTML = '<div class="msg-ok">Guardando...</div>';
      const data = Object.fromEntries(new FormData(e.target));
      const r = await fetch('/api/config', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(data)
      });
      if (r.ok) msg.innerHTML = '<div class="msg-ok">Guardado. Reiniciando...</div>';
      else msg.innerHTML = '<div class="msg-error">' + await r.text() + '</div>';
    });
    document.getElementById('reset').addEventListener('click', async () => {
      if (!confirm('¿Borrar config y volver al portal?')) return;
      await fetch('/api/reset', {method: 'POST'});
      document.getElementById('msg').innerHTML = '<div class="msg-ok">Borrado. Reiniciando en modo Portal...</div>';
    });
  </script>
</body>
</html>
```

- [ ] **Step 5: `data/app.js`**

Crea `data/app.js`:

```javascript
const canvas = document.getElementById('radar');
const ctx = canvas.getContext('2d');
const lista = document.getElementById('lista');
const nSpan = document.getElementById('n');
const staleBanner = document.getElementById('stale');

let centro = { lat: 0, lon: 0 };
let radioKm = 25;

function dibujarBase() {
  const w = canvas.width, h = canvas.height;
  const cx = w / 2, cy = h / 2;
  const rMax = Math.min(w, h) / 2 - 20;
  ctx.fillStyle = '#161b22';
  ctx.fillRect(0, 0, w, h);
  ctx.strokeStyle = '#30363d';
  ctx.fillStyle = '#8b949e';
  ctx.font = '11px system-ui';
  // círculos concéntricos cada 20% del radio
  for (let i = 1; i <= 5; i++) {
    ctx.beginPath();
    ctx.arc(cx, cy, rMax * i / 5, 0, Math.PI * 2);
    ctx.stroke();
    ctx.fillText(Math.round(radioKm * i / 5) + ' km', cx + 4, cy - rMax * i / 5 - 2);
  }
  // ejes N/S/E/W
  ctx.beginPath();
  ctx.moveTo(cx, cy - rMax); ctx.lineTo(cx, cy + rMax);
  ctx.moveTo(cx - rMax, cy); ctx.lineTo(cx + rMax, cy);
  ctx.stroke();
  ctx.fillStyle = '#79b8ff';
  ctx.fillText('N', cx - 4, cy - rMax - 4);
  ctx.fillText('S', cx - 4, cy + rMax + 14);
  ctx.fillText('E', cx + rMax + 4, cy + 4);
  ctx.fillText('O', cx - rMax - 14, cy + 4);
  // punto central
  ctx.fillStyle = '#f85149';
  ctx.beginPath(); ctx.arc(cx, cy, 3, 0, Math.PI * 2); ctx.fill();
}

function dibujarAviones(aviones) {
  const w = canvas.width, h = canvas.height;
  const cx = w / 2, cy = h / 2;
  const rMax = Math.min(w, h) / 2 - 20;
  for (const a of aviones) {
    if (a.dist_km > radioKm) continue;
    const r = (a.dist_km / radioKm) * rMax;
    const rad = (a.bearing - 90) * Math.PI / 180;   // 0° = norte, en canvas norte es -Y
    const x = cx + r * Math.cos(rad);
    const y = cy + r * Math.sin(rad);
    // triángulo orientado según track
    ctx.save();
    ctx.translate(x, y);
    ctx.rotate((a.trk - 90) * Math.PI / 180);
    ctx.fillStyle = '#56d364';
    ctx.beginPath();
    ctx.moveTo(6, 0); ctx.lineTo(-4, -4); ctx.lineTo(-4, 4);
    ctx.closePath(); ctx.fill();
    ctx.restore();
    // etiqueta
    ctx.fillStyle = '#d0d7de';
    ctx.font = '10px system-ui';
    ctx.fillText(a.cs || a.hex, x + 8, y - 4);
  }
}

function pintarLista(aviones) {
  lista.innerHTML = '';
  nSpan.textContent = aviones.length;
  for (const a of aviones) {
    const tr = document.createElement('tr');
    tr.innerHTML =
      '<td>' + (a.cs || a.hex) + '</td>' +
      '<td>' + a.alt_ft + '</td>' +
      '<td>' + a.dist_km.toFixed(1) + '</td>' +
      '<td>' + a.bearing + '°</td>';
    lista.appendChild(tr);
  }
}

async function refrescar() {
  try {
    const r = await fetch('/api/aircraft');
    if (!r.ok) throw new Error('http ' + r.status);
    const data = await r.json();
    centro = data.center;
    radioKm = data.radio_km;
    staleBanner.style.display = data.stale ? 'block' : 'none';
    dibujarBase();
    dibujarAviones(data.aircraft || []);
    pintarLista(data.aircraft || []);
  } catch (e) {
    staleBanner.style.display = 'block';
    staleBanner.textContent = 'Sin conexión con la placa.';
  }
}

dibujarBase();
refrescar();
setInterval(refrescar, 2000);
```

- [ ] **Step 6: Commit**

```
git add data/
git commit -m "feat: frontend estático (portal, index radar, config) para LittleFS"
```

---

## Task 10: WifiPortal (AP + formulario + geocoding + save)

**Files:**
- Create: `src/wifi_portal.h`, `src/wifi_portal.cpp`

- [ ] **Step 1: `src/wifi_portal.h`**

Crea `src/wifi_portal.h`:

```cpp
#pragma once
#include "config_store.h"
#include "http_client.h"

class WifiPortal {
 public:
  // Bloquea indefinidamente sirviendo el portal AP.
  // Cuando el usuario envía el formulario y el geocoding tiene éxito, guarda
  // la config en NVS y reinicia la ESP32 (no retorna).
  static void ejecutar(IHttpClient& http);
};
```

- [ ] **Step 2: `src/wifi_portal.cpp`**

Crea `src/wifi_portal.cpp`:

```cpp
#include "wifi_portal.h"
#include "geocoder.h"
#include "status_led.h"
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

namespace {

String macSufijo() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char buf[5];
  snprintf(buf, sizeof(buf), "%02X%02X", mac[4], mac[5]);
  return String(buf);
}

}  // namespace

void WifiPortal::ejecutar(IHttpClient& http) {
  StatusLed::setEstado(EstadoLed::PORTAL);

  String ssidAp = String("RadarVuelos-") + macSufijo();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssidAp.c_str());
  Serial.printf("[portal] AP levantado: %s  IP: %s\n",
                ssidAp.c_str(), WiFi.softAPIP().toString().c_str());

  static AsyncWebServer server(80);
  server.serveStatic("/", LittleFS, "/").setDefaultFile("portal.html");

  server.on("/api/save", HTTP_POST,
    [](AsyncWebServerRequest*) {},
    nullptr,
    [&http](AsyncWebServerRequest* req, uint8_t* data, size_t len,
            size_t /*index*/, size_t /*total*/) {
      JsonDocument doc;
      if (deserializeJson(doc, data, len)) {
        req->send(400, "text/plain", "JSON inválido");
        return;
      }
      std::string ssid  = doc["ssid"]      | "";
      std::string pass  = doc["password"]  | "";
      std::string dir   = doc["direccion"] | "";
      int radio         = doc["radio_km"]  | 25;
      if (ssid.empty() || pass.empty() || dir.empty()) {
        req->send(400, "text/plain", "Campos obligatorios vacíos");
        return;
      }
      Geocoder g(http);
      double lat = 0, lon = 0;
      if (!g.resolver(dir, lat, lon)) {
        req->send(400, "text/plain",
                  "Dirección no encontrada. Prueba a añadir ciudad y país.");
        return;
      }
      Config cfg;
      cfg.ssid = ssid;
      cfg.password = pass;
      cfg.direccion = dir;
      cfg.lat = lat;
      cfg.lon = lon;
      cfg.radio_km = radio;
      if (!ConfigStore::guardar(cfg)) {
        req->send(500, "text/plain", "Error guardando en NVS");
        return;
      }
      req->send(200, "text/plain", "OK, reiniciando");
      delay(500);
      ESP.restart();
    });

  server.begin();
  Serial.println("[portal] esperando configuración...");
  // Loop pasivo — el AsyncWebServer maneja las requests en su tarea propia.
  for (;;) {
    delay(1000);
  }
}
```

- [ ] **Step 3: Verificar compilación en placa**

Run:
```
~/.platformio/penv/bin/pio run -e esp32dev
```
Expected: sin errores en `wifi_portal.cpp` (aún link error por falta de main — normal).

- [ ] **Step 4: Commit**

```
git add src/wifi_portal.h src/wifi_portal.cpp
git commit -m "feat: implementar WifiPortal con AP + geocoding + guardado en NVS"
```

---

## Task 11: WebServer (rutas en Modo Radar)

**Files:**
- Create: `src/web_server.h`, `src/web_server.cpp`

- [ ] **Step 1: `src/web_server.h`**

Crea `src/web_server.h`:

```cpp
#pragma once
#include "config_store.h"
#include "radar_state.h"
#include "http_client.h"

class RadarWebServer {
 public:
  // Arranca las rutas HTTP. `http` se usa para geocoding al reconfigurar.
  static void iniciar(const Config& cfg, RadarState& estado, IHttpClient& http);
};
```

- [ ] **Step 2: `src/web_server.cpp`**

Crea `src/web_server.cpp`:

```cpp
#include "web_server.h"
#include "geocoder.h"
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

namespace {

AsyncWebServer server(80);
Config          s_cfg;
RadarState*     s_estado = nullptr;
IHttpClient*    s_http = nullptr;

void handleAircraft(AsyncWebServerRequest* req) {
  Snapshot snap = s_estado->snapshot();
  JsonDocument doc;
  auto c = doc["center"].to<JsonObject>();
  c["lat"] = snap.lat;
  c["lon"] = snap.lon;
  doc["radio_km"] = snap.radio_km;
  doc["ts"]       = snap.ts;
  doc["stale"]    = snap.stale;
  auto arr = doc["aircraft"].to<JsonArray>();
  for (const auto& a : snap.aeronaves) {
    auto o = arr.add<JsonObject>();
    o["hex"]     = a.hex;
    o["cs"]      = a.callsign;
    o["lat"]     = a.lat;
    o["lon"]     = a.lon;
    o["alt_ft"] = a.alt_ft;
    o["gs"]     = a.gs_kt;
    o["trk"]    = a.track_deg;
    o["dist_km"] = a.dist_km;
    o["bearing"] = a.bearing;
  }
  String out;
  serializeJson(doc, out);
  req->send(200, "application/json", out);
}

void handleConfigGet(AsyncWebServerRequest* req) {
  JsonDocument doc;
  doc["ssid"]      = s_cfg.ssid;
  doc["password"]  = s_cfg.password;
  doc["direccion"] = s_cfg.direccion;
  doc["radio_km"]  = s_cfg.radio_km;
  String out;
  serializeJson(doc, out);
  req->send(200, "application/json", out);
}

void handleConfigPost(AsyncWebServerRequest* req, uint8_t* data, size_t len,
                      size_t /*index*/, size_t /*total*/) {
  JsonDocument doc;
  if (deserializeJson(doc, data, len)) {
    req->send(400, "text/plain", "JSON inválido");
    return;
  }
  std::string ssid = doc["ssid"]      | "";
  std::string pass = doc["password"]  | "";
  std::string dir  = doc["direccion"] | "";
  int radio        = doc["radio_km"]  | 25;
  if (ssid.empty() || pass.empty() || dir.empty()) {
    req->send(400, "text/plain", "Campos obligatorios vacíos");
    return;
  }
  double lat = s_cfg.lat, lon = s_cfg.lon;
  if (dir != s_cfg.direccion) {
    Geocoder g(*s_http);
    if (!g.resolver(dir, lat, lon)) {
      req->send(400, "text/plain",
                "Dirección no encontrada. Prueba a añadir ciudad y país.");
      return;
    }
  }
  Config nueva;
  nueva.ssid = ssid;
  nueva.password = pass;
  nueva.direccion = dir;
  nueva.lat = lat;
  nueva.lon = lon;
  nueva.radio_km = radio;
  if (!ConfigStore::guardar(nueva)) {
    req->send(500, "text/plain", "Error guardando en NVS");
    return;
  }
  req->send(200, "text/plain", "OK, reiniciando");
  delay(500);
  ESP.restart();
}

void handleReset(AsyncWebServerRequest* req) {
  ConfigStore::borrar();
  req->send(200, "text/plain", "Config borrada, reiniciando");
  delay(500);
  ESP.restart();
}

}  // namespace

void RadarWebServer::iniciar(const Config& cfg, RadarState& estado, IHttpClient& http) {
  s_cfg = cfg;
  s_estado = &estado;
  s_http = &http;

  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  server.on("/config", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(LittleFS, "/config.html", "text/html");
  });
  server.on("/api/aircraft", HTTP_GET, handleAircraft);
  server.on("/api/config",   HTTP_GET, handleConfigGet);
  server.on("/api/config",   HTTP_POST,
            [](AsyncWebServerRequest*) {}, nullptr, handleConfigPost);
  server.on("/api/reset",    HTTP_POST, handleReset);
  server.begin();
  Serial.println("[web] servidor iniciado");
}
```

- [ ] **Step 3: Verificar compilación en placa**

Run:
```
~/.platformio/penv/bin/pio run -e esp32dev
```
Expected: sin errores en `web_server.cpp` (link error por falta de main — normal).

- [ ] **Step 4: Commit**

```
git add src/web_server.h src/web_server.cpp
git commit -m "feat: implementar RadarWebServer con rutas /api/aircraft, /config, /api/reset"
```

---

## Task 12: main.cpp — orquestador (boot → decide modo → arranca tasks)

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Sobrescribir `src/main.cpp`**

Reemplaza el contenido de `src/main.cpp` con:

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "config_store.h"
#include "http_client.h"
#include "adsb_client.h"
#include "radar_state.h"
#include "wifi_portal.h"
#include "web_server.h"
#include "status_led.h"

namespace {

RadarState*     g_estado = nullptr;
WifiHttpClient  g_http;
Config          g_cfg;
uint32_t        g_ultimoIntentoWifiMs = 0;

// Conecta al WiFi guardado. Devuelve true si conecta en <= 20 s.
bool conectarWifi() {
  StatusLed::setEstado(EstadoLed::CONECTANDO_WIFI);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(g_cfg.ssid.c_str(), g_cfg.password.c_str());
  uint32_t inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 20000) {
    delay(200);
  }
  if (WiFi.status() != WL_CONNECTED) return false;
  Serial.printf("[wifi] conectado, IP: %s\n", WiFi.localIP().toString().c_str());
  return true;
}

// Task del poller: cada 3 s consulta ADSB.lol y actualiza el snapshot.
void tareaPoller(void*) {
  AdsbClient cliente(g_http);
  for (;;) {
    // Watchdog implícito: si el fetch se cuelga > 15 s se dispara el TWDT (activo por defecto).
    if (WiFi.status() != WL_CONNECTED) {
      g_estado->marcarStale();
      StatusLed::setEstado(EstadoLed::RADAR_ERROR);
      if (millis() - g_ultimoIntentoWifiMs > 60000) {
        Serial.println("[wifi] 60s sin conexión, reiniciando");
        ESP.restart();
      }
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
    g_ultimoIntentoWifiMs = millis();
    std::vector<Aeronave> aviones;
    bool ok = cliente.fetchCerca(g_cfg.lat, g_cfg.lon, g_cfg.radio_km, aviones);
    if (!ok) {
      // 1 reintento inmediato
      ok = cliente.fetchCerca(g_cfg.lat, g_cfg.lon, g_cfg.radio_km, aviones);
    }
    if (ok) {
      g_estado->actualizar(aviones, millis());
      StatusLed::setEstado(EstadoLed::RADAR_OK);
    } else {
      g_estado->marcarStale();
      StatusLed::setEstado(EstadoLed::RADAR_ERROR);
    }
    vTaskDelay(pdMS_TO_TICKS(3000));
  }
}

void modoRadar() {
  g_estado = new RadarState(g_cfg.lat, g_cfg.lon, g_cfg.radio_km);
  RadarWebServer::iniciar(g_cfg, *g_estado, g_http);
  xTaskCreatePinnedToCore(tareaPoller, "poller", 8192, nullptr, 1, nullptr, 0);
  Serial.println("[radar] modo operativo");
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n===== Radar de vuelos ESP32 =====");

  if (!LittleFS.begin(true)) {
    Serial.println("[fs] error montando LittleFS");
  }

  StatusLed::iniciar(2);

  bool tieneCfg = ConfigStore::cargar(g_cfg);
  if (tieneCfg) {
    Serial.printf("[cfg] cargada: ssid=%s dir=%s lat=%.4f lon=%.4f r=%d\n",
                  g_cfg.ssid.c_str(), g_cfg.direccion.c_str(),
                  g_cfg.lat, g_cfg.lon, g_cfg.radio_km);
    if (conectarWifi()) {
      modoRadar();
      return;
    }
    Serial.println("[wifi] no conecta, entrando en portal");
  } else {
    Serial.println("[cfg] no hay config, entrando en portal");
  }
  WifiPortal::ejecutar(g_http);   // no retorna
}

void loop() {
  // Todo en tasks.
  delay(1000);
}
```

- [ ] **Step 2: Compilar el firmware completo**

Run:
```
~/.platformio/penv/bin/pio run -e esp32dev
```
Expected: `SUCCESS`. Si hay errores de undefined reference, revisar los `#include` de main.

- [ ] **Step 3: Commit**

```
git add src/main.cpp
git commit -m "feat: main.cpp orquestador — decide portal vs radar y arranca tasks"
```

---

## Task 13: Flash del firmware + subida de LittleFS a la placa

**Files:** (ninguno)

- [ ] **Step 1: Flashear el firmware**

Con la placa conectada por USB:
```
~/.platformio/penv/bin/pio run -e esp32dev -t upload --upload-port /dev/cu.usbserial-120
```
Expected: `SUCCESS` y `Hard resetting via RTS pin...`.

- [ ] **Step 2: Subir el filesystem LittleFS con `data/`**

```
~/.platformio/penv/bin/pio run -e esp32dev -t uploadfs --upload-port /dev/cu.usbserial-120
```
Expected: `SUCCESS`. Sube los HTML/CSS/JS a la partición LittleFS.

- [ ] **Step 3: Abrir el monitor serie**

```
~/.platformio/penv/bin/pio device monitor -p /dev/cu.usbserial-120 -b 115200
```
Expected en boot inicial (sin config previa):
```
===== Radar de vuelos ESP32 =====
[cfg] no hay config, entrando en portal
[portal] AP levantado: RadarVuelos-XXXX  IP: 192.168.4.1
[portal] esperando configuración...
```
El LED interno parpadea a 1 Hz.

- [ ] **Step 4: Commit sanity (nada nuevo que commitear, sólo verificar)**

```
git status
```
Expected: `nothing to commit, working tree clean`.

---

## Task 14: Validación end-to-end en placa (checklist manual)

**Files:** (ninguno — sólo verificación)

Ejecuta cada punto y marca antes de continuar al siguiente.

- [ ] **1. Portal visible desde el móvil**

Con la placa recién flasheada, aparece el AP `RadarVuelos-XXXX`. Conectarse (sin password de AP por defecto).

- [ ] **2. Formulario carga y envía**

Abrir `http://192.168.4.1/` en el navegador → aparece `portal.html`. Rellenar SSID, password del WiFi de casa, dirección real (ej. "Puerta del Sol, Madrid, España"), radio 25. Enviar.

Expected: mensaje "¡Guardado! La placa se reiniciará en unos segundos." y en el monitor serie:
```
[portal] guardando cfg
[wifi] conectado, IP: 192.168.1.xx
[radar] modo operativo
```

- [ ] **3. Modo Radar accesible desde LAN**

Anotar la IP de la placa (aparece en el monitor serie). Abrir `http://<ip>/` desde el móvil o portátil (misma LAN).

Expected: se ve el radar polar con círculos concéntricos y, en < 5 s, aparecen aviones dibujados (si hay tráfico en 25 km). La tabla se puebla con callsigns.

- [ ] **4. Página `/config` precarga valores actuales**

Abrir `http://<ip>/config`. Los campos aparecen rellenos con la config actual.

- [ ] **5. Reconfiguración con dirección nueva funciona**

Cambiar la dirección a otro sitio conocido. Guardar. La placa reinicia y vuelve al radar con nuevas coordenadas.

- [ ] **6. Portal con dirección inventada muestra error y no guarda**

`POST /api/reset` desde la web `/config` → placa vuelve al portal. Introducir "asdfghjkl asdfghjkl" como dirección → mensaje "Dirección no encontrada. Prueba a añadir ciudad y país.". Config no se guarda.

- [ ] **7. Auto-reboot si WiFi cae 60 s**

Apagar el router (o desasociar la placa por MAC). En < 90 s la placa debería reiniciarse (LED va a error, luego reboot y vuelta a portal si la red sigue caída, o vuelta al radar si vuelve).

- [ ] **8. Reset total desde `/config` funciona**

En `/config`, botón "Borrar config y volver al portal". Confirmar. La placa reinicia y vuelve al portal con AP `RadarVuelos-XXXX`.

- [ ] **9. Commit final (si se han hecho ajustes)**

```
git status
git log --oneline
```
Expected: la historia se ve limpia (~14 commits).

---

## Self-review post-plan

Antes de dar el plan por bueno, verificaciones internas:

- **Cobertura del spec:**
  - Sección 2 (Requisitos funcionales) puntos 1-8 → cubiertos en Tasks 5, 10, 12 (portal), 3 (geocoding), 4 (fetch), 11 (web), 7 (LED), 12 (reboot 60s).
  - Sección 6 (Flujo) — 6.1 en Task 12, 6.2 en Task 10, 6.3 en Task 12, 6.4 en Task 9, 6.5 en Task 11, 6.6 en Task 11.
  - Sección 7 (Errores) — 7.1 en Task 5 (magic byte test), 7.2 en Task 12, 7.3 en Task 12 (reintento), 7.4 en Task 10 y 11 (rechazo POST), 7.5 en Task 4 y 3 (tests malformed), 7.6 en Task 4 (test truncado), 7.7 comentado en Task 12 (TWDT default). Cubierto.
  - Sección 8 (Tests) — Tasks 1, 3, 4, 5 tienen los cuatro suites nombrados en el spec. Cubierto.

- **Consistencia de tipos:** `Aeronave` (no `Aircraft`) usado consistentemente desde Task 4 hasta Task 11. `Config` con mismos campos en Tasks 5, 10, 11, 12.

- **Placeholders:** ninguno. Todos los steps tienen código real o comandos ejecutables.

---

## Notas y post-implementación

- **Backup del firmware original:** se conserva en `backup_flash_original_4MB.bin` (excluido del git). Restauración:
  ```
  ~/.platformio/penv/bin/python ~/.platformio/packages/tool-esptoolpy/esptool.py \
    --chip esp32 --port /dev/cu.usbserial-120 --baud 460800 \
    write_flash 0x0 backup_flash_original_4MB.bin
  ```
- **v2 candidatos (fuera de este plan):** OTA, pantalla OLED, WS2812, filtrado por altitud, logs syslog.
- **Convención de commits:** todos en español, un commit por tarea completada (el plan lo especifica en cada task).
