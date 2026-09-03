# Pantalla Meteo + swipe vertical — diseño

- **Fecha:** 2026-09-03
- **Proyecto:** Radar de vuelo (ESP32 + Cheap Yellow Display)
- **Bloque:** 3 de 6 (Pantalla Meteo)
- **Autor:** Alberto del Pozo (tidehub)
- **Estado:** diseño aprobado conversacionalmente

## Contexto

Bloque 1: fundación táctil + carrusel. Bloque 2: reloj + paleta dark. Bloque 3 sustituye el placeholder Meteo por una vista real con datos de **Open-Meteo** (sin API key) sobre la lat/lon del `Config`. Introduce **swipe vertical** (gesto nuevo) para alternar entre dos sub-vistas dentro de la misma pantalla: próximas 12 horas y próximos 5 días. Reutiliza la paleta dark.

## Objetivos

- `PantallaMeteo` con dos sub-vistas alternables por swipe vertical:
  - **Horas**: temp actual + icono grande + viento + tira de 6 horas próximas.
  - **Días**: temp actual + icono + 5 días con min/max e icono por día.
- Cliente `MeteoClient` para Open-Meteo con parser JSON del payload combinado (current + hourly + daily). Testeable en native con `MockHttpClient`.
- Ampliación táctil: `SWIPE_ARRIBA` / `SWIPE_ABAJO` en `GestureDetector`, `touch::Touch`, `pantallas` y su enum. `Pantalla::alDeslizar` migra a `enum class Direccion`.
- Iconos meteo dibujados a mano con primitivas TFT_eSPI (sol, nube, lluvia, tormenta, niebla, nieve).
- Refresco cada 30 min en task dedicada de core 0.

## No-objetivos

- Sensación térmica, humedad, presión, UV, precipitación acumulada. Open-Meteo los ofrece; los dejamos fuera por scope.
- Configuración de unidades (Celsius fijo, km/h fijo).
- Múltiples ubicaciones o cambios rápidos de ciudad.
- Alertas meteo push, notificaciones.
- Rediseño visual de `pantalla_menu`, `pantalla_ajustes`, sub-pantallas de ajustes o placeholders restantes.

## Arquitectura

### Ampliación táctil (cascada desde el detector al gestor)

**`lib/touch/src/gesture_detector.{h,cpp}`**
- Enum `TipoEvento` amplía con `SWIPE_ARRIBA = 3`, `SWIPE_ABAJO = 4`.
- `onRelease`: si `dur < UMBRAL_SWIPE_MS && |dy| > UMBRAL_SWIPE_PX && |dy| > |dx|` → produce `SWIPE_ARRIBA` (dy<0) o `SWIPE_ABAJO` (dy>0). El check horizontal existente se mantiene con estricta desigualdad `|dx| > |dy|` para que no colisionen.
- Tests native añadidos: `swipe_arriba`, `swipe_abajo`, `swipe_diagonal_predomina_vertical`, `swipe_horizontal_estricto_no_es_vertical`.

**`lib/touch/src/touch.cpp`**
- `tareaTouch` reencola los dos casos nuevos en la cola FreeRTOS sin cambios de lógica.

**`lib/pantallas/`**
- Nuevo `enum class Direccion : uint8_t { IZQUIERDA=0, DERECHA=1, ARRIBA=2, ABAJO=3 }` en `pantalla.h`.
- La firma virtual pasa a `virtual void alDeslizar(Direccion dir) {}`. Todas las pantallas concretas heredan el `{}` default salvo `PantallaMeteo`, que implementa.
- `TipoEventoUi` en `gestor_pantallas.h` amplía con `SWIPE_ARRIBA`, `SWIPE_ABAJO`.
- `GestorPantallas::tick` sobre esos dos casos:
  - Si `pilaUi_` no vacía → ignorar.
  - Si vacía → `actual_->alDeslizar(dir)`. **No cambia** de vista del carrusel.
- Los swipes horizontales siguen operando exactamente como hoy (cambian la vista activa en modo CARRUSEL, ignoran si FIJO o si hay pila UI). No se delegan a la pantalla actual.
- Tests native añadidos: `swipe_vertical_se_delega_a_pantalla_actual`, `swipe_vertical_ignorado_con_pila_ui`.

**`src/main.cpp`**
- El switch del traductor `touch::TipoEvento` → `pantallas::TipoEventoUi` cubre los 5 casos.

### `lib/meteo_client/`

Sin dependencias del display. Depende de `IHttpClient` y ArduinoJson.

**`meteo_client.h`**
```cpp
#pragma once
#include "http_client.h"
#include <cstdint>
#include <string>
#include <vector>

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

  // Bloqueante. HTTPS a api.open-meteo.com. TZ Europe/Madrid en la request.
  // Devuelve true si el JSON se parseó correctamente y llenó el snapshot.
  bool fetch(double lat, double lon, MeteoSnapshot& out);

  // Parser puro para tests native (no toca la red).
  static bool parsear(const std::string& json, MeteoSnapshot& out);

 private:
  IHttpClient& http_;
};
```

