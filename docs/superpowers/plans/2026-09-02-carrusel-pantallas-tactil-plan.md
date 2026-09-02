# Plan — Carrusel de pantallas con panel táctil

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Habilitar el panel táctil XPT2046 del CYD, introducir una arquitectura de pantallas con menú y carrusel/fijo configurable, y permitir reconfigurar la localización desde el propio menú mediante un QR de la LAN — sin romper el radar actual.

**Architecture:** Se refactoriza `display_radar` en libs específicas (`tft_driver`, `qr_view`, `pantalla_radar`) y se añaden módulos nuevos (`touch`, `pantallas` con interfaz `Pantalla` + `GestorPantallas`, `pantalla_menu`, `pantalla_ajustes`). La task de display existente pasa a llamar a `GestorPantallas::tick`, que orquesta la vista activa, la rotación del carrusel, el stack de sub-menús y la barra superior común con el botón "☰". La lógica testeable en native (detector de gestos, migración de `Config` v1→v2, `GestorPantallas`) queda desacoplada de Arduino con interfaces inyectables.

**Tech Stack:** ESP32 Arduino framework, PlatformIO, TFT_eSPI (ILI9341 320×240), `PaulStoffregen/XPT2046_Touchscreen`, ArduinoJson, ricmoo/QRCode, Preferences (NVS), FreeRTOS tasks + queues, Unity para tests native.

**Spec:** [`docs/superpowers/specs/2026-09-02-carrusel-pantallas-tactil-design.md`](../specs/2026-09-02-carrusel-pantallas-tactil-design.md)

---

## Mapa de ficheros

| Fichero | Responsabilidad | Estado |
|---|---|---|
| `platformio.ini` | Añadir dep XPT2046 + build flags de pines touch | modificar |
| `lib/config_store/src/config_store.h` | Campos v2 en `Config`, bump `VERSION=2` | modificar |
| `lib/config_store/src/config_store.cpp` | Serializar/deserializar v2 con migración v1→v2 | modificar |
| `test/test_config_store/test_main.cpp` | Tests roundtrip v2 y migración v1→v2 | ampliar |
| `lib/qr_view/src/qr_view.h` | API `pintarQR(titulo, url, subtexto)` | crear |
| `lib/qr_view/src/qr_view.cpp` | Implementación con TFT_eSPI + ricmoo/QRCode | crear |
| `lib/qr_view/library.json` | Manifest de la lib | crear |
| `lib/tft_driver/src/tft_driver.h` | Init TFT + backlight, singleton `getTft()` | crear |
| `lib/tft_driver/src/tft_driver.cpp` | Implementación | crear |
| `lib/tft_driver/library.json` | Manifest de la lib | crear |
| `lib/touch/src/gesture_detector.h` | Detector de gestos puro (testeable en native) | crear |
| `lib/touch/src/gesture_detector.cpp` | Implementación | crear |
| `lib/touch/src/touch.h` | Wrapper Arduino XPT2046 + cola FreeRTOS | crear |
| `lib/touch/src/touch.cpp` | Task de poll y calibración | crear |
| `lib/touch/library.json` | Manifest de la lib | crear |
| `test/test_gesture_detector/test_main.cpp` | Tests native del detector | crear |
| `lib/pantallas/src/pantalla.h` | Interfaz `Pantalla` | crear |
| `lib/pantallas/src/renderizador_ui.h` | Interfaz `IRenderizadorUi` (barra + limpiar) | crear |
| `lib/pantallas/src/gestor_pantallas.h` | API pública del gestor | crear |
| `lib/pantallas/src/gestor_pantallas.cpp` | Lógica pura testeable | crear |
| `lib/pantallas/library.json` | Manifest | crear |
| `test/test_gestor_pantallas/test_main.cpp` | Tests native con `PantallaFake` | crear |
| `lib/pantalla_radar/src/pantalla_radar.h` | `PantallaRadar : Pantalla` | crear |
| `lib/pantalla_radar/src/pantalla_radar.cpp` | Migra sonar (sprite 220×220 + panel 100×220) | crear |
| `lib/pantalla_radar/library.json` | Manifest | crear |
| `lib/pantalla_menu/src/pantalla_menu.h` | Home con lista de vistas + Ajustes | crear |
| `lib/pantalla_menu/src/pantalla_menu.cpp` | Implementación | crear |
| `lib/pantalla_menu/library.json` | Manifest | crear |
| `lib/pantalla_proximamente/src/pantalla_proximamente.h` | Placeholder genérico parametrizable | crear |
| `lib/pantalla_proximamente/src/pantalla_proximamente.cpp` | Implementación | crear |
| `lib/pantalla_proximamente/library.json` | Manifest | crear |
| `lib/pantalla_ajustes/src/pantalla_ajustes.h` | Menú de Ajustes principal | crear |
| `lib/pantalla_ajustes/src/pantalla_ajustes.cpp` | Implementación | crear |
| `lib/pantalla_ajustes/src/pantalla_intervalo.{h,cpp}` | Picker −/+ | crear |
| `lib/pantalla_ajustes/src/pantalla_seleccion_vistas.{h,cpp}` | Checklist + reorden | crear |
| `lib/pantalla_ajustes/src/pantalla_seleccion_vista_fija.{h,cpp}` | Selector | crear |
| `lib/pantalla_ajustes/src/pantalla_config_localizacion.{h,cpp}` | QR LAN | crear |
| `lib/pantalla_ajustes/src/pantalla_calibrar_touch.{h,cpp}` | 4 esquinas | crear |
| `lib/pantalla_ajustes/src/pantalla_confirmar_reset.{h,cpp}` | Confirmación | crear |
| `lib/pantalla_ajustes/library.json` | Manifest | crear |
| `src/main.cpp` | Ensamblar gestor + touch + registro de pantallas | modificar |
| `lib/display_radar/` | Eliminar tras migración completa | borrar |

---

## Task 1: Preparar entorno (dep XPT2046 + build flags de pines)

**Files:**
- Modify: `platformio.ini`

Objetivo: dejar el proyecto compilando con la nueva dep antes de escribir código, para que las tareas siguientes no se compliquen con problemas de build.

- [ ] **Step 1: Editar `platformio.ini` con la dep del touch y los pines del CYD**

Modificar el bloque `[env:esp32dev]` para dejar así la sección `lib_deps` y las `build_flags` (añadir la dep y las 5 macros de pines al final de `build_flags`, sin tocar lo existente):

```ini
lib_deps =
  bblanchon/ArduinoJson@^7.0.4
  esphome/ESPAsyncWebServer-esphome@^3.2.2
  esphome/AsyncTCP-esphome@^2.1.4
  bodmer/TFT_eSPI@^2.5.43
  ricmoo/QRCode@^0.0.1
  paulstoffregen/XPT2046_Touchscreen@^1.4
build_flags =
  -DCORE_DEBUG_LEVEL=3
  -std=gnu++17
  -DUSER_SETUP_LOADED=1
  -DILI9341_2_DRIVER=1
  -DTFT_WIDTH=240
  -DTFT_HEIGHT=320
  -DTFT_MISO=12
  -DTFT_MOSI=13
  -DTFT_SCLK=14
  -DTFT_CS=15
  -DTFT_DC=2
  -DTFT_RST=-1
  -DTFT_BL=21
  -DTFT_BACKLIGHT_ON=HIGH
  -DTFT_INVERSION_ON=1
  -DLOAD_GLCD=1
  -DLOAD_FONT2=1
  -DLOAD_FONT4=1
  -DSPI_FREQUENCY=55000000
  ; Panel táctil XPT2046 (bus SPI aparte del TFT en la CYD ESP32-2432S028R)
  -DTOUCH_CS=33
  -DTOUCH_CLK=25
  -DTOUCH_MOSI=32
  -DTOUCH_MISO=39
  -DTOUCH_IRQ=36
```

- [ ] **Step 2: Compilar para verificar que la dep se resuelve**

Run: `~/.platformio/penv/bin/pio run -e esp32dev -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos"`
Expected: `[SUCCESS]`, sin errores; `XPT2046_Touchscreen` aparece en el listado de deps descargadas.

- [ ] **Step 3: Commit**

```bash
git add platformio.ini
git commit -m "feat: dep XPT2046_Touchscreen + build flags de pines del touch del CYD"
```

---

## Task 2: `ConfigStore` v2 — extensión + migración v1→v2

**Files:**
- Modify: `lib/config_store/src/config_store.h`
- Modify: `lib/config_store/src/config_store.cpp`
- Modify: `test/test_config_store/test_main.cpp`

- [ ] **Step 1: Escribir el test roundtrip v2 (falla porque los campos v2 aún no existen)**

Añadir al principio de `test/test_config_store/test_main.cpp` (tras el `#include <cstdint>`):

```cpp
void test_serializar_y_deserializar_v2_roundtrip(void) {
  Config in;
  in.ssid = "MiWifi";
  in.password = "secreto123";
  in.direccion = "Calle Mayor 1, Madrid";
  in.lat = 40.4168;
  in.lon = -3.7038;
  in.radio_km = 25;
  in.modo = ModoVista::FIJO;
  in.intervalo_carrusel_s = 30;
  in.vista_fija = 2;
  in.vistas_orden = {0, 2, 5};
  in.touch_min_x = 200;
  in.touch_max_x = 3800;
  in.touch_min_y = 240;
  in.touch_max_y = 3900;
  in.touch_calibrado = true;

  std::vector<uint8_t> buf;
  ConfigStore::serializar(in, buf);
  TEST_ASSERT_TRUE(buf.size() > 0);

  Config out;
  bool ok = ConfigStore::deserializar(buf.data(), buf.size(), out);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL_STRING("MiWifi", out.ssid.c_str());
  TEST_ASSERT_EQUAL(25, out.radio_km);
  TEST_ASSERT_EQUAL(static_cast<int>(ModoVista::FIJO), static_cast<int>(out.modo));
  TEST_ASSERT_EQUAL(30, out.intervalo_carrusel_s);
  TEST_ASSERT_EQUAL(2, out.vista_fija);
  TEST_ASSERT_EQUAL(3, (int)out.vistas_orden.size());
  TEST_ASSERT_EQUAL(0, out.vistas_orden[0]);
  TEST_ASSERT_EQUAL(2, out.vistas_orden[1]);
  TEST_ASSERT_EQUAL(5, out.vistas_orden[2]);
  TEST_ASSERT_EQUAL(200, out.touch_min_x);
  TEST_ASSERT_EQUAL(3800, out.touch_max_x);
  TEST_ASSERT_TRUE(out.touch_calibrado);
}
```

Y añadir un test de migración v1→v2 justo después:

```cpp
void test_deserializar_buffer_v1_rellena_defaults_v2(void) {
  // Construimos manualmente un buffer v1 (magic 0xC0DE, version 1, lat, lon,
  // radio, ssid, password, direccion). Sin campos v2.
  std::vector<uint8_t> buf;
  // magic 0xC0DE
  buf.push_back(0xDE); buf.push_back(0xC0);
  // version 1
  buf.push_back(0x01);
  // lat = 41.0 (double LE)
  double lat = 41.0, lon = 2.0;
  const uint8_t* platlat = reinterpret_cast<const uint8_t*>(&lat);
  buf.insert(buf.end(), platlat, platlat + sizeof(double));
  const uint8_t* platlon = reinterpret_cast<const uint8_t*>(&lon);
  buf.insert(buf.end(), platlon, platlon + sizeof(double));
  // radio 30 (u16 LE)
  buf.push_back(30); buf.push_back(0);
  // ssid "Red" (u16 len + bytes)
  const char* ssid = "Red";
  buf.push_back(3); buf.push_back(0);
  buf.insert(buf.end(), ssid, ssid + 3);
  // password "clave"
  const char* pass = "clave";
  buf.push_back(5); buf.push_back(0);
  buf.insert(buf.end(), pass, pass + 5);
  // direccion "BCN"
  const char* dir = "BCN";
  buf.push_back(3); buf.push_back(0);
  buf.insert(buf.end(), dir, dir + 3);

  Config out;
  bool ok = ConfigStore::deserializar(buf.data(), buf.size(), out);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL_STRING("Red", out.ssid.c_str());
  TEST_ASSERT_EQUAL_STRING("clave", out.password.c_str());
  TEST_ASSERT_EQUAL_STRING("BCN", out.direccion.c_str());
  TEST_ASSERT_EQUAL(30, out.radio_km);
  // Defaults v2:
  TEST_ASSERT_EQUAL(static_cast<int>(ModoVista::CARRUSEL), static_cast<int>(out.modo));
  TEST_ASSERT_EQUAL(10, out.intervalo_carrusel_s);
  TEST_ASSERT_EQUAL(0, out.vista_fija);
  TEST_ASSERT_EQUAL(6, (int)out.vistas_orden.size());
  TEST_ASSERT_FALSE(out.touch_calibrado);
}
```

Y registrarlos en `main`:

```cpp
int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_serializar_y_deserializar_roundtrip);
  RUN_TEST(test_deserializar_magic_byte_erroneo);
  RUN_TEST(test_deserializar_buffer_muy_pequeno);
  RUN_TEST(test_deserializar_buffer_nulo);
  RUN_TEST(test_serializar_y_deserializar_v2_roundtrip);
  RUN_TEST(test_deserializar_buffer_v1_rellena_defaults_v2);
  return UNITY_END();
}
```

- [ ] **Step 2: Ejecutar tests para confirmar fallos**

Run: `~/.platformio/penv/bin/pio test -e native -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos"`
Expected: fallo de compilación por `ModoVista` no declarado y `Config` sin los campos nuevos.

- [ ] **Step 3: Extender `Config` y `ConfigStore` con la v2**

Reemplazar el contenido de `lib/config_store/src/config_store.h` por:

```cpp
#pragma once
#include <string>
#include <vector>
#include <cstdint>

enum class ModoVista : uint8_t { FIJO = 0, CARRUSEL = 1 };

struct Config {
  // v1 (persistido desde el inicio del proyecto):
  std::string ssid;
  std::string password;
  std::string direccion;
  double lat = 0.0;
  double lon = 0.0;
  int    radio_km = 25;

  // v2:
  ModoVista modo = ModoVista::CARRUSEL;
  uint16_t  intervalo_carrusel_s = 10;
  uint8_t   vista_fija = 0;
  std::vector<uint8_t> vistas_orden = {0, 1, 2, 3, 4, 5};

  int16_t touch_min_x = 0;
  int16_t touch_max_x = 0;
  int16_t touch_min_y = 0;
  int16_t touch_max_y = 0;
  bool    touch_calibrado = false;
};

// Persistencia en NVS mediante Preferences. La serialización a bytes se expone
// como funciones estáticas para poder testearla en native (sin depender de NVS).
//
// Layout binario (little-endian):
//   [0..1]   magic 0xC0DE
//   [2]      version (1 o 2)
//   Después, los campos v1 tal como estaban:
//     lat (double), lon (double), radio_km (u16),
//     ssid (u16 len + bytes), password (u16 len + bytes), direccion (u16 len + bytes)
//   Si version == 2, a continuación:
//     modo (u8), intervalo (u16), vista_fija (u8),
//     n_vistas (u8), vistas_orden[n_vistas] (u8 c/u),
//     touch_min_x (i16), touch_max_x (i16), touch_min_y (i16), touch_max_y (i16),
//     touch_calibrado (u8)
class ConfigStore {
 public:
  static constexpr uint16_t MAGIC = 0xC0DE;
  static constexpr uint8_t  VERSION = 2;
  static constexpr size_t   MAX_STR = 128;

  static void serializar(const Config& in, std::vector<uint8_t>& out);
  static bool deserializar(const uint8_t* data, size_t size, Config& out);

#ifndef UNIT_TEST
  static bool cargar(Config& out);
  static bool guardar(const Config& cfg);
  static void borrar();
#endif
};
```

Reemplazar `lib/config_store/src/config_store.cpp` por:

```cpp
#include "config_store.h"
#include <cstring>

#ifndef UNIT_TEST
#include <Preferences.h>
static const char* NVS_NAMESPACE = "radarvuelos";
static const char* NVS_KEY = "cfg";
#endif

namespace {

void escribirU8(std::vector<uint8_t>& v, uint8_t x) { v.push_back(x); }

void escribirU16(std::vector<uint8_t>& v, uint16_t x) {
  v.push_back(static_cast<uint8_t>(x & 0xFF));
  v.push_back(static_cast<uint8_t>((x >> 8) & 0xFF));
}

void escribirI16(std::vector<uint8_t>& v, int16_t x) {
  escribirU16(v, static_cast<uint16_t>(x));
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

bool leerU8(const uint8_t* data, size_t size, size_t& pos, uint8_t& out) {
  if (pos + 1 > size) return false;
  out = data[pos++];
  return true;
}

bool leerU16(const uint8_t* data, size_t size, size_t& pos, uint16_t& out) {
  if (pos + 2 > size) return false;
  out = static_cast<uint16_t>(data[pos]) |
        (static_cast<uint16_t>(data[pos + 1]) << 8);
  pos += 2;
  return true;
}

bool leerI16(const uint8_t* data, size_t size, size_t& pos, int16_t& out) {
  uint16_t u;
  if (!leerU16(data, size, pos, u)) return false;
  out = static_cast<int16_t>(u);
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
  escribirU8 (out, VERSION);
  escribirDouble(out, in.lat);
  escribirDouble(out, in.lon);
  escribirU16(out, static_cast<uint16_t>(in.radio_km));
  escribirString(out, in.ssid);
  escribirString(out, in.password);
  escribirString(out, in.direccion);
  // v2:
  escribirU8 (out, static_cast<uint8_t>(in.modo));
  escribirU16(out, in.intervalo_carrusel_s);
  escribirU8 (out, in.vista_fija);
  uint8_t n = static_cast<uint8_t>(std::min<size_t>(in.vistas_orden.size(), 32));
  escribirU8(out, n);
  for (uint8_t i = 0; i < n; ++i) escribirU8(out, in.vistas_orden[i]);
  escribirI16(out, in.touch_min_x);
  escribirI16(out, in.touch_max_x);
  escribirI16(out, in.touch_min_y);
  escribirI16(out, in.touch_max_y);
  escribirU8 (out, in.touch_calibrado ? 1 : 0);
}

bool ConfigStore::deserializar(const uint8_t* data, size_t size, Config& out) {
  if (!data || size < 4) return false;
  size_t pos = 0;
  uint16_t magic;
  if (!leerU16(data, size, pos, magic) || magic != MAGIC) return false;
  uint8_t version;
  if (!leerU8(data, size, pos, version)) return false;
  if (version != 1 && version != 2) return false;

  double lat, lon;
  if (!leerDouble(data, size, pos, lat)) return false;
  if (!leerDouble(data, size, pos, lon)) return false;
  uint16_t radio;
  if (!leerU16(data, size, pos, radio)) return false;
  std::string ssid, pass, dir;
  if (!leerString(data, size, pos, ssid)) return false;
  if (!leerString(data, size, pos, pass)) return false;
  if (!leerString(data, size, pos, dir))  return false;

  Config tmp;  // parte de los defaults del struct
  tmp.ssid = std::move(ssid);
  tmp.password = std::move(pass);
  tmp.direccion = std::move(dir);
  tmp.lat = lat;
  tmp.lon = lon;
  tmp.radio_km = radio;

  if (version == 2) {
    uint8_t modoU8;
    if (!leerU8(data, size, pos, modoU8)) return false;
    tmp.modo = (modoU8 == 0) ? ModoVista::FIJO : ModoVista::CARRUSEL;
    if (!leerU16(data, size, pos, tmp.intervalo_carrusel_s)) return false;
    if (!leerU8 (data, size, pos, tmp.vista_fija))          return false;
    uint8_t n;
    if (!leerU8(data, size, pos, n)) return false;
    if (n > 32) return false;
    tmp.vistas_orden.clear();
    tmp.vistas_orden.reserve(n);
    for (uint8_t i = 0; i < n; ++i) {
      uint8_t vid;
      if (!leerU8(data, size, pos, vid)) return false;
      tmp.vistas_orden.push_back(vid);
    }
    if (!leerI16(data, size, pos, tmp.touch_min_x)) return false;
    if (!leerI16(data, size, pos, tmp.touch_max_x)) return false;
    if (!leerI16(data, size, pos, tmp.touch_min_y)) return false;
    if (!leerI16(data, size, pos, tmp.touch_max_y)) return false;
    uint8_t calibU8;
    if (!leerU8(data, size, pos, calibU8)) return false;
    tmp.touch_calibrado = (calibU8 != 0);
  }
  // Si version == 1, los campos v2 conservan los defaults del struct.

  out = std::move(tmp);
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

- [ ] **Step 4: Correr los tests native y confirmar que pasan**

Run: `~/.platformio/penv/bin/pio test -e native -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos"`
Expected: 6 tests OK. Los 4 existentes siguen pasando (roundtrip v1 sigue siendo válido porque `MAGIC` no cambió y ahora todos los `serializar()` producen v2, pero los buffers viejos v1 se aceptan al deserializar).

Nota: el test antiguo `test_serializar_y_deserializar_roundtrip` sigue verde porque hace roundtrip: aunque ahora se serializa como v2, deserializar produce el mismo `Config` en los campos v1 comparados.

- [ ] **Step 5: Compilar firmware para verificar que main.cpp sigue enlazando**

Run: `~/.platformio/penv/bin/pio run -e esp32dev -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos"`
Expected: `[SUCCESS]`.

- [ ] **Step 6: Commit**

```bash
git add lib/config_store test/test_config_store
git commit -m "feat(config): ConfigStore v2 con modo carrusel, orden de vistas y calibración táctil + migración v1"
```

---

## Task 3: `qr_view` — extraer helper QR

**Files:**
- Create: `lib/qr_view/library.json`
- Create: `lib/qr_view/src/qr_view.h`
- Create: `lib/qr_view/src/qr_view.cpp`
- Modify: `lib/display_radar/src/display_radar.cpp` (delegar `pintarPortalQR` a `qr_view`)

Objetivo: extraer el QR a una lib reutilizable sin cambiar de comportamiento visible.

- [ ] **Step 1: Crear `lib/qr_view/library.json`**

```json
{
  "name": "qr_view",
  "version": "0.1.0",
  "description": "Helper para pintar QR sobre TFT_eSPI en el radar de vuelos",
  "dependencies": {
    "bodmer/TFT_eSPI": "^2.5.43",
    "ricmoo/QRCode": "^0.0.1"
  }
}
```

- [ ] **Step 2: Crear `lib/qr_view/src/qr_view.h`**

```cpp
#pragma once
#include <string>

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
```

- [ ] **Step 3: Crear `lib/qr_view/src/qr_view.cpp`**

```cpp
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
  tft.setTextFont(2);
  tft.setCursor(6, 4);
  tft.print(titulo.c_str());

  // Área izquierda: cuadrado 240x240. QR centrado con escala 6.
  constexpr int LADO_IZQ = 240;
  constexpr int ESCALA = 6;
  // Estimación del tamaño para centrar (v2..v5 → 25..37 módulos).
  const int version = (url.size() <= 32) ? 2 : (url.size() <= 53) ? 3 : (url.size() <= 78) ? 4 : 5;
  const int tamModulos = 17 + version * 4;  // regla del QR
  const int lado = tamModulos * ESCALA;
  const int qrX = (LADO_IZQ - lado) / 2;
  const int qrY = 26;

  pintarSoloQR(tft, qrX, qrY, ESCALA, url, /*margen=*/6);

  tft.setTextColor(COL_URL, COL_FONDO);
  tft.setTextFont(1);
  tft.setCursor(6, qrY + lado + 12);
  tft.print(url.c_str());

  // Panel derecho: cada línea 18 px de alto en font 2.
  int py = 32;
  tft.setTextColor(COL_PANEL, COL_FONDO);
  tft.setTextFont(2);
  for (const auto& l : lineasPanel) {
    tft.setCursor(LADO_IZQ + 4, py);
    tft.print(l.c_str());
    py += 20;
  }
}

}  // namespace qr_view
```

- [ ] **Step 4: Delegar `DisplayRadar::pintarPortalQR` a `qr_view`**

Reemplazar el cuerpo actual de `DisplayRadar::pintarPortalQR` en `lib/display_radar/src/display_radar.cpp` por:

```cpp
void DisplayRadar::pintarPortalQR(const std::string& ssidAp, const std::string& url) {
  if (!s_iniciado) return;
  std::vector<std::string> lineas = {
    "1. WiFi:",
    ssidAp,
    "",
    "2. Escanea",
    "   el QR",
    "",
    "o abre la",
    "URL a mano",
  };
  qr_view::pintarPortalConQR(s_tft, "Modo Portal", url, lineas);
}
```

Y añadir `#include "qr_view.h"` en la cabecera de includes del mismo `.cpp`.

- [ ] **Step 5: Compilar firmware**

Run: `~/.platformio/penv/bin/pio run -e esp32dev -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos"`
Expected: `[SUCCESS]`.

- [ ] **Step 6: Commit**

```bash
git add lib/qr_view lib/display_radar
git commit -m "refactor(qr): extraer QR portal a lib qr_view; display_radar delega en ella"
```

---

## Task 4: `tft_driver` — extraer init TFT + backlight

**Files:**
- Create: `lib/tft_driver/library.json`
- Create: `lib/tft_driver/src/tft_driver.h`
- Create: `lib/tft_driver/src/tft_driver.cpp`
- Modify: `lib/display_radar/src/display_radar.cpp` (usar `tft_driver::getTft()`)

- [ ] **Step 1: Crear `lib/tft_driver/library.json`**

```json
{
  "name": "tft_driver",
  "version": "0.1.0",
  "description": "Inicialización compartida del TFT y helpers básicos",
  "dependencies": {
    "bodmer/TFT_eSPI": "^2.5.43"
  }
}
```

- [ ] **Step 2: Crear `lib/tft_driver/src/tft_driver.h`**

```cpp
#pragma once
#include <string>

class TFT_eSPI;

namespace tft_driver {

// Inicializa el TFT (rotación landscape 320x240), backlight en GPIO 21 HIGH,
// rellena la pantalla de negro. Idempotente.
void iniciar();

// Devuelve la instancia global de TFT_eSPI. Necesario porque TFT_eSPI ocupa
// bastante RAM y no queremos duplicarla por pantalla.
TFT_eSPI& getTft();

// Pinta un splash centrado de dos líneas (título grande + detalle). Usado
// antes de que exista GestorPantallas (splash de arranque, "conectando WiFi").
void pintarSplash(const std::string& titulo, const std::string& detalle);

}  // namespace tft_driver
```

- [ ] **Step 3: Crear `lib/tft_driver/src/tft_driver.cpp`**

```cpp
#include "tft_driver.h"
#include <Arduino.h>
#include <TFT_eSPI.h>

namespace tft_driver {

namespace {
TFT_eSPI s_tft;
bool     s_iniciado = false;

constexpr uint16_t COL_FONDO = 0x0000;
constexpr uint16_t COL_PANEL = 0x07E0;
}  // namespace

void iniciar() {
  if (s_iniciado) return;
  s_tft.init();
  s_tft.setRotation(1);
  s_tft.fillScreen(COL_FONDO);
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);
  s_iniciado = true;
}

TFT_eSPI& getTft() { return s_tft; }

void pintarSplash(const std::string& titulo, const std::string& detalle) {
  if (!s_iniciado) return;
  s_tft.fillScreen(COL_FONDO);
  s_tft.setTextColor(COL_PANEL, COL_FONDO);
  s_tft.setTextFont(4);
  s_tft.setCursor(10, 60);
  s_tft.print(titulo.c_str());
  s_tft.setTextFont(2);
  s_tft.setCursor(10, 110);
  s_tft.print(detalle.c_str());
}

}  // namespace tft_driver
```

- [ ] **Step 4: Migrar `DisplayRadar::iniciar` para usar `tft_driver`**

En `lib/display_radar/src/display_radar.cpp`, sustituir el bloque `namespace { TFT_eSPI s_tft; ...` por una referencia externa y adaptar `iniciar()`:

```cpp
#include "tft_driver.h"
// ...
namespace {
TFT_eSprite    s_sprite(&tft_driver::getTft());
bool           s_iniciado = false;
// ... (constantes de colores igual)
}

void DisplayRadar::iniciar() {
  if (s_iniciado) return;
  tft_driver::iniciar();
  s_sprite.setColorDepth(8);
  void* p = s_sprite.createSprite(RADAR_LADO, RADAR_LADO);
  Serial.printf("[display] createSprite %dx%d 8bpp -> %s (heap %u)\n",
                RADAR_LADO, RADAR_LADO, p ? "OK" : "FAIL",
                (unsigned)ESP.getFreeHeap());
  if (p) s_sprite.fillSprite(COL_FONDO);
  s_iniciado = true;
}
```

Reemplazar todas las referencias a `s_tft` en el resto del fichero por `tft_driver::getTft()`.

Y en `DisplayRadar::pintarMensaje`, redirigir a `tft_driver::pintarSplash`:

```cpp
void DisplayRadar::pintarMensaje(const std::string& titulo, const std::string& detalle) {
  tft_driver::pintarSplash(titulo, detalle);
}
```