**`meteo_client.cpp`** — `fetch` construye la URL de Open-Meteo con los parámetros `current_weather=true&hourly=temperature_2m,weather_code&daily=temperature_2m_max,temperature_2m_min,weather_code&forecast_days=5&timezone=Europe/Madrid`; llama a `http_.get(url, body, status, 15000)`; delega en `parsear(body, out)`. `parsear` extrae:
- `current_weather.temperature`, `current_weather.weathercode`, `current_weather.windspeed`.
- `hourly.time[i]`, `hourly.temperature_2m[i]`, `hourly.weather_code[i]` — filtra a partir del índice actual (`current_weather.time` marca la hora en curso), coge hasta 12.
- `daily.time[j]`, `daily.temperature_2m_max[j]`, `daily.temperature_2m_min[j]`, `daily.weather_code[j]` — coge 5.
- `time` viene como `"2026-09-03T14:00"`; extraer hora con `atoi(cadena+11)` y día de mes con `atoi(cadena+8)`.

Mapeo WMO → categoría (`enum class IconoMeteo { SOL, NUBE, LLUVIA, NIEVE, TORMENTA, NIEBLA }`):
- 0 → SOL
- 1, 2, 3 → NUBE
- 45, 48 → NIEBLA
- 51..67, 80..82 → LLUVIA
- 71..77, 85, 86 → NIEVE
- 95..99 → TORMENTA
- default → NUBE

### `lib/pantalla_meteo/`

Depende de `pantallas`, `tft_driver`, `paleta_dark`, `meteo_client`.

**`pantalla_meteo.h`**
```cpp
#pragma once
#include "pantalla.h"
#include "meteo_client.h"
#include <cstdint>

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
  void dibujarHoras(TFT_eSPI& tft);
  void dibujarDias(TFT_eSPI& tft);
  void dibujarActual(TFT_eSPI& tft, int y);
  void dibujarIndicadorSub(TFT_eSPI& tft);
  void dibujarIcono(TFT_eSPI& tft, int cx, int cy, int lado, int codigoWmo);

  const MeteoSnapshot& snap_;
  SubVista sub_ = SubVista::HORAS;
  bool     dirty_ = true;
  uint32_t ultObtenidoMs_ = 0;   // para detectar snapshot nuevo
};
```

**Layout Horas** (área 320×220 bajo la barra):
- Fondo dark.
- Temp actual en font 7 grande, `x = 20, y = OFFSET_Y + 12`. `°C` en font 4 pegado.
- Icono actual (48 px) centrado verticalmente a la derecha, `x ≈ 220`.
- Segunda fila (font 2 gris): `"Viento 12 km/h"` a la izquierda.
- Tira horizontal de 6 horas centrada en `y ≈ OFFSET_Y + 110`. Cada slot 48 px de ancho: hora (font 1), icono (24 px), temp (font 2).
- `dibujarIndicadorSub` pinta dos rectitos arriba-der (`H` relleno, `D` vacío).

**Layout Días**:
- Fondo dark.
- Mismo bloque actual arriba (temp + icono + viento).
- 5 filas apiladas a partir de `y ≈ OFFSET_Y + 90`, cada una 22 px alta: `[Día abrev] [icono 20 px] [tmin°-tmax°]`. Días: `Lun/Mar/Mie/Jue/Vie/Sab/Dom` calculados a partir de `dia_mes` con `tm.tm_wday` de `struct tm` reconstruido con `mktime`.
- `dibujarIndicadorSub` pinta `H` vacío, `D` relleno.

**Iconos** (funciones locales, todas centradas en `(cx,cy)`, con `lado` como diámetro nominal):
- SOL: `fillCircle(cx, cy, lado/3, AMARILLO)` + 8 rayos como `drawLine`.
- NUBE: 3 `fillCircle` grises solapados + `fillRect` base.
- LLUVIA: NUBE + 3 `drawLine` verticales azules cortas debajo (gotas).
- TORMENTA: NUBE + polígono relleno amarillo tipo rayo (`fillTriangle` x2).
- NIEBLA: 4 `drawFastHLine` grises horizontales.
- NIEVE: NUBE + 3 asteriscos ("*" font 2) blancos debajo.

**Comportamiento**:
- `alEntrar` marca dirty y resetea el "ult obtenido".
- `alDeslizar(ARRIBA|ABAJO)` alterna sub y marca dirty. IZQ/DER: no-op (los captura el gestor).
- `dibujar`: si `snap_.obtenido_ms != ultObtenidoMs_` o `dirty_` → repinta la sub-vista completa. Si sólo cambia el tick del reloj sin datos nuevos, no repinta.

### `src/main.cpp`