- [ ] **Step 5: Compilar firmware**

Run: `~/.platformio/penv/bin/pio run -e esp32dev -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos"`
Expected: `[SUCCESS]`. El radar sigue funcionando igual (verificaremos en placa al final).

- [ ] **Step 6: Commit**

```bash
git add lib/tft_driver lib/display_radar
git commit -m "refactor(tft): extraer init TFT y splash a lib tft_driver"
```

---

## Task 5: `touch::GestureDetector` — detector de gestos puro (testeable en native)

**Files:**
- Create: `lib/touch/library.json`
- Create: `lib/touch/src/gesture_detector.h`
- Create: `lib/touch/src/gesture_detector.cpp`
- Create: `test/test_gesture_detector/test_main.cpp`

- [ ] **Step 1: Crear `lib/touch/library.json`**

```json
{
  "name": "touch",
  "version": "0.1.0",
  "description": "Panel táctil XPT2046 + detector de gestos + cola de eventos"
}
```

- [ ] **Step 2: Escribir el test primero (falla porque el detector no existe aún)**

`test/test_gesture_detector/test_main.cpp`:

```cpp
#include <unity.h>
#include "gesture_detector.h"

using touch::EventoTactil;
using touch::GestureDetector;
using touch::TipoEvento;

void test_tap_corto_produce_evento_tap(void) {
  GestureDetector g;
  g.onPress(100, 120, /*ms=*/1000);
  auto ev = g.onRelease(102, 118, /*ms=*/1100);
  TEST_ASSERT_TRUE(ev.has_value());
  TEST_ASSERT_EQUAL(static_cast<int>(TipoEvento::TAP), static_cast<int>(ev->tipo));
  TEST_ASSERT_EQUAL(100, ev->x);
  TEST_ASSERT_EQUAL(120, ev->y);
}

void test_pulsacion_larga_no_produce_evento(void) {
  GestureDetector g;
  g.onPress(50, 50, 1000);
  auto ev = g.onRelease(50, 50, 1400);  // 400 ms > umbral tap 300 ms
  TEST_ASSERT_FALSE(ev.has_value());
}

void test_desplazamiento_grande_no_es_tap(void) {
  GestureDetector g;
  g.onPress(50, 50, 1000);
  auto ev = g.onRelease(80, 60, 1200);  // dx=30 > umbral 20
  TEST_ASSERT_FALSE(ev.has_value());
}

void test_swipe_derecha_produce_evento(void) {
  GestureDetector g;
  g.onPress(20, 100, 1000);
  auto ev = g.onRelease(120, 105, 1200);  // dx=100 en 200 ms, dy pequeño
  TEST_ASSERT_TRUE(ev.has_value());
  TEST_ASSERT_EQUAL(static_cast<int>(TipoEvento::SWIPE_DERECHA), static_cast<int>(ev->tipo));
}

void test_swipe_izquierda_produce_evento(void) {
  GestureDetector g;
  g.onPress(200, 100, 1000);
  auto ev = g.onRelease(100, 100, 1200);
  TEST_ASSERT_TRUE(ev.has_value());
  TEST_ASSERT_EQUAL(static_cast<int>(TipoEvento::SWIPE_IZQUIERDA), static_cast<int>(ev->tipo));
}

void test_swipe_vertical_no_produce_evento(void) {
  GestureDetector g;
  g.onPress(100, 20, 1000);
  auto ev = g.onRelease(105, 180, 1200);  // dy grande, dx pequeño
  TEST_ASSERT_FALSE(ev.has_value());
}

void test_swipe_muy_lento_no_produce_evento(void) {
  GestureDetector g;
  g.onPress(20, 100, 1000);
  auto ev = g.onRelease(120, 100, 1600);  // 600 ms > umbral 400 ms
  TEST_ASSERT_FALSE(ev.has_value());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_tap_corto_produce_evento_tap);
  RUN_TEST(test_pulsacion_larga_no_produce_evento);
  RUN_TEST(test_desplazamiento_grande_no_es_tap);
  RUN_TEST(test_swipe_derecha_produce_evento);
  RUN_TEST(test_swipe_izquierda_produce_evento);
  RUN_TEST(test_swipe_vertical_no_produce_evento);
  RUN_TEST(test_swipe_muy_lento_no_produce_evento);
  return UNITY_END();
}
```

- [ ] **Step 3: Run test (falla por compilación)**

Run: `~/.platformio/penv/bin/pio test -e native -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos" -f test_gesture_detector`
Expected: FAIL. `gesture_detector.h` no encontrado.

- [ ] **Step 4: Crear `lib/touch/src/gesture_detector.h`**

```cpp
#pragma once
#include <cstdint>
#include <optional>

namespace touch {

enum class TipoEvento : uint8_t {
  TAP = 0,
  SWIPE_IZQUIERDA = 1,
  SWIPE_DERECHA   = 2,
};

struct EventoTactil {
  TipoEvento tipo;
  int16_t x;
  int16_t y;
};

// Detector puro y sin dependencias de Arduino. Recibe eventos crudos press/release
// con coordenadas en el espacio de pantalla (ya calibradas) y timestamps en ms.
// Devuelve un evento gesto si el release cierra un patrón reconocido.
//
// Umbrales:
//   TAP:   duración < 300 ms, desplazamiento < 20 px.
//   SWIPE: |dx| > 60 px, |dx| > |dy|, duración < 400 ms.
class GestureDetector {
 public:
  void onPress(int16_t x, int16_t y, uint32_t ms);
  std::optional<EventoTactil> onRelease(int16_t x, int16_t y, uint32_t ms);

  static constexpr uint32_t UMBRAL_TAP_MS   = 300;
  static constexpr int16_t  UMBRAL_TAP_PX   = 20;
  static constexpr uint32_t UMBRAL_SWIPE_MS = 400;
  static constexpr int16_t  UMBRAL_SWIPE_PX = 60;

 private:
  int16_t  x0_ = 0;
  int16_t  y0_ = 0;
  uint32_t t0_ = 0;
  bool     activo_ = false;
};

}  // namespace touch
```

- [ ] **Step 5: Crear `lib/touch/src/gesture_detector.cpp`**

```cpp
#include "gesture_detector.h"
#include <cstdlib>

namespace touch {

void GestureDetector::onPress(int16_t x, int16_t y, uint32_t ms) {
  x0_ = x; y0_ = y; t0_ = ms; activo_ = true;
}

std::optional<EventoTactil> GestureDetector::onRelease(int16_t x, int16_t y, uint32_t ms) {
  if (!activo_) return std::nullopt;
  activo_ = false;
  const int32_t dx = static_cast<int32_t>(x) - x0_;
  const int32_t dy = static_cast<int32_t>(y) - y0_;
  const uint32_t dur = (ms >= t0_) ? (ms - t0_) : 0;

  if (dur < UMBRAL_TAP_MS && std::abs(dx) < UMBRAL_TAP_PX && std::abs(dy) < UMBRAL_TAP_PX) {
    return EventoTactil{TipoEvento::TAP, x0_, y0_};
  }
  if (dur < UMBRAL_SWIPE_MS && std::abs(dx) > UMBRAL_SWIPE_PX && std::abs(dx) > std::abs(dy)) {
    return EventoTactil{dx > 0 ? TipoEvento::SWIPE_DERECHA : TipoEvento::SWIPE_IZQUIERDA,
                        x0_, y0_};
  }
  return std::nullopt;
}

}  // namespace touch
```

- [ ] **Step 6: Run tests native — todos verdes**

Run: `~/.platformio/penv/bin/pio test -e native -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos" -f test_gesture_detector`
Expected: 7 tests OK.

- [ ] **Step 7: Commit**

```bash
git add lib/touch/library.json lib/touch/src/gesture_detector.h lib/touch/src/gesture_detector.cpp test/test_gesture_detector
git commit -m "feat(touch): detector de gestos tap/swipe puro con tests native"
```

---

## Task 6: `touch::Touch` — wrapper Arduino XPT2046 + cola FreeRTOS + calibración

**Files:**
- Create: `lib/touch/src/touch.h`
- Create: `lib/touch/src/touch.cpp`

- [ ] **Step 1: Crear `lib/touch/src/touch.h`**

```cpp
#pragma once
#include "gesture_detector.h"
#include <cstdint>

namespace touch {

struct CalibracionTouch {
  int16_t min_x = 300;
  int16_t max_x = 3800;
  int16_t min_y = 300;
  int16_t max_y = 3800;
  bool    valida = false;
};

// Arranca la task de poll del XPT2046 en core 0. Convierte lecturas crudas
// a coordenadas de pantalla (320×240 en rotación landscape) con la calibración
// dada, y publica eventos gesto en una cola FreeRTOS interna.
void iniciar(const CalibracionTouch& cal);

// Actualiza la calibración usada por la task (sin necesidad de reiniciar).
void setCalibracion(const CalibracionTouch& cal);

// Espera hasta `timeoutMs` un evento gesto. Devuelve true si sacó uno; false por timeout.
bool esperarEvento(EventoTactil& out, uint32_t timeoutMs);

// Lectura cruda del panel (para el flujo de calibración). Bloqueante hasta
// detectar un press y su release. Devuelve el promedio del press.
bool leerCrudoBloqueante(int16_t& xRawOut, int16_t& yRawOut, uint32_t timeoutMs);

}  // namespace touch
```

- [ ] **Step 2: Crear `lib/touch/src/touch.cpp`**

```cpp
#include "touch.h"
#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

namespace touch {

namespace {

SPIClass* s_spi = nullptr;
XPT2046_Touchscreen* s_ts = nullptr;
CalibracionTouch s_cal;
GestureDetector s_det;
QueueHandle_t s_cola = nullptr;

int16_t mapearX(int16_t xRaw) {
  if (!s_cal.valida || s_cal.max_x <= s_cal.min_x) return 0;
  int32_t v = ((int32_t)(xRaw - s_cal.min_x) * 320) / (s_cal.max_x - s_cal.min_x);
  if (v < 0) v = 0; if (v > 319) v = 319;
  return static_cast<int16_t>(v);
}
int16_t mapearY(int16_t yRaw) {
  if (!s_cal.valida || s_cal.max_y <= s_cal.min_y) return 0;
  int32_t v = ((int32_t)(yRaw - s_cal.min_y) * 240) / (s_cal.max_y - s_cal.min_y);
  if (v < 0) v = 0; if (v > 239) v = 239;
  return static_cast<int16_t>(v);
}

void tareaTouch(void*) {
  bool estabaTocado = false;
  int16_t xr = 0, yr = 0;
  for (;;) {
    bool tocado = s_ts->touched();
    if (tocado) {
      TS_Point p = s_ts->getPoint();
      xr = p.x; yr = p.y;
      if (!estabaTocado) {
        s_det.onPress(mapearX(xr), mapearY(yr), millis());
        estabaTocado = true;
      }
    } else if (estabaTocado) {
      auto ev = s_det.onRelease(mapearX(xr), mapearY(yr), millis());
      if (ev.has_value() && s_cola != nullptr) {
        EventoTactil e = ev.value();
        xQueueSend(s_cola, &e, 0);
      }
      estabaTocado = false;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

}  // namespace

void iniciar(const CalibracionTouch& cal) {
  s_cal = cal;
  if (s_spi == nullptr) {
    s_spi = new SPIClass(HSPI);
    s_spi->begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  }
  if (s_ts == nullptr) {
    s_ts = new XPT2046_Touchscreen(TOUCH_CS, TOUCH_IRQ);
    s_ts->begin(*s_spi);
    s_ts->setRotation(1);
  }
  if (s_cola == nullptr) {
    s_cola = xQueueCreate(16, sizeof(EventoTactil));
  }
  xTaskCreatePinnedToCore(tareaTouch, "touch", 4096, nullptr, 1, nullptr, 0);
  Serial.println("[touch] task de poll arrancada");
}

void setCalibracion(const CalibracionTouch& cal) { s_cal = cal; }

bool esperarEvento(EventoTactil& out, uint32_t timeoutMs) {
  if (!s_cola) return false;
  return xQueueReceive(s_cola, &out, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

bool leerCrudoBloqueante(int16_t& xRawOut, int16_t& yRawOut, uint32_t timeoutMs) {
  const uint32_t inicio = millis();
  // Espera press
  while (!s_ts->touched()) {
    if (millis() - inicio > timeoutMs) return false;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  // Promedia 8 muestras mientras está tocando
  int32_t sx = 0, sy = 0;
  int n = 0;
  while (s_ts->touched() && n < 8) {
    TS_Point p = s_ts->getPoint();
    sx += p.x; sy += p.y;
    ++n;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  if (n == 0) return false;
  xRawOut = static_cast<int16_t>(sx / n);
  yRawOut = static_cast<int16_t>(sy / n);
  // Espera release
  while (s_ts->touched()) vTaskDelay(pdMS_TO_TICKS(20));
  return true;
}

}  // namespace touch
```

- [ ] **Step 3: Compilar firmware**

Run: `~/.platformio/penv/bin/pio run -e esp32dev -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos"`
Expected: `[SUCCESS]`.

- [ ] **Step 4: Commit**

```bash
git add lib/touch/src/touch.h lib/touch/src/touch.cpp
git commit -m "feat(touch): wrapper Arduino XPT2046 con task de poll y cola FreeRTOS"
```

---

## Task 7: `pantallas` — interfaces `Pantalla` e `IRenderizadorUi`

**Files:**
- Create: `lib/pantallas/library.json`
- Create: `lib/pantallas/src/pantalla.h`
- Create: `lib/pantallas/src/renderizador_ui.h`

- [ ] **Step 1: Crear `lib/pantallas/library.json`**

```json
{
  "name": "pantallas",
  "version": "0.1.0",
  "description": "Interfaz Pantalla + GestorPantallas (lógica pura)"
}
```

- [ ] **Step 2: Crear `lib/pantallas/src/pantalla.h`**

```cpp
#pragma once
#include <cstdint>

namespace pantallas {

class Pantalla {
 public:
  virtual ~Pantalla() = default;
  virtual const char* nombre() const = 0;
  virtual uint8_t id() const = 0;

  virtual void alEntrar() {}
  virtual void alSalir() {}

  // Coordenadas relativas al área de contenido (0..319, 0..219). Origen arriba-izq.
  virtual void alTocar(int x, int y) {}

  // direccion: -1 swipe a izquierda (siguiente), +1 swipe a derecha (anterior).
  virtual void alDeslizar(int direccion) {}

  // msAhora: reloj monótono en ms. La pantalla decide si repinta o no.
  virtual void dibujar(uint32_t msAhora) = 0;
};

}  // namespace pantallas
```

- [ ] **Step 3: Crear `lib/pantallas/src/renderizador_ui.h`**

```cpp
#pragma once
#include <cstdint>

namespace pantallas {

class IRenderizadorUi {
 public:
  virtual ~IRenderizadorUi() = default;

  // Pinta la barra superior de 20 px con botón "☰" a la izquierda, título centrado
  // y (opcional) dots del carrusel abajo-derecha si nDots > 0.
  virtual void pintarBarraSuperior(const char* titulo, uint8_t dotActual, uint8_t nDots) = 0;

  // Limpia el área de contenido (320×220) a negro.
  virtual void limpiarAreaContenido() = 0;
};

}  // namespace pantallas
```

- [ ] **Step 4: Compilar (solo cabeceras — nada que romper)**

Run: `~/.platformio/penv/bin/pio run -e esp32dev -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos"`
Expected: `[SUCCESS]`.

- [ ] **Step 5: Commit**

```bash
git add lib/pantallas/library.json lib/pantallas/src/pantalla.h lib/pantallas/src/renderizador_ui.h
git commit -m "feat(pantallas): interfaz Pantalla + IRenderizadorUi"
```

---

## Task 8: `pantallas::GestorPantallas` — lógica pura + tests native

**Files:**
- Create: `lib/pantallas/src/gestor_pantallas.h`
- Create: `lib/pantallas/src/gestor_pantallas.cpp`
- Create: `test/test_gestor_pantallas/test_main.cpp`

- [ ] **Step 1: Escribir el test primero**

`test/test_gestor_pantallas/test_main.cpp`:

```cpp
#include <unity.h>
#include "gestor_pantallas.h"
#include "pantalla.h"
#include "renderizador_ui.h"

using pantallas::EventoUi;
using pantallas::GestorPantallas;
using pantallas::IRenderizadorUi;
using pantallas::ModoGestor;
using pantallas::Pantalla;
using pantallas::TipoEventoUi;

class PantallaFake : public Pantalla {
 public:
  const char* nombreFake;
  uint8_t idFake;
  int nEntradas = 0, nSalidas = 0, nDibujos = 0;
  int ultTapX = -1, ultTapY = -1, ultSwipe = 0;
  PantallaFake(const char* n, uint8_t i) : nombreFake(n), idFake(i) {}
  const char* nombre() const override { return nombreFake; }
  uint8_t id() const override { return idFake; }
  void alEntrar() override { ++nEntradas; }
  void alSalir() override { ++nSalidas; }
  void alTocar(int x, int y) override { ultTapX = x; ultTapY = y; }
  void alDeslizar(int d) override { ultSwipe = d; }
  void dibujar(uint32_t) override { ++nDibujos; }
};

class RendererFake : public IRenderizadorUi {
 public:
  int nBarras = 0, nLimpiezas = 0;
  uint8_t ultDot = 0, ultN = 0;
  void pintarBarraSuperior(const char*, uint8_t d, uint8_t n) override {
    ++nBarras; ultDot = d; ultN = n;
  }
  void limpiarAreaContenido() override { ++nLimpiezas; }
};

void test_modo_carrusel_rota_al_pasar_el_intervalo(void) {
  RendererFake r;
  PantallaFake a("A", 0), b("B", 1), c("C", 2);
  GestorPantallas g(r);
  g.registrar(&a); g.registrar(&b); g.registrar(&c);
  g.configurarModo(ModoGestor::CARRUSEL, /*intervaloS=*/5,
                   /*vistasOrden=*/{0, 1, 2}, /*idFija=*/0);
  g.iniciar(/*msAhora=*/0);

  TEST_ASSERT_EQUAL(1, a.nEntradas);
  g.tick(1000); TEST_ASSERT_EQUAL(0, b.nEntradas);
  g.tick(5001); TEST_ASSERT_EQUAL(1, a.nSalidas);
  TEST_ASSERT_EQUAL(1, b.nEntradas);
  g.tick(10002); TEST_ASSERT_EQUAL(1, c.nEntradas);
  g.tick(15003); TEST_ASSERT_EQUAL(2, a.nEntradas);  // vuelta al principio
}

void test_swipe_manual_adelanta_y_resetea_timer(void) {
  RendererFake r;
  PantallaFake a("A", 0), b("B", 1);
  GestorPantallas g(r);
  g.registrar(&a); g.registrar(&b);
  g.configurarModo(ModoGestor::CARRUSEL, 5, {0, 1}, 0);
  g.iniciar(0);

  g.tick(2000);
  g.encolarEvento({TipoEventoUi::SWIPE_IZQUIERDA, 0, 0});
  g.tick(2000);  // procesa evento
  TEST_ASSERT_EQUAL(1, b.nEntradas);

  // Timer reseteado: 3 s más NO debería rotar (necesitamos otros 5).
  g.tick(5000);
  TEST_ASSERT_EQUAL(0, a.nEntradas - 1);  // a.nEntradas sigue en 1
  g.tick(7500);  // 5.5 s desde el swipe -> rota
  TEST_ASSERT_EQUAL(2, a.nEntradas);
}

void test_modo_fijo_ignora_timer_y_swipe(void) {
  RendererFake r;
  PantallaFake a("A", 0), b("B", 1);
  GestorPantallas g(r);
  g.registrar(&a); g.registrar(&b);
  g.configurarModo(ModoGestor::FIJO, 5, {0, 1}, /*idFija=*/1);
  g.iniciar(0);
  TEST_ASSERT_EQUAL(1, b.nEntradas);
  g.tick(60000);
  TEST_ASSERT_EQUAL(1, b.nEntradas);  // ninguna nueva entrada
  g.encolarEvento({TipoEventoUi::SWIPE_IZQUIERDA, 0, 0});
  g.tick(60000);
  TEST_ASSERT_EQUAL(0, a.nEntradas);
}

void test_tap_en_zona_menu_vuelve_a_home_y_vacia_pila(void) {
  RendererFake r;
  PantallaFake home("Home", 10);
  PantallaFake radar("Radar", 0);
  PantallaFake sub("Sub", 99);
  GestorPantallas g(r);
  g.setHome(&home);
  g.registrar(&home); g.registrar(&radar); g.registrar(&sub);
  g.configurarModo(ModoGestor::CARRUSEL, 5, {0}, 0);
  g.iniciar(0);
  g.mostrarPorId(0);       // entra en radar
  g.abrirEnPila(&sub);     // push sub
  TEST_ASSERT_EQUAL(1, sub.nEntradas);

  // Tap en (10, 10) — zona del botón menú (0..40 × 0..20).
  g.encolarEvento({TipoEventoUi::TAP, 10, 10});
  g.tick(1000);
  TEST_ASSERT_EQUAL(1, sub.nSalidas);
  // Con modo CARRUSEL, iniciar() arranca en orden[0]=radar (no en home),
  // así que home sólo entra 1 vez: la del tap en el botón menú.
  TEST_ASSERT_EQUAL(1, home.nEntradas);
}

void test_pila_ui_pausa_el_carrusel(void) {
  RendererFake r;
  PantallaFake home("Home", 10);
  PantallaFake a("A", 0), b("B", 1);
  PantallaFake sub("Sub", 99);
  GestorPantallas g(r);
  g.setHome(&home);
  g.registrar(&home); g.registrar(&a); g.registrar(&b);
  g.configurarModo(ModoGestor::CARRUSEL, 5, {0, 1}, 0);
  g.iniciar(0);
  g.mostrarPorId(0);
  g.abrirEnPila(&sub);
  g.tick(20000);  // mucho tiempo, pero sub está en la pila
  TEST_ASSERT_EQUAL(0, b.nEntradas);
  g.volverAtras();
  TEST_ASSERT_EQUAL(1, sub.nSalidas);
  // Timer resetea al volver → 5 s más para rotar.
  g.tick(20500);
  TEST_ASSERT_EQUAL(0, b.nEntradas);
  g.tick(25500);
  TEST_ASSERT_EQUAL(1, b.nEntradas);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_modo_carrusel_rota_al_pasar_el_intervalo);
  RUN_TEST(test_swipe_manual_adelanta_y_resetea_timer);
  RUN_TEST(test_modo_fijo_ignora_timer_y_swipe);
  RUN_TEST(test_tap_en_zona_menu_vuelve_a_home_y_vacia_pila);
  RUN_TEST(test_pila_ui_pausa_el_carrusel);
  return UNITY_END();
}
```

- [ ] **Step 2: Run test — falla porque `GestorPantallas` no existe**

Run: `~/.platformio/penv/bin/pio test -e native -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos" -f test_gestor_pantallas`
Expected: FAIL de compilación.

- [ ] **Step 3: Crear `lib/pantallas/src/gestor_pantallas.h`**

```cpp
#pragma once
#include "pantalla.h"
#include "renderizador_ui.h"
#include <cstdint>
#include <vector>

namespace pantallas {

enum class ModoGestor : uint8_t { FIJO = 0, CARRUSEL = 1 };

enum class TipoEventoUi : uint8_t {
  TAP = 0,
  SWIPE_IZQUIERDA = 1,
  SWIPE_DERECHA   = 2,
};

struct EventoUi {
  TipoEventoUi tipo;
  int16_t x;
  int16_t y;
};

// Zona tocable del botón "☰" (en coordenadas de pantalla completa).
constexpr int ZONA_MENU_X_MAX = 40;
constexpr int ZONA_MENU_Y_MAX = 20;

class GestorPantallas {
 public:
  explicit GestorPantallas(IRenderizadorUi& renderer);

  void registrar(Pantalla* p);
  void setHome(Pantalla* home);
  void configurarModo(ModoGestor modo, uint16_t intervaloS,
                      const std::vector<uint8_t>& vistasOrden, uint8_t idFija);
  void iniciar(uint32_t msAhora);

  // Motor: llamar cada frame desde la task de display.
  void tick(uint32_t msAhora);

  // Entrada desde touch (ya destinada al gestor; la task Arduino la reencola aquí).
  void encolarEvento(const EventoUi& ev);

  // Navegación programática (usada desde el menú y sub-pantallas).
  void mostrarPorId(uint8_t id);
  void abrirEnPila(Pantalla* p);
  void volverAtras();

 private:
  void aplicarPantalla(Pantalla* p, uint32_t msAhora);
  Pantalla* pantallaPorId(uint8_t id);

  IRenderizadorUi& renderer_;
  std::vector<Pantalla*> registradas_;
  Pantalla* home_ = nullptr;
  Pantalla* actual_ = nullptr;
  std::vector<Pantalla*> pilaUi_;
  ModoGestor modo_ = ModoGestor::CARRUSEL;
  uint16_t intervaloS_ = 10;
  std::vector<uint8_t> orden_;
  uint8_t idFija_ = 0;
  size_t   indiceCarrusel_ = 0;
  uint32_t ultimoCambioMs_ = 0;
  uint32_t ultimoTickMs_ = 0;
  std::vector<EventoUi> cola_;
};

}  // namespace pantallas
```

- [ ] **Step 4: Crear `lib/pantallas/src/gestor_pantallas.cpp`**

```cpp
#include "gestor_pantallas.h"

namespace pantallas {

GestorPantallas::GestorPantallas(IRenderizadorUi& r) : renderer_(r) {}

void GestorPantallas::registrar(Pantalla* p) { registradas_.push_back(p); }
void GestorPantallas::setHome(Pantalla* h) { home_ = h; }

void GestorPantallas::configurarModo(ModoGestor m, uint16_t iv,
                                     const std::vector<uint8_t>& orden, uint8_t idF) {
  modo_ = m;
  intervaloS_ = iv;
  orden_ = orden;
  idFija_ = idF;
}

Pantalla* GestorPantallas::pantallaPorId(uint8_t id) {
  for (auto* p : registradas_) if (p->id() == id) return p;
  return nullptr;
}

void GestorPantallas::aplicarPantalla(Pantalla* p, uint32_t msAhora) {
  if (actual_ == p) return;
  if (actual_) actual_->alSalir();
  actual_ = p;
  ultimoCambioMs_ = msAhora;
  renderer_.limpiarAreaContenido();
  if (actual_) actual_->alEntrar();
}

void GestorPantallas::iniciar(uint32_t msAhora) {
  ultimoTickMs_ = msAhora;
  Pantalla* inicial = nullptr;
  if (modo_ == ModoGestor::FIJO) {
    inicial = pantallaPorId(idFija_);
  } else if (!orden_.empty()) {
    indiceCarrusel_ = 0;
    inicial = pantallaPorId(orden_[0]);
  }
  if (!inicial && home_) inicial = home_;
  aplicarPantalla(inicial, msAhora);
}

void GestorPantallas::encolarEvento(const EventoUi& ev) { cola_.push_back(ev); }

void GestorPantallas::mostrarPorId(uint8_t id) {
  Pantalla* p = pantallaPorId(id);
  if (!p) return;
  pilaUi_.clear();
  aplicarPantalla(p, ultimoTickMs_);
  // Ubicar el índice en el carrusel si corresponde.
  for (size_t i = 0; i < orden_.size(); ++i) {
    if (orden_[i] == id) { indiceCarrusel_ = i; break; }
  }
}

void GestorPantallas::abrirEnPila(Pantalla* p) {
  if (!p) return;
  if (actual_) pilaUi_.push_back(actual_);
  aplicarPantalla(p, ultimoTickMs_);
}

void GestorPantallas::volverAtras() {
  if (pilaUi_.empty()) {
    if (home_) aplicarPantalla(home_, ultimoTickMs_);
    return;
  }
  Pantalla* prev = pilaUi_.back();
  pilaUi_.pop_back();
  aplicarPantalla(prev, ultimoTickMs_);
}

void GestorPantallas::tick(uint32_t msAhora) {
  ultimoTickMs_ = msAhora;
  // 1. Procesar eventos táctiles.
  for (const auto& ev : cola_) {
    switch (ev.tipo) {
      case TipoEventoUi::TAP:
        if (ev.x < ZONA_MENU_X_MAX && ev.y < ZONA_MENU_Y_MAX && home_) {
          // Volver al home vaciando la pila.
          pilaUi_.clear();
          aplicarPantalla(home_, msAhora);
        } else if (actual_) {
          // Coordenadas relativas al área de contenido (restar barra).
          actual_->alTocar(ev.x, ev.y - 20);
        }
        break;
      case TipoEventoUi::SWIPE_IZQUIERDA:
      case TipoEventoUi::SWIPE_DERECHA:
        if (!pilaUi_.empty()) break;                  // pila abierta: ignorar
        if (modo_ != ModoGestor::CARRUSEL) break;     // fijo: ignorar
        if (orden_.empty()) break;
        if (ev.tipo == TipoEventoUi::SWIPE_IZQUIERDA)
          indiceCarrusel_ = (indiceCarrusel_ + 1) % orden_.size();
        else
          indiceCarrusel_ = (indiceCarrusel_ + orden_.size() - 1) % orden_.size();
        aplicarPantalla(pantallaPorId(orden_[indiceCarrusel_]), msAhora);
        break;
    }
  }
  cola_.clear();

  // 2. Rotación automática en carrusel (sólo si no hay pila UI y no estamos en home).
  if (modo_ == ModoGestor::CARRUSEL && pilaUi_.empty() && actual_ != home_
      && !orden_.empty() && intervaloS_ > 0) {
    const uint32_t delta = msAhora - ultimoCambioMs_;
    if (delta >= static_cast<uint32_t>(intervaloS_) * 1000u) {
      indiceCarrusel_ = (indiceCarrusel_ + 1) % orden_.size();
      aplicarPantalla(pantallaPorId(orden_[indiceCarrusel_]), msAhora);
    }
  }

  // 3. Pintar.
  const uint8_t nDots = (modo_ == ModoGestor::CARRUSEL && pilaUi_.empty() && actual_ != home_)
                        ? static_cast<uint8_t>(orden_.size()) : 0;
  const uint8_t dotAct = static_cast<uint8_t>(indiceCarrusel_);
  renderer_.pintarBarraSuperior(actual_ ? actual_->nombre() : "", dotAct, nDots);
  if (actual_) actual_->dibujar(msAhora);
}

}  // namespace pantallas
```