Cambios:
1. Include `pantalla_meteo.h` y `meteo_client.h`.
2. `MeteoSnapshot g_snapMeteo;` global.
3. Justo antes de `xTaskCreatePinnedToCore(tareaPoller, ...)`, crear una task `tareaMeteoRefresh` en core 0, prio 1, stack 6144:
   ```cpp
   xTaskCreatePinnedToCore(tareaMeteoRefresh, "meteo", 6144, nullptr, 1, nullptr, 0);
   ```
   La task hace: primer `MeteoClient(g_http).fetch(g_cfg.lat, g_cfg.lon, g_snapMeteo)` al arrancar (con 5 s de gracia para dar tiempo a NTP), luego `vTaskDelay(1800000 / portTICK)` en bucle (30 min).
4. Swap del placeholder Meteo:
   ```cpp
   auto* meteo = new PantallaMeteo(g_snapMeteo);
   ```
5. Traductor de eventos en `tareaDisplay` amplía a los 5 casos:
   ```cpp
   case touch::TipoEvento::SWIPE_ARRIBA: u.tipo = pantallas::TipoEventoUi::SWIPE_ARRIBA; break;
   case touch::TipoEvento::SWIPE_ABAJO:  u.tipo = pantallas::TipoEventoUi::SWIPE_ABAJO;  break;
   ```

`g_snapMeteo` es la única variable compartida entre task de meteo y task de display. Aceptamos el mismo pragmatismo que con `s_cal` en `touch`: escritura desde una task, lecturas desde otra, sin mutex; con `MeteoSnapshot::obtenido_ms` como versión "canario". Si vemos parcheo en placa, añadimos double-buffer.

## Testing

### `test/test_meteo_client/test_main.cpp` (Unity, native)

- `test_parsear_payload_valido_extrae_actual_horas_y_dias`: JSON pegado en el fichero → asserts sobre `temp_actual_c`, número de horas, número de días, `dias[0].tmin < dias[0].tmax`.
- `test_parsear_json_malformado_devuelve_false`: `"{"` → false.
- `test_parsear_sin_hourly_deja_horas_vacio_pero_ok`: payload con `current_weather` y `daily` pero sin `hourly` → `snapshot.ok = true`, `snapshot.horas.empty()`.
- `test_parsear_wmo_95_es_tormenta`: verificar el mapping WMO → categoría con un helper `MeteoClient::categoria(int wmo)` público estático (útil para test y para la vista).

### `test/test_gesture_detector` amplía

- `swipe_arriba_produce_evento`, `swipe_abajo_produce_evento`.
- `swipe_diagonal_predomina_vertical`: `dx=30, dy=80` → `SWIPE_ARRIBA`.
- `swipe_horizontal_no_es_vertical`: `dx=80, dy=30` → `SWIPE_DERECHA` (no arriba/abajo).

### `test/test_gestor_pantallas` amplía

- `swipe_vertical_se_delega_a_pantalla_actual`: encolar `SWIPE_ARRIBA`, la fake registra `ultDireccion = Direccion::ARRIBA`.
- `swipe_vertical_ignorado_con_pila_ui`: con pila abierta, el swipe vertical no llega a la pantalla ni cambia estado.

### Verificación en placa

- Sub-vista Horas: temp actual visible, viento, tira de 6 horas.
- Swipe abajo → sub-vista Días.
- Swipe arriba desde Días → vuelve a Horas.
- Swipe izquierda/derecha: cambia de vista del carrusel (no altera sub-vista de Meteo).
- Sin conexión al arrancar: la vista muestra `"Meteo: sin datos"` centrado hasta que el fetch triunfa.
- Tras 30 min: log `[meteo] refresh OK` y snapshot renovado.

## Riesgos y decisiones asumidas

- Flash **87.9%** actual. Meteo + iconos + parser JSON sumarán varios KB; márgen aún holgado pero crítico. Si sube por encima de 92%, retirar `LOAD_FONT7` y usar font 6 en el reloj (más pequeño pero suficiente).
- Cambio de firma `alDeslizar(int)` → `alDeslizar(Direccion)`: **impacta a cualquier `Pantalla` que lo hubiera implementado**. Hoy sólo `PantallaFake` (test) lo hace, y trivialmente. Sin impacto en las pantallas actuales del proyecto.
- Colisión de heap TLS: task de meteo hace HTTPS ~35 KB heap simultáneo con la task ADSB del radar. Refresh cada 30 min vs. 3 s del radar → probabilidad de coincidencia < 1%. Aceptable; si vemos crashes en placa, semaforo compartido para HTTPS.
- Zona horaria: pasamos `timezone=Europe/Madrid` a Open-Meteo. La API devuelve tiempos ya en local (`hourly.time` = local, no UTC). No hay que aplicar TZ en el parser.
- Sin persistencia. Al reiniciar la placa se pierde el último snapshot y se re-fetch al arrancar.

## Fuera de alcance / siguientes bloques

- Bloque 4: Fútbol Liga española (proveedor por decidir).
- Bloque 5: MotoGP.
- Bloque 6: F1 (Jolpica-F1).
- Fondo común: `lib/cache_datos/` con TTLs y task única, refactor cuando el patrón se repita 2 veces más.