- [ ] **Step 5: Run test native**

Run: `~/.platformio/penv/bin/pio test -e native -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos" -f test_gestor_pantallas`
Expected: 5 tests OK.

- [ ] **Step 6: Commit**

```bash
git add lib/pantallas/src/gestor_pantallas.h lib/pantallas/src/gestor_pantallas.cpp test/test_gestor_pantallas
git commit -m "feat(pantallas): GestorPantallas con carrusel/fijo, pila UI y tests native"
```

---

## Task 9: `pantalla_radar` — migrar el radar detrás de la interfaz `Pantalla`

**Files:**
- Create: `lib/pantalla_radar/library.json`
- Create: `lib/pantalla_radar/src/pantalla_radar.h`
- Create: `lib/pantalla_radar/src/pantalla_radar.cpp`

Se crea la nueva lib en paralelo con `display_radar` viejo; el borrado va en la Task 20.

- [ ] **Step 1: Crear `lib/pantalla_radar/library.json`**

```json
{
  "name": "pantalla_radar",
  "version": "0.1.0",
  "description": "Vista Radar (sonar) para el carrusel",
  "dependencies": {
    "bodmer/TFT_eSPI": "^2.5.43"
  }
}
```

- [ ] **Step 2: Crear `lib/pantalla_radar/src/pantalla_radar.h`**

```cpp
#pragma once
#include "pantalla.h"
#include "radar_state.h"
#include <cstdint>

class TFT_eSprite;

class PantallaRadar : public pantallas::Pantalla {
 public:
  explicit PantallaRadar(RadarState& estado);
  ~PantallaRadar() override;

  const char* nombre() const override { return "Radar"; }
  uint8_t id() const override { return 0; }

  void alEntrar() override;
  void alSalir() override;
  void dibujar(uint32_t msAhora) override;

 private:
  RadarState& estado_;
  TFT_eSprite* sprite_ = nullptr;
  int angBarrido_ = 0;
};
```

- [ ] **Step 3: Crear `lib/pantalla_radar/src/pantalla_radar.cpp`**

```cpp
#include "pantalla_radar.h"
#include "tft_driver.h"
#include <TFT_eSPI.h>
#include <cmath>
#include <cstdio>
#include <algorithm>

namespace {
constexpr int LADO_RADAR   = 220;   // sprite cuadrado dentro de 320×220
constexpr int OFFSET_Y     = 20;    // barra superior de 20 px
constexpr int PANEL_X      = LADO_RADAR;
constexpr int PANEL_ANCHO  = 320 - LADO_RADAR;

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
  const int rMax = LADO_RADAR / 2 - 8;
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
  tft.fillRect(PANEL_X, OFFSET_Y, PANEL_ANCHO, 220, COL_FONDO);
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
    tft.setCursor(PANEL_X + 4, OFFSET_Y + 40);
    tft.print("sin");
    tft.setCursor(PANEL_X + 4, OFFSET_Y + 58);
    tft.print("aviones");
    return;
  }
  const bool encima = (mc->dist_km < 3.0);
  const uint16_t col = encima ? COL_ENCIMA : COL_PANEL_TXT;
  tft.setTextColor(col, COL_FONDO);
  tft.setTextFont(2);
  tft.setCursor(PANEL_X + 4, OFFSET_Y + 38);
  tft.print((mc->callsign.empty() ? mc->hex : mc->callsign).c_str());
  tft.setTextFont(4);
  tft.setCursor(PANEL_X + 4, OFFSET_Y + 62);
  if (mc->dist_km < 10.0) std::snprintf(buf, sizeof(buf), "%.1f", mc->dist_km);
  else                    std::snprintf(buf, sizeof(buf), "%d", (int)mc->dist_km);
  tft.print(buf);
  tft.setTextFont(2);
  tft.setCursor(PANEL_X + 4, OFFSET_Y + 96);
  tft.print("km");
  tft.setTextColor(COL_PANEL_TXT, COL_FONDO);
  tft.setTextFont(2);
  tft.setCursor(PANEL_X + 4, OFFSET_Y + 122);
  std::snprintf(buf, sizeof(buf), "%dft", mc->alt_ft);
  tft.print(buf);
  tft.setCursor(PANEL_X + 4, OFFSET_Y + 144);
  std::snprintf(buf, sizeof(buf), "%d\xB0", mc->bearing);
  tft.print(buf);
  if (encima) {
    tft.setTextColor(COL_ENCIMA, COL_FONDO);
    tft.setCursor(PANEL_X + 4, OFFSET_Y + 200);
    tft.print("ENCIMA");
  }
}

}  // namespace

PantallaRadar::PantallaRadar(RadarState& estado) : estado_(estado) {}
PantallaRadar::~PantallaRadar() { alSalir(); }

void PantallaRadar::alEntrar() {
  if (sprite_) return;
  sprite_ = new TFT_eSprite(&tft_driver::getTft());
  sprite_->setColorDepth(8);
  void* p = sprite_->createSprite(LADO_RADAR, LADO_RADAR);
  Serial.printf("[radar] sprite %dx%d -> %s (heap %u)\n",
                LADO_RADAR, LADO_RADAR, p ? "OK" : "FAIL",
                (unsigned)ESP.getFreeHeap());
  if (p) sprite_->fillSprite(COL_FONDO);
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
  pintarPanelSonar(tft_driver::getTft(), snap);
  angBarrido_ = (angBarrido_ + 10) % 360;
}
```

- [ ] **Step 4: Compilar firmware**

Run: `~/.platformio/penv/bin/pio run -e esp32dev -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos"`
Expected: `[SUCCESS]`.

- [ ] **Step 5: Commit**

```bash
git add lib/pantalla_radar
git commit -m "feat(pantalla_radar): migrar radar sonar detrás de la interfaz Pantalla"
```

---

## Task 10: `pantalla_menu` — home

**Files:**
- Create: `lib/pantalla_menu/library.json`
- Create: `lib/pantalla_menu/src/pantalla_menu.h`
- Create: `lib/pantalla_menu/src/pantalla_menu.cpp`

- [ ] **Step 1: Crear `lib/pantalla_menu/library.json`**

```json
{
  "name": "pantalla_menu",
  "version": "0.1.0",
  "description": "Home del carrusel con listado de vistas y entrada a Ajustes"
}
```

- [ ] **Step 2: Crear `lib/pantalla_menu/src/pantalla_menu.h`**

```cpp
#pragma once
#include "pantalla.h"
#include "gestor_pantallas.h"
#include <string>
#include <vector>

class PantallaMenu : public pantallas::Pantalla {
 public:
  struct Entrada { uint8_t id; std::string etiqueta; };

  PantallaMenu(pantallas::GestorPantallas& gestor, pantallas::Pantalla* ajustes);
  void configurarEntradas(const std::vector<Entrada>& entradas);

  const char* nombre() const override { return "Menú"; }
  uint8_t id() const override { return 10; }

  void alEntrar() override { dirty_ = true; }
  void alTocar(int x, int y) override;
  void dibujar(uint32_t msAhora) override;

 private:
  pantallas::GestorPantallas& gestor_;
  pantallas::Pantalla* ajustes_;
  std::vector<Entrada> entradas_;
  bool dirty_ = true;
};
```

- [ ] **Step 3: Crear `lib/pantalla_menu/src/pantalla_menu.cpp`**

```cpp
#include "pantalla_menu.h"
#include "tft_driver.h"
#include <TFT_eSPI.h>

namespace {
constexpr int OFFSET_Y = 20;
constexpr int ALTO_FILA = 28;
constexpr uint16_t COL_FONDO = 0x0000;
constexpr uint16_t COL_TXT   = 0x07E0;
constexpr uint16_t COL_LINEA = 0x03E0;
}

PantallaMenu::PantallaMenu(pantallas::GestorPantallas& g, pantallas::Pantalla* a)
  : gestor_(g), ajustes_(a) {}

void PantallaMenu::configurarEntradas(const std::vector<Entrada>& e) {
  entradas_ = e;
  dirty_ = true;
}

void PantallaMenu::alTocar(int x, int y) {
  const int fila = y / ALTO_FILA;
  const int nTotal = static_cast<int>(entradas_.size()) + 1;  // + Ajustes
  if (fila < 0 || fila >= nTotal) return;
  if (fila < static_cast<int>(entradas_.size())) {
    gestor_.mostrarPorId(entradas_[fila].id);
  } else if (ajustes_) {
    gestor_.abrirEnPila(ajustes_);
  }
}

void PantallaMenu::dibujar(uint32_t) {
  if (!dirty_) return;
  auto& tft = tft_driver::getTft();
  tft.fillRect(0, OFFSET_Y, 320, 220, COL_FONDO);
  tft.setTextFont(2);
  tft.setTextColor(COL_TXT, COL_FONDO);
  int y = OFFSET_Y + 4;
  for (const auto& e : entradas_) {
    tft.setCursor(12, y);
    tft.print(e.etiqueta.c_str());
    tft.drawFastHLine(0, y + ALTO_FILA - 4, 320, COL_LINEA);
    y += ALTO_FILA;
  }
  tft.setCursor(12, y);
  tft.print("Ajustes");
  dirty_ = false;
}
```

- [ ] **Step 4: Compilar**

Run: `~/.platformio/penv/bin/pio run -e esp32dev -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos"`
Expected: `[SUCCESS]`.

- [ ] **Step 5: Commit**

```bash
git add lib/pantalla_menu
git commit -m "feat(pantalla_menu): home con listado de vistas y entrada a Ajustes"
```

---

## Task 11: `pantalla_proximamente` — placeholder genérico

**Files:**
- Create: `lib/pantalla_proximamente/library.json`
- Create: `lib/pantalla_proximamente/src/pantalla_proximamente.h`
- Create: `lib/pantalla_proximamente/src/pantalla_proximamente.cpp`

- [ ] **Step 1: Crear `library.json`**

```json
{
  "name": "pantalla_proximamente",
  "version": "0.1.0",
  "description": "Placeholder para vistas aún no implementadas"
}
```

- [ ] **Step 2: Crear `pantalla_proximamente.h`**

```cpp
#pragma once
#include "pantalla.h"
#include <string>

class PantallaProximamente : public pantallas::Pantalla {
 public:
  PantallaProximamente(uint8_t id, std::string nombre) : id_(id), nombre_(std::move(nombre)) {}
  const char* nombre() const override { return nombre_.c_str(); }
  uint8_t id() const override { return id_; }
  void alEntrar() override { dirty_ = true; }
  void dibujar(uint32_t) override;

 private:
  uint8_t id_;
  std::string nombre_;
  bool dirty_ = true;
};
```

- [ ] **Step 3: Crear `pantalla_proximamente.cpp`**

```cpp
#include "pantalla_proximamente.h"
#include "tft_driver.h"
#include <TFT_eSPI.h>

void PantallaProximamente::dibujar(uint32_t) {
  if (!dirty_) return;
  auto& tft = tft_driver::getTft();
  tft.fillRect(0, 20, 320, 220, 0x0000);
  tft.setTextColor(0x07E0, 0x0000);
  tft.setTextFont(4);
  tft.setCursor(20, 90);
  tft.print(nombre_.c_str());
  tft.setTextFont(2);
  tft.setCursor(20, 140);
  tft.print("Proximamente");
  dirty_ = false;
}
```

- [ ] **Step 4: Compilar**

Run: `~/.platformio/penv/bin/pio run -e esp32dev -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos"`
Expected: `[SUCCESS]`.

- [ ] **Step 5: Commit**

```bash
git add lib/pantalla_proximamente
git commit -m "feat(pantalla_proximamente): placeholder para vistas futuras"
```

---

## Task 12: `pantalla_ajustes` — menú principal de ajustes

**Files:**
- Create: `lib/pantalla_ajustes/library.json`
- Create: `lib/pantalla_ajustes/src/pantalla_ajustes.h`
- Create: `lib/pantalla_ajustes/src/pantalla_ajustes.cpp`

- [ ] **Step 1: Crear `library.json`**

```json
{
  "name": "pantalla_ajustes",
  "version": "0.1.0",
  "description": "Sub-pantallas de configuración accesibles desde el menú principal"
}
```

- [ ] **Step 2: Crear `pantalla_ajustes.h`**

```cpp
#pragma once
#include "pantalla.h"
#include "gestor_pantallas.h"
#include "config_store.h"

class PantallaAjustes : public pantallas::Pantalla {
 public:
  struct SubPantallas {
    pantallas::Pantalla* intervalo;
    pantallas::Pantalla* seleccionVistas;
    pantallas::Pantalla* seleccionVistaFija;
    pantallas::Pantalla* configLocalizacion;
    pantallas::Pantalla* calibrarTouch;
    pantallas::Pantalla* confirmarReset;
  };

  PantallaAjustes(pantallas::GestorPantallas& g, Config& cfg, SubPantallas subs);

  const char* nombre() const override { return "Ajustes"; }
  uint8_t id() const override { return 20; }

  void alEntrar() override { dirty_ = true; }
  void alTocar(int x, int y) override;
  void dibujar(uint32_t) override;

 private:
  pantallas::GestorPantallas& gestor_;
  Config& cfg_;
  SubPantallas subs_;
  bool dirty_ = true;
};
```

- [ ] **Step 3: Crear `pantalla_ajustes.cpp`**

```cpp
#include "pantalla_ajustes.h"
#include "tft_driver.h"
#include <TFT_eSPI.h>
#include <cstdio>

namespace {
constexpr int OFFSET_Y  = 20;
constexpr int ALTO_FILA = 26;
constexpr uint16_t COL_FONDO = 0x0000;
constexpr uint16_t COL_TXT   = 0x07E0;
constexpr uint16_t COL_LINEA = 0x03E0;

const char* labelVista(uint8_t id) {
  switch (id) {
    case 0: return "Radar";
    case 1: return "Reloj";
    case 2: return "Meteo";
    case 3: return "Fútbol";
    case 4: return "MotoGP";
    case 5: return "F1";
    default: return "?";
  }
}
}  // namespace

PantallaAjustes::PantallaAjustes(pantallas::GestorPantallas& g, Config& c, SubPantallas s)
  : gestor_(g), cfg_(c), subs_(s) {}

void PantallaAjustes::alTocar(int x, int y) {
  const int fila = y / ALTO_FILA;
  // Índice dinámico según modo:
  // 0: Modo (siempre)
  // 1: (CARRUSEL) Intervalo | (FIJO) Vista fija
  // 2: (CARRUSEL) Vistas activas
  // 3: Reconfigurar localización
  // 4: Calibrar táctil
  // 5: Reset total
  // 6: Volver
  const bool carrusel = (cfg_.modo == ModoVista::CARRUSEL);
  const int idxVistasActivas = carrusel ? 2 : -1;
  const int idxVistaFija     = carrusel ? -1 : 1;
  const int idxIntervalo     = carrusel ? 1 : -1;
  const int idxLoc           = 3;
  const int idxCal           = 4;
  const int idxReset         = 5;
  const int idxVolver        = 6;

  if (fila == 0) {
    cfg_.modo = (cfg_.modo == ModoVista::CARRUSEL) ? ModoVista::FIJO : ModoVista::CARRUSEL;
    ConfigStore::guardar(cfg_);
    dirty_ = true;
  } else if (fila == idxIntervalo && subs_.intervalo) {
    gestor_.abrirEnPila(subs_.intervalo);
  } else if (fila == idxVistasActivas && subs_.seleccionVistas) {
    gestor_.abrirEnPila(subs_.seleccionVistas);
  } else if (fila == idxVistaFija && subs_.seleccionVistaFija) {
    gestor_.abrirEnPila(subs_.seleccionVistaFija);
  } else if (fila == idxLoc && subs_.configLocalizacion) {
    gestor_.abrirEnPila(subs_.configLocalizacion);
  } else if (fila == idxCal && subs_.calibrarTouch) {
    gestor_.abrirEnPila(subs_.calibrarTouch);
  } else if (fila == idxReset && subs_.confirmarReset) {
    gestor_.abrirEnPila(subs_.confirmarReset);
  } else if (fila == idxVolver) {
    gestor_.volverAtras();
  }
}

void PantallaAjustes::dibujar(uint32_t) {
  if (!dirty_) return;
  auto& tft = tft_driver::getTft();
  tft.fillRect(0, OFFSET_Y, 320, 220, COL_FONDO);
  tft.setTextFont(2);
  tft.setTextColor(COL_TXT, COL_FONDO);
  char buf[48];
  int y = OFFSET_Y + 4;
  auto fila = [&](const char* txt) {
    tft.setCursor(10, y);
    tft.print(txt);
    tft.drawFastHLine(0, y + ALTO_FILA - 4, 320, COL_LINEA);
    y += ALTO_FILA;
  };
  std::snprintf(buf, sizeof(buf), "Modo: %s",
                cfg_.modo == ModoVista::CARRUSEL ? "Carrusel" : "Fijo");
  fila(buf);
  if (cfg_.modo == ModoVista::CARRUSEL) {
    std::snprintf(buf, sizeof(buf), "Intervalo: %u s", (unsigned)cfg_.intervalo_carrusel_s);
    fila(buf);
    fila("Vistas activas y orden");
  } else {
    std::snprintf(buf, sizeof(buf), "Vista fija: %s", labelVista(cfg_.vista_fija));
    fila(buf);
    fila(" ");  // ocupa el hueco de la fila 2 en modo fijo
  }
  fila("Reconfigurar localizacion");
  fila("Calibrar tactil");
  fila("Reset total");
  fila("< Volver");
  dirty_ = false;
}
```

- [ ] **Step 4: Compilar**

Run: `~/.platformio/penv/bin/pio run -e esp32dev -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos"`
Expected: `[SUCCESS]`.

- [ ] **Step 5: Commit**

```bash
git add lib/pantalla_ajustes/library.json lib/pantalla_ajustes/src/pantalla_ajustes.h lib/pantalla_ajustes/src/pantalla_ajustes.cpp
git commit -m "feat(pantalla_ajustes): menú de ajustes principal con modo/intervalo/vistas"
```

---

## Task 13: Sub-pantalla `PantallaIntervalo` (picker −/+)

**Files:**
- Create: `lib/pantalla_ajustes/src/pantalla_intervalo.h`
- Create: `lib/pantalla_ajustes/src/pantalla_intervalo.cpp`

- [ ] **Step 1: Crear `pantalla_intervalo.h`**

```cpp
#pragma once
#include "pantalla.h"
#include "gestor_pantallas.h"
#include "config_store.h"

class PantallaIntervalo : public pantallas::Pantalla {
 public:
  PantallaIntervalo(pantallas::GestorPantallas& g, Config& cfg)
    : gestor_(g), cfg_(cfg) {}
  const char* nombre() const override { return "Intervalo"; }
  uint8_t id() const override { return 30; }
  void alEntrar() override { dirty_ = true; }
  void alTocar(int x, int y) override;
  void dibujar(uint32_t) override;

 private:
  pantallas::GestorPantallas& gestor_;
  Config& cfg_;
  bool dirty_ = true;
};
```

- [ ] **Step 2: Crear `pantalla_intervalo.cpp`**

```cpp
#include "pantalla_intervalo.h"
#include "tft_driver.h"
#include <TFT_eSPI.h>
#include <cstdio>

void PantallaIntervalo::alTocar(int x, int y) {
  // Botón "-": [30, 90] × [80, 160].  Botón "+": [200, 260] × [80, 160].
  // Botón "< Volver": [10, 90] × [180, 210].
  if (y >= 80 && y <= 160) {
    if (x >= 30 && x <= 90 && cfg_.intervalo_carrusel_s > 5) {
      cfg_.intervalo_carrusel_s -= 5;
      ConfigStore::guardar(cfg_);
      dirty_ = true;
    } else if (x >= 200 && x <= 260 && cfg_.intervalo_carrusel_s < 120) {
      cfg_.intervalo_carrusel_s += 5;
      ConfigStore::guardar(cfg_);
      dirty_ = true;
    }
  } else if (y >= 180 && y <= 210 && x >= 10 && x <= 90) {
    gestor_.volverAtras();
  }
}

void PantallaIntervalo::dibujar(uint32_t) {
  if (!dirty_) return;
  auto& tft = tft_driver::getTft();
  tft.fillRect(0, 20, 320, 220, 0x0000);
  tft.setTextColor(0x07E0, 0x0000);
  tft.setTextFont(2);
  tft.setCursor(10, 30);
  tft.print("Intervalo carrusel");

  // Botón "-"
  tft.fillRect(30, 80, 60, 80, 0x03E0);
  tft.setTextColor(0xFFFF, 0x03E0);
  tft.setTextFont(4);
  tft.setCursor(50, 100);
  tft.print("-");
  // Botón "+"
  tft.fillRect(200, 80, 60, 80, 0x03E0);
  tft.setCursor(215, 100);
  tft.print("+");
  // Valor centrado
  tft.setTextColor(0x07E0, 0x0000);
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%u s", (unsigned)cfg_.intervalo_carrusel_s);
  tft.setCursor(120, 100);
  tft.print(buf);
  // Volver
  tft.setTextFont(2);
  tft.setCursor(10, 190);
  tft.setTextColor(0x07E0, 0x0000);
  tft.print("< Volver");

  dirty_ = false;
}
```

- [ ] **Step 3: Compilar**

Run: `~/.platformio/penv/bin/pio run -e esp32dev -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos"`
Expected: `[SUCCESS]`.

- [ ] **Step 4: Commit**

```bash
git add lib/pantalla_ajustes/src/pantalla_intervalo.h lib/pantalla_ajustes/src/pantalla_intervalo.cpp
git commit -m "feat(pantalla_ajustes): sub-pantalla PantallaIntervalo con picker -/+"
```

---

## Task 14: Sub-pantalla `PantallaSeleccionVistas` (checklist + reorden)

**Files:**
- Create: `lib/pantalla_ajustes/src/pantalla_seleccion_vistas.h`
- Create: `lib/pantalla_ajustes/src/pantalla_seleccion_vistas.cpp`

- [ ] **Step 1: Crear `pantalla_seleccion_vistas.h`**

```cpp
#pragma once
#include "pantalla.h"
#include "gestor_pantallas.h"
#include "config_store.h"
#include <vector>

class PantallaSeleccionVistas : public pantallas::Pantalla {
 public:
  PantallaSeleccionVistas(pantallas::GestorPantallas& g, Config& cfg)
    : gestor_(g), cfg_(cfg) {}
  const char* nombre() const override { return "Vistas"; }
  uint8_t id() const override { return 31; }
  void alEntrar() override;
  void alTocar(int x, int y) override;
  void dibujar(uint32_t) override;

 private:
  void guardar();
  int nActivas() const;

  pantallas::GestorPantallas& gestor_;
  Config& cfg_;
  // Estado local: para cada id 0..5, {activa, posición en orden si activa}.
  std::vector<uint8_t> ordenLocal_;   // ids en orden actual, sólo activas
  bool                 activas_[6] = {false, false, false, false, false, false};
  bool dirty_ = true;
};
```

- [ ] **Step 2: Crear `pantalla_seleccion_vistas.cpp`**

```cpp
#include "pantalla_seleccion_vistas.h"
#include "tft_driver.h"
#include <TFT_eSPI.h>
#include <algorithm>
#include <cstdio>

namespace {
const char* labelVista(uint8_t id) {
  switch (id) {
    case 0: return "Radar";
    case 1: return "Reloj";
    case 2: return "Meteo";
    case 3: return "Futbol";
    case 4: return "MotoGP";
    case 5: return "F1";
    default: return "?";
  }
}
constexpr int OFFSET_Y  = 20;
constexpr int ALTO_FILA = 26;
}

void PantallaSeleccionVistas::alEntrar() {
  ordenLocal_ = cfg_.vistas_orden;
  for (int i = 0; i < 6; ++i) activas_[i] = false;
  for (auto id : ordenLocal_) if (id < 6) activas_[id] = true;
  dirty_ = true;
}

int PantallaSeleccionVistas::nActivas() const { return static_cast<int>(ordenLocal_.size()); }

void PantallaSeleccionVistas::guardar() {
  cfg_.vistas_orden = ordenLocal_;
  ConfigStore::guardar(cfg_);
}

void PantallaSeleccionVistas::alTocar(int x, int y) {
  // Cada fila del listado muestra las 6 vistas (fila 0..5).
  // Fila 6 = "< Volver".
  // Dentro de una fila: check [10,60], nombre [70,220], flecha ↑ [230,265], flecha ↓ [270,305].
  int fila = y / ALTO_FILA;
  if (fila >= 0 && fila < 6) {
    uint8_t id = static_cast<uint8_t>(fila);
    if (x >= 10 && x <= 60) {
      if (activas_[id]) {
        if (nActivas() > 1) {
          activas_[id] = false;
          ordenLocal_.erase(std::remove(ordenLocal_.begin(), ordenLocal_.end(), id),
                            ordenLocal_.end());
        }
      } else {
        activas_[id] = true;
        ordenLocal_.push_back(id);
      }
      guardar();
      dirty_ = true;
    } else if (x >= 230 && x <= 265 && activas_[id]) {
      auto it = std::find(ordenLocal_.begin(), ordenLocal_.end(), id);
      if (it != ordenLocal_.begin() && it != ordenLocal_.end()) {
        std::iter_swap(it, it - 1);
        guardar();
        dirty_ = true;
      }
    } else if (x >= 270 && x <= 305 && activas_[id]) {
      auto it = std::find(ordenLocal_.begin(), ordenLocal_.end(), id);
      if (it != ordenLocal_.end() && (it + 1) != ordenLocal_.end()) {
        std::iter_swap(it, it + 1);
        guardar();
        dirty_ = true;
      }
    }
  } else if (fila == 6 && x >= 10 && x <= 90) {
    gestor_.volverAtras();
  }
}

void PantallaSeleccionVistas::dibujar(uint32_t) {
  if (!dirty_) return;
  auto& tft = tft_driver::getTft();
  tft.fillRect(0, OFFSET_Y, 320, 220, 0x0000);
  tft.setTextFont(2);
  tft.setTextColor(0x07E0, 0x0000);
  char buf[32];
  int y = OFFSET_Y + 2;
  for (uint8_t id = 0; id < 6; ++id) {
    // Check
    tft.drawRect(12, y + 2, 20, 20, 0x07E0);
    if (activas_[id]) tft.fillRect(15, y + 5, 14, 14, 0x07E0);
    // Nombre
    std::snprintf(buf, sizeof(buf), " %s", labelVista(id));
    tft.setCursor(40, y + 4);
    tft.print(buf);
    // Flechas
    if (activas_[id]) {
      tft.setCursor(238, y + 4); tft.print("^");
      tft.setCursor(278, y + 4); tft.print("v");
    }
    y += ALTO_FILA;
  }
  tft.setCursor(10, y + 4);
  tft.print("< Volver");
  dirty_ = false;
}
```

- [ ] **Step 3: Compilar**

Run: `~/.platformio/penv/bin/pio run -e esp32dev -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos"`
Expected: `[SUCCESS]`.

- [ ] **Step 4: Commit**

```bash
git add lib/pantalla_ajustes/src/pantalla_seleccion_vistas.h lib/pantalla_ajustes/src/pantalla_seleccion_vistas.cpp
git commit -m "feat(pantalla_ajustes): sub-pantalla seleccion de vistas activas y orden"
```

---

## Task 15: Sub-pantalla `PantallaSeleccionVistaFija`

**Files:**
- Create: `lib/pantalla_ajustes/src/pantalla_seleccion_vista_fija.h`
- Create: `lib/pantalla_ajustes/src/pantalla_seleccion_vista_fija.cpp`

- [ ] **Step 1: Crear el `.h`**

```cpp
#pragma once
#include "pantalla.h"
#include "gestor_pantallas.h"
#include "config_store.h"

class PantallaSeleccionVistaFija : public pantallas::Pantalla {
 public:
  PantallaSeleccionVistaFija(pantallas::GestorPantallas& g, Config& cfg)
    : gestor_(g), cfg_(cfg) {}
  const char* nombre() const override { return "Vista fija"; }
  uint8_t id() const override { return 32; }
  void alEntrar() override { dirty_ = true; }
  void alTocar(int x, int y) override;
  void dibujar(uint32_t) override;

 private:
  pantallas::GestorPantallas& gestor_;
  Config& cfg_;
  bool dirty_ = true;
};
```

- [ ] **Step 2: Crear el `.cpp`**

```cpp
#include "pantalla_seleccion_vista_fija.h"
#include "tft_driver.h"
#include <TFT_eSPI.h>
#include <cstdio>

namespace {
const char* labelVista(uint8_t id) {
  switch (id) {
    case 0: return "Radar"; case 1: return "Reloj"; case 2: return "Meteo";
    case 3: return "Futbol"; case 4: return "MotoGP"; case 5: return "F1";
    default: return "?";
  }
}
constexpr int OFFSET_Y = 20;
constexpr int ALTO_FILA = 26;
}

void PantallaSeleccionVistaFija::alTocar(int x, int y) {
  int fila = y / ALTO_FILA;
  if (fila >= 0 && fila < 6) {
    cfg_.vista_fija = static_cast<uint8_t>(fila);
    ConfigStore::guardar(cfg_);
    dirty_ = true;
  } else if (fila == 6 && x >= 10 && x <= 90) {
    gestor_.volverAtras();
  }
}

void PantallaSeleccionVistaFija::dibujar(uint32_t) {
  if (!dirty_) return;
  auto& tft = tft_driver::getTft();
  tft.fillRect(0, OFFSET_Y, 320, 220, 0x0000);
  tft.setTextFont(2);
  tft.setTextColor(0x07E0, 0x0000);
  char buf[32];
  int y = OFFSET_Y + 2;
  for (uint8_t id = 0; id < 6; ++id) {
    tft.drawCircle(22, y + 12, 8, 0x07E0);
    if (cfg_.vista_fija == id) tft.fillCircle(22, y + 12, 5, 0x07E0);
    std::snprintf(buf, sizeof(buf), "  %s", labelVista(id));
    tft.setCursor(40, y + 4);
    tft.print(buf);
    y += ALTO_FILA;
  }
  tft.setCursor(10, y + 4);
  tft.print("< Volver");
  dirty_ = false;
}
```

- [ ] **Step 3: Compilar**

Run: `~/.platformio/penv/bin/pio run -e esp32dev -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos"`
Expected: `[SUCCESS]`.

- [ ] **Step 4: Commit**

```bash
git add lib/pantalla_ajustes/src/pantalla_seleccion_vista_fija.h lib/pantalla_ajustes/src/pantalla_seleccion_vista_fija.cpp
git commit -m "feat(pantalla_ajustes): sub-pantalla seleccion de vista fija"
```

---

## Task 16: Sub-pantalla `PantallaConfirmarReset`

**Files:**
- Create: `lib/pantalla_ajustes/src/pantalla_confirmar_reset.h`
- Create: `lib/pantalla_ajustes/src/pantalla_confirmar_reset.cpp`

- [ ] **Step 1: Crear el `.h`**

```cpp
#pragma once
#include "pantalla.h"
#include "gestor_pantallas.h"

class PantallaConfirmarReset : public pantallas::Pantalla {
 public:
  explicit PantallaConfirmarReset(pantallas::GestorPantallas& g) : gestor_(g) {}
  const char* nombre() const override { return "Reset"; }
  uint8_t id() const override { return 34; }
  void alEntrar() override { dirty_ = true; }
  void alTocar(int x, int y) override;
  void dibujar(uint32_t) override;

 private:
  pantallas::GestorPantallas& gestor_;
  bool dirty_ = true;
};
```

- [ ] **Step 2: Crear el `.cpp`**

```cpp
#include "pantalla_confirmar_reset.h"
#include "tft_driver.h"
#include "config_store.h"
#include <Arduino.h>
#include <TFT_eSPI.h>

void PantallaConfirmarReset::alTocar(int x, int y) {
  // Cancelar: [10, 150] × [160, 210].  Borrar: [170, 310] × [160, 210].
  if (y >= 160 && y <= 210) {
    if (x >= 10 && x <= 150) {
      gestor_.volverAtras();
    } else if (x >= 170 && x <= 310) {
      ConfigStore::borrar();
      delay(300);
      ESP.restart();
    }
  }
}

void PantallaConfirmarReset::dibujar(uint32_t) {
  if (!dirty_) return;
  auto& tft = tft_driver::getTft();
  tft.fillRect(0, 20, 320, 220, 0x0000);
  tft.setTextFont(2);
  tft.setTextColor(0xF800, 0x0000);
  tft.setCursor(10, 30);
  tft.print("Reset total");
  tft.setTextColor(0x07E0, 0x0000);
  tft.setCursor(10, 60);
  tft.print("Se borrara TODA la config:");
  tft.setCursor(10, 80);
  tft.print("WiFi, direccion, ajustes.");
  tft.setCursor(10, 110);
  tft.print("Confirmas?");
  // Botón Cancelar
  tft.fillRect(10, 160, 140, 50, 0x03E0);
  tft.setTextColor(0xFFFF, 0x03E0);
  tft.setCursor(40, 180);
  tft.print("Cancelar");
  // Botón Borrar
  tft.fillRect(170, 160, 140, 50, 0xF800);
  tft.setCursor(210, 180);
  tft.print("Borrar");
  dirty_ = false;
}
```

- [ ] **Step 3: Compilar**

Run: `~/.platformio/penv/bin/pio run -e esp32dev -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos"`
Expected: `[SUCCESS]`.

- [ ] **Step 4: Commit**

```bash
git add lib/pantalla_ajustes/src/pantalla_confirmar_reset.h lib/pantalla_ajustes/src/pantalla_confirmar_reset.cpp
git commit -m "feat(pantalla_ajustes): confirmacion de reset total"
```

---

## Task 17: Sub-pantalla `PantallaConfigLocalizacion` (QR LAN)

**Files:**
- Create: `lib/pantalla_ajustes/src/pantalla_config_localizacion.h`
- Create: `lib/pantalla_ajustes/src/pantalla_config_localizacion.cpp`

- [ ] **Step 1: Crear el `.h`**

```cpp
#pragma once
#include "pantalla.h"
#include "gestor_pantallas.h"

class PantallaConfigLocalizacion : public pantallas::Pantalla {
 public:
  explicit PantallaConfigLocalizacion(pantallas::GestorPantallas& g) : gestor_(g) {}
  const char* nombre() const override { return "Localizacion"; }
  uint8_t id() const override { return 33; }
  void alEntrar() override { dirty_ = true; }
  void alTocar(int x, int y) override;
  void dibujar(uint32_t) override;

 private:
  pantallas::GestorPantallas& gestor_;
  bool dirty_ = true;
};
```

- [ ] **Step 2: Crear el `.cpp`**

```cpp
#include "pantalla_config_localizacion.h"
#include "tft_driver.h"
#include "qr_view.h"
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <string>
#include <vector>

void PantallaConfigLocalizacion::alTocar(int x, int y) {
  // Volver: [10, 90] × [210, 240].
  if (x >= 10 && x <= 90 && y >= 210 && y <= 240) {
    gestor_.volverAtras();
  }
}

void PantallaConfigLocalizacion::dibujar(uint32_t) {
  if (!dirty_) return;
  auto& tft = tft_driver::getTft();
  tft.fillRect(0, 20, 320, 220, 0x0000);

  if (WiFi.status() != WL_CONNECTED) {
    tft.setTextFont(2);
    tft.setTextColor(0xF800, 0x0000);
    tft.setCursor(10, 40);
    tft.print("Sin conexion WiFi ahora.");
    tft.setTextColor(0x07E0, 0x0000);
    tft.setCursor(10, 70);
    tft.print("Usa Reset total para");
    tft.setCursor(10, 88);
    tft.print("volver al portal AP.");
    tft.setCursor(10, 220);
    tft.print("< Volver");
    dirty_ = false;
    return;
  }

  std::string url = std::string("http://") + WiFi.localIP().toString().c_str() + "/config";
  std::vector<std::string> lineas = {
    "1. Escanea con",
    "   el movil",
    "",
    "(en la WiFi",
    "de casa)",
    "",
    "URL:",
    url,
  };
  qr_view::pintarPortalConQR(tft, "Reconfigurar", url, lineas);
  tft.setTextFont(2);
  tft.setTextColor(0x07E0, 0x0000);
  tft.setCursor(10, 222);
  tft.print("< Volver");
  dirty_ = false;
}
```

- [ ] **Step 3: Compilar**

Run: `~/.platformio/penv/bin/pio run -e esp32dev -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos"`
Expected: `[SUCCESS]`.

- [ ] **Step 4: Commit**

```bash
git add lib/pantalla_ajustes/src/pantalla_config_localizacion.h lib/pantalla_ajustes/src/pantalla_config_localizacion.cpp
git commit -m "feat(pantalla_ajustes): QR de la LAN para reconfigurar localizacion"
```

---

## Task 18: Sub-pantalla `PantallaCalibrarTouch` (4 esquinas)

**Files:**
- Create: `lib/pantalla_ajustes/src/pantalla_calibrar_touch.h`
- Create: `lib/pantalla_ajustes/src/pantalla_calibrar_touch.cpp`

- [ ] **Step 1: Crear el `.h`**

```cpp
#pragma once
#include "pantalla.h"
#include "gestor_pantallas.h"
#include "config_store.h"

class PantallaCalibrarTouch : public pantallas::Pantalla {
 public:
  PantallaCalibrarTouch(pantallas::GestorPantallas& g, Config& cfg)
    : gestor_(g), cfg_(cfg) {}
  const char* nombre() const override { return "Calibrar"; }
  uint8_t id() const override { return 35; }
  void alEntrar() override;
  void dibujar(uint32_t) override;

 private:
  pantallas::GestorPantallas& gestor_;
  Config& cfg_;
  int paso_ = 0;  // 0..3 esquinas, 4 = terminado
  int16_t rawX_[4] = {0, 0, 0, 0};
  int16_t rawY_[4] = {0, 0, 0, 0};
  bool dirty_ = true;
};
```

- [ ] **Step 2: Crear el `.cpp`**

```cpp
#include "pantalla_calibrar_touch.h"
#include "tft_driver.h"
#include "touch.h"
#include <TFT_eSPI.h>
#include <Arduino.h>

namespace {
struct Cruz { int x, y; };
const Cruz cruces[4] = {{20, 20}, {300, 20}, {20, 220}, {300, 220}};
}

void PantallaCalibrarTouch::alEntrar() {
  paso_ = 0;
  dirty_ = true;
}

void PantallaCalibrarTouch::dibujar(uint32_t) {
  if (!dirty_) return;
  auto& tft = tft_driver::getTft();
  if (paso_ >= 4) {
    // Guardar y salir.
    int16_t minX = std::min({rawX_[0], rawX_[1], rawX_[2], rawX_[3]});
    int16_t maxX = std::max({rawX_[0], rawX_[1], rawX_[2], rawX_[3]});
    int16_t minY = std::min({rawY_[0], rawY_[1], rawY_[2], rawY_[3]});
    int16_t maxY = std::max({rawY_[0], rawY_[1], rawY_[2], rawY_[3]});
    cfg_.touch_min_x = minX; cfg_.touch_max_x = maxX;
    cfg_.touch_min_y = minY; cfg_.touch_max_y = maxY;
    cfg_.touch_calibrado = true;
    ConfigStore::guardar(cfg_);
    touch::CalibracionTouch nueva{minX, maxX, minY, maxY, true};
    touch::setCalibracion(nueva);
    gestor_.volverAtras();
    dirty_ = false;
    return;
  }
  tft.fillScreen(0x0000);
  tft.setTextFont(2);
  tft.setTextColor(0x07E0, 0x0000);
  tft.setCursor(80, 110);
  tft.print("Toca la cruz");
  const Cruz& c = cruces[paso_];
  tft.drawLine(c.x - 8, c.y, c.x + 8, c.y, 0x07E0);
  tft.drawLine(c.x, c.y - 8, c.x, c.y + 8, 0x07E0);
  dirty_ = false;

  // Lectura bloqueante: sí, esta pantalla se queda hasta que se toque cada cruz.
  int16_t rx = 0, ry = 0;
  if (touch::leerCrudoBloqueante(rx, ry, /*timeoutMs=*/30000)) {
    rawX_[paso_] = rx; rawY_[paso_] = ry;
    ++paso_;
    dirty_ = true;
  } else {
    // Timeout: aborta la calibración sin guardar.
    gestor_.volverAtras();
  }
}
```

- [ ] **Step 3: Compilar**

Run: `~/.platformio/penv/bin/pio run -e esp32dev -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos"`
Expected: `[SUCCESS]`.

- [ ] **Step 4: Commit**

```bash
git add lib/pantalla_ajustes/src/pantalla_calibrar_touch.h lib/pantalla_ajustes/src/pantalla_calibrar_touch.cpp
git commit -m "feat(pantalla_ajustes): calibracion tactil por 4 esquinas"
```

---

## Task 19: `main.cpp` — ensamblar gestor + touch + registro de pantallas

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Reemplazar `src/main.cpp`**

Sustituir el contenido de `src/main.cpp` por:

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
#include "tft_driver.h"
#include "touch.h"

#include "pantalla.h"
#include "gestor_pantallas.h"
#include "renderizador_ui.h"
#include "pantalla_radar.h"
#include "pantalla_menu.h"
#include "pantalla_proximamente.h"
#include "pantalla_ajustes.h"
#include "pantalla_intervalo.h"
#include "pantalla_seleccion_vistas.h"
#include "pantalla_seleccion_vista_fija.h"
#include "pantalla_config_localizacion.h"
#include "pantalla_calibrar_touch.h"
#include "pantalla_confirmar_reset.h"

#include <TFT_eSPI.h>

namespace {

WifiHttpClient  g_http;
Config          g_cfg;
RadarState*     g_estado = nullptr;
uint32_t        g_ultimoIntentoWifiMs = 0;

// Renderer real: barra superior con "menú" a la izquierda, título centrado, dots
// del carrusel abajo-derecha del área de contenido.
class RenderizadorUiReal : public pantallas::IRenderizadorUi {
 public:
  void pintarBarraSuperior(const char* titulo, uint8_t dotAct, uint8_t nDots) override {
    auto& tft = tft_driver::getTft();
    tft.fillRect(0, 0, 320, 20, 0x0000);
    // Botón menú
    tft.setTextColor(0x07E0, 0x0000);
    tft.setTextFont(2);
    tft.setCursor(4, 2);
    tft.print("[=]");
    // Título centrado
    int16_t ancho = tft.textWidth(titulo);
    tft.setCursor((320 - ancho) / 2, 2);
    tft.print(titulo);
    // Dots
    if (nDots > 0) {
      const int xIni = 320 - (nDots * 10) - 4;
      for (int i = 0; i < nDots; ++i) {
        if (i == dotAct) tft.fillCircle(xIni + i * 10, 232, 3, 0x07E0);
        else             tft.drawCircle(xIni + i * 10, 232, 3, 0x07E0);
      }
    }
  }
  void limpiarAreaContenido() override {
    tft_driver::getTft().fillRect(0, 20, 320, 220, 0x0000);
  }
};

RenderizadorUiReal g_renderer;
pantallas::GestorPantallas* g_gestor = nullptr;
std::vector<pantallas::Pantalla*> g_pantallasParaBorrar;

bool conectarWifi() {
  StatusLed::setEstado(EstadoLed::CONECTANDO_WIFI);
  tft_driver::pintarSplash("Conectando WiFi", g_cfg.ssid);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(g_cfg.ssid.c_str(), g_cfg.password.c_str());
  uint32_t inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 20000) delay(200);
  if (WiFi.status() != WL_CONNECTED) return false;
  Serial.printf("[wifi] conectado, IP: %s\n", WiFi.localIP().toString().c_str());
  return true;
}

void tareaPoller(void*) {
  AdsbClient cliente(g_http);
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      g_estado->marcarStale();
      StatusLed::setEstado(EstadoLed::RADAR_ERROR);
      if (millis() - g_ultimoIntentoWifiMs > 60000) {
        Serial.println("[wifi] 60s sin conexion, reiniciando");
        ESP.restart();
      }
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
    g_ultimoIntentoWifiMs = millis();
    std::vector<Aeronave> aviones;
    bool ok = cliente.fetchCerca(g_cfg.lat, g_cfg.lon, g_cfg.radio_km, aviones);
    if (!ok) ok = cliente.fetchCerca(g_cfg.lat, g_cfg.lon, g_cfg.radio_km, aviones);
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

void tareaDisplay(void*) {
  for (;;) {
    // Reencolar cualquier evento tactil pendiente en el gestor.
    touch::EventoTactil ev;
    while (touch::esperarEvento(ev, /*timeoutMs=*/0)) {
      pantallas::EventoUi u;
      switch (ev.tipo) {
        case touch::TipoEvento::TAP:              u.tipo = pantallas::TipoEventoUi::TAP; break;
        case touch::TipoEvento::SWIPE_IZQUIERDA:  u.tipo = pantallas::TipoEventoUi::SWIPE_IZQUIERDA; break;
        case touch::TipoEvento::SWIPE_DERECHA:    u.tipo = pantallas::TipoEventoUi::SWIPE_DERECHA; break;
      }
      u.x = ev.x; u.y = ev.y;
      g_gestor->encolarEvento(u);
    }
    g_gestor->tick(millis());
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

void arrancarTouch() {
  touch::CalibracionTouch cal{g_cfg.touch_min_x, g_cfg.touch_max_x,
                              g_cfg.touch_min_y, g_cfg.touch_max_y,
                              g_cfg.touch_calibrado};
  if (!cal.valida) {
    // Defaults tentativos para poder navegar al calibrar.
    cal = touch::CalibracionTouch{300, 3800, 300, 3800, true};
  }
  touch::iniciar(cal);
}

void modoRadar() {
  g_estado = new RadarState(g_cfg.lat, g_cfg.lon, g_cfg.radio_km);
  RadarWebServer::iniciar(g_cfg, *g_estado, g_http);

  arrancarTouch();

  // Construir pantallas.
  auto* radar   = new PantallaRadar(*g_estado);
  auto* reloj   = new PantallaProximamente(1, "Reloj");
  auto* meteo   = new PantallaProximamente(2, "Meteo");
  auto* futbol  = new PantallaProximamente(3, "Futbol");
  auto* motogp  = new PantallaProximamente(4, "MotoGP");
  auto* f1      = new PantallaProximamente(5, "F1");

  static pantallas::GestorPantallas gestor(g_renderer);
  g_gestor = &gestor;

  // Sub-pantallas de ajustes.
  auto* intervalo    = new PantallaIntervalo(gestor, g_cfg);
  auto* selVistas    = new PantallaSeleccionVistas(gestor, g_cfg);
  auto* selVistaFija = new PantallaSeleccionVistaFija(gestor, g_cfg);
  auto* configLoc    = new PantallaConfigLocalizacion(gestor);
  auto* calibrar     = new PantallaCalibrarTouch(gestor, g_cfg);
  auto* confReset    = new PantallaConfirmarReset(gestor);
  PantallaAjustes::SubPantallas subs{intervalo, selVistas, selVistaFija,
                                     configLoc, calibrar, confReset};
  auto* ajustes = new PantallaAjustes(gestor, g_cfg, subs);

  // Menú (home).
  auto* menu = new PantallaMenu(gestor, ajustes);
  menu->configurarEntradas({
    {0, "Radar"}, {1, "Reloj"}, {2, "Meteo"},
    {3, "Futbol"}, {4, "MotoGP"}, {5, "F1"},
  });

  gestor.setHome(menu);
  for (auto* p : {static_cast<pantallas::Pantalla*>(radar),
                  reloj, meteo, futbol, motogp, f1,
                  menu, ajustes,
                  intervalo, selVistas, selVistaFija,
                  configLoc, calibrar, confReset}) {
    gestor.registrar(p);
  }

  gestor.configurarModo(
    g_cfg.modo == ModoVista::CARRUSEL ? pantallas::ModoGestor::CARRUSEL
                                      : pantallas::ModoGestor::FIJO,
    g_cfg.intervalo_carrusel_s,
    g_cfg.vistas_orden,
    g_cfg.vista_fija);
  gestor.iniciar(millis());

  // Forzar calibración si no está calibrado.
  if (!g_cfg.touch_calibrado) gestor.abrirEnPila(calibrar);

  xTaskCreatePinnedToCore(tareaPoller,  "poller",  8192, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(tareaDisplay, "display", 4096, nullptr, 1, nullptr, 1);
  Serial.println("[radar] modo operativo con carrusel");
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n===== Radar de vuelos ESP32 =====");

  tft_driver::iniciar();
  tft_driver::pintarSplash("Radar de vuelos", "arrancando...");

  if (!LittleFS.begin(true)) Serial.println("[fs] error montando LittleFS");
  StatusLed::iniciar(4, /*activoBajo=*/true);

  bool tieneCfg = ConfigStore::cargar(g_cfg);
  if (tieneCfg) {
    if (conectarWifi()) { modoRadar(); return; }
    Serial.println("[wifi] no conecta, entrando en portal");
  } else {
    Serial.println("[cfg] no hay config, entrando en portal");
  }

  uint8_t mac[6];
  WiFi.macAddress(mac);
  char sufijo[5];
  snprintf(sufijo, sizeof(sufijo), "%02X%02X", mac[4], mac[5]);
  std::string ap = std::string("RadarVuelos-") + sufijo;
  // El portal sigue reutilizando el helper QR ya existente.
  // (`WifiPortal::ejecutar` pinta su propia UI internamente.)
  WifiPortal::ejecutar(g_http);  // no retorna
}

void loop() { delay(1000); }
```

Nota importante: se elimina la llamada a `DisplayRadar::pintarPortalQR` — el portal ya la hace por su cuenta. También se elimina la inclusión de `display_radar.h` porque el radar vive ahora en `pantalla_radar`.

- [ ] **Step 2: Compilar firmware**

Run: `~/.platformio/penv/bin/pio run -e esp32dev -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos"`
Expected: `[SUCCESS]`. Puede haber warnings sobre punteros no liberados en `modoRadar()` — se ignora, la placa no se apaga.

- [ ] **Step 3: Comprobar que el `wifi_portal` sigue pintando el QR del AP**

Al modo portal se le llama `wifi_portal.cpp`, y aún depende de `DisplayRadar::pintarPortalQR`. Como se ha vaciado esa función delegándola a `qr_view` (Task 3), sigue funcionando; pero el archivo `display_radar.cpp` debe seguir compilando aunque `main.cpp` ya no la use.

Verificar que `pio run` sigue OK sin errores de link.

- [ ] **Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "feat(main): ensamblar gestor de pantallas, touch y sub-pantallas de ajustes"
```

---

## Task 20: Eliminar `lib/display_radar` y usar el QR desde `wifi_portal`

**Files:**
- Modify: `lib/wifi_portal/src/wifi_portal.cpp` (pintar QR con `qr_view` en el arranque del portal)
- Delete: `lib/display_radar/`

`display_radar` fue el puente durante la migración; ya no lo necesitamos. `wifi_portal` sigue queriendo pintar su QR del AP; lo hará directamente con `qr_view`.

- [ ] **Step 1: Añadir en `wifi_portal.cpp` la impresión del QR al iniciar (antes del bucle)**

En `lib/wifi_portal/src/wifi_portal.cpp`, dentro de `WifiPortal::ejecutar`, tras `server.begin();` y antes del `for(;;)` final, insertar:

```cpp
// Pintar QR del AP en pantalla (antes vivía en DisplayRadar::pintarPortalQR).
{
  std::vector<std::string> lineas = {
    "1. WiFi:",
    ssidAp.c_str(),
    "",
    "2. Escanea",
    "   el QR",
    "",
    "o abre la",
    "URL a mano",
  };
  qr_view::pintarPortalConQR(tft_driver::getTft(), "Modo Portal",
                             "http://192.168.4.1/", lineas);
}
```

Y añadir los includes al principio del fichero:

```cpp
#include "qr_view.h"
#include "tft_driver.h"
#include <vector>
```

- [ ] **Step 2: Borrar la lib `display_radar`**

Run:
```bash
rm -rf lib/display_radar
```

- [ ] **Step 3: Compilar firmware para verificar que nada la enlaza**

Run: `~/.platformio/penv/bin/pio run -e esp32dev -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos"`
Expected: `[SUCCESS]`. Si algún fichero incluía `display_radar.h`, el compilador lo señala y se corrige.

- [ ] **Step 4: Commit**

```bash
git add lib/wifi_portal lib/display_radar
git commit -m "refactor: retirar lib display_radar; wifi_portal usa qr_view directamente"
```

---

## Task 21: Verificación en placa

**Files:** ninguno; sólo verificación manual.

- [ ] **Step 1: Compilar y subir firmware**

Run: `~/.platformio/penv/bin/pio run -e esp32dev -t upload --upload-port /dev/cu.usbserial-110 -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos"`
Expected: `[SUCCESS]`, `Hard resetting via RTS pin...`.

- [ ] **Step 2: Subir LittleFS**

Run: `~/.platformio/penv/bin/pio run -e esp32dev -t uploadfs --upload-port /dev/cu.usbserial-110 -d "/Users/albertodelpozo/Documents/Proyectos/PlatformIO/Projects/Radar vuelos"`
Expected: `[SUCCESS]`.

- [ ] **Step 3: Abrir monitor serie y verificar el arranque**

Run: `~/.platformio/penv/bin/pio device monitor -b 115200 -p /dev/cu.usbserial-110`
Expected:
- `===== Radar de vuelos ESP32 =====`
- Con config existente: `[wifi] conectado, IP: <ip>` + `[touch] task de poll arrancada` + `[radar] modo operativo con carrusel`.
- Sin config: entra al portal y pinta su QR.

- [ ] **Step 4: Verificación funcional manual**

- [ ] Al primer arranque tras el update, si `touch_calibrado == false`, se muestra la pantalla de calibración con 4 cruces sucesivas y tras completarla se guarda y aparece el radar (o la vista inicial del carrusel).
- [ ] El swipe horizontal cambia entre vistas activas del carrusel.
- [ ] El tap en la esquina superior izquierda (botón "[=]") lleva al menú desde cualquier vista.
- [ ] En Ajustes, tap en `Modo` alterna Carrusel/Fijo y se refleja al salir del menú.
- [ ] En modo carrusel, `Intervalo` sube/baja de 5 en 5 con los botones −/+.
- [ ] En `Vistas activas y orden`, marcar/desmarcar cambia el subset del carrusel; ↑/↓ reordena; no permite quedar sin vistas.
- [ ] En modo fijo, `Vista fija` selecciona la vista y al volver al menú se ve sólo esa vista.
- [ ] `Reconfigurar localización` muestra un QR con `http://<ip-lan>/config`; escaneando con el móvil se abre `config.html` y `POST /api/config` reinicia la placa como hasta ahora.
- [ ] `Calibrar táctil` puede relanzarse desde el menú y sobrescribe la calibración.
- [ ] `Reset total` pide confirmación; al confirmar, la placa arranca en modo portal (QR del AP).
- [ ] El radar sigue mostrando aviones detectados con el mismo comportamiento que antes del refactor.

- [ ] **Step 5: Commit del changelog (opcional)**

Si algo se ha ajustado en placa (por ejemplo, umbrales del gesture detector o pines del touch en una variante rara del CYD), commiteando los cambios.

---

## Self-review

- **Cobertura del spec**: cada apartado del spec aparece en tareas:
  - Driver táctil + calibración → Task 5, 6, 18.
  - Interfaz `Pantalla` + `GestorPantallas` → Task 7, 8.
  - Refactor del radar → Task 9.
  - Menú → Task 10.
  - PantallaProximamente para bloques 2–6 → Task 11.
  - Ajustes con todas las sub-pantallas → Tasks 12–18.
  - QR LAN → Task 17.
  - Reset total → Task 16.
  - Persistencia + migración v1→v2 → Task 2 con tests.
  - Splash previo NO pasa por el gestor → Task 19 (usa `tft_driver::pintarSplash` en `setup()` y en `conectarWifi`).
  - `qr_view` compartido con `wifi_portal` → Task 3, 20.
  - No se altera `wifi_portal` funcionalmente → sólo cambia la impresión del QR a `qr_view` en Task 20.
  - Tests native para `GestorPantallas` y `ConfigStore` v2 → Task 2 y 8.

- **Placeholder scan**: sin TBD/TODO. Todos los steps de código muestran el código completo. Los mensajes de commit están escritos, los comandos también.

- **Consistencia de tipos**: `Pantalla`, `IRenderizadorUi`, `GestorPantallas`, `EventoUi`, `TipoEventoUi`, `ModoGestor` — declarados en Task 7/8 y usados con el mismo nombre en todas las tareas posteriores. `touch::EventoTactil`, `touch::TipoEvento`, `touch::CalibracionTouch` — coherentes entre Task 5, 6, 18 y 19. `Config` extendida en Task 2 y usada en todo el resto.
