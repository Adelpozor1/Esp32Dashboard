# Carrusel de pantallas con panel táctil — diseño

- **Fecha:** 2026-09-02
- **Proyecto:** Radar de vuelo (ESP32 + Cheap Yellow Display)
- **Bloque:** 1 de 6 (fundación táctil + arquitectura de pantallas)
- **Autor:** Alberto del Pozo (tidehub)
- **Estado:** diseño aprobado a nivel conversacional, pendiente de revisión del documento

## Contexto

El firmware actual muestra una única pantalla (radar tipo sonar) sobre TFT ILI9341 en la Cheap Yellow Display (CYD, placa `ESP32-2432S028R`). El display tiene panel táctil resistivo XPT2046 con bus SPI separado del TFT, hoy sin usar. Se quiere aprovechar el táctil para introducir varias vistas informativas (radar, reloj, meteo, fútbol Liga española, MotoGP, F1) y darle al usuario control sobre cómo se enseñan.

Este documento describe únicamente el bloque **fundación**: driver táctil, arquitectura de pantallas y carrusel/menú configurable. Las vistas de contenido nuevas (reloj, meteo y deportes) se especifican en documentos separados, uno por bloque.

## Objetivos

- Habilitar el panel táctil XPT2046 con calibración persistida.
- Introducir una arquitectura de **pantallas** con interfaz común, permitiendo añadir vistas nuevas sin tocar la infraestructura.
- Ofrecer al usuario un **menú principal** al que se vuelve desde cualquier vista tocando un botón fijo.
- Dentro del menú, configurar dos modos de operación:
  - **Fijo**: una sola vista activa.
  - **Carrusel**: rotación automática entre un subconjunto de vistas en un orden dado, con intervalo configurable, sin bloquear el swipe/tap manual.
- Permitir **reconfigurar la localización** (dirección y radio) desde el menú, sin pasar por el flujo de emparejamiento WiFi. Mostrar un QR de la LAN que abre `config.html` en el móvil.
- Mantener disponible un **reset total** que borra la config y vuelve al portal AP.
- Migrar la persistencia (`ConfigStore`) sin perder configuraciones existentes.

## No-objetivos (fuera de este bloque)

- Implementación real de las vistas Reloj, Meteo, Fútbol, MotoGP y F1. En este bloque sólo se registran como `PantallaProximamente` (placeholder) para poder probar el carrusel y el menú con más de una entrada.
- Rediseño del flujo del portal AP inicial. Sigue funcionando como hoy (`wifi_portal.cpp`).
- Rediseño del geocoder, la API de aviones o el radar en sí.

## Alcance funcional

Al terminar este bloque, en la placa se puede:

1. Arrancar en modo Radar como hasta ahora.
2. Tocar la esquina superior izquierda para ir al **menú principal**.
3. Desde el menú, entrar en cualquier vista disponible.
4. Desde una vista, tocar el mismo botón para volver al menú.
5. En modo carrusel, ver la rotación automática y adelantarla con swipe horizontal (adelantar reinicia el temporizador).
6. En modo fijo, ver siempre la vista elegida.
7. Configurar en el menú: modo (Fijo/Carrusel), intervalo (5–120 s), qué vistas se incluyen en el carrusel y en qué orden, y (en modo fijo) qué vista es la fija.
8. Reconfigurar la localización desde el menú mediante un QR de la LAN que abre `config.html` en el móvil.
9. Reset total desde el menú que borra la config y arranca el portal AP.
10. Calibrar el táctil desde el menú, o automáticamente al primer arranque tras la migración a la nueva versión de `Config`.

## Arquitectura

### Módulos nuevos y refactorizados

El módulo actual `display_radar` se descompone para que ningún fichero tenga demasiadas responsabilidades:

- `lib/tft_driver/` — inicialización de TFT_eSPI (`init`, `setRotation(1)`, backlight en GPIO 21) y helpers de fuentes/colores compartidos. Todo lo que hoy vive en `DisplayRadar::iniciar` referente al hardware TFT.
- `lib/touch/` — driver XPT2046 usando `PaulStoffregen/XPT2046_Touchscreen`. Contiene:
  - `Touch::iniciar(pines, calibracion)` — SPI del touch en pines separados del TFT.
  - `Touch::calibrar()` — flujo de 4 esquinas, devuelve `{min_x, max_x, min_y, max_y}`.
  - `Touch::taskPoll()` — task FreeRTOS en core 0, poll cada 50 ms.
  - Detección de gesto: **Tap** (touch + release en <300 ms, desplazamiento <20 px) y **Swipe** horizontal (>60 px en X en <400 ms).
  - Cola FreeRTOS `QueueHandle_t` de eventos `{EventoTactil::TAP|SWIPE_L|SWIPE_R, x, y}`.
- `lib/pantallas/` — interfaz común `Pantalla`:
  ```cpp
  class Pantalla {
   public:
    virtual ~Pantalla() = default;
    virtual const char* nombre() const = 0;
    virtual uint8_t id() const = 0;
    virtual void alEntrar() {}
    virtual void alSalir() {}
    virtual void alTocar(int x, int y) {}
    virtual void alDeslizar(int direccion) {}  // -1 izq, +1 der
    virtual void dibujar(uint32_t msAhora) = 0;
  };
  ```
  Y el orquestador `GestorPantallas`:
  - Registro de pantallas disponibles (`registrar(Pantalla*)`).
  - Referencia a `Config` para modo, intervalo, vistas activas y orden.
  - Estado interno: pantalla actual, `stackUi` (para menú → sub-menús), timer del carrusel.
  - `tick(uint32_t msAhora)` invocada por la task de display: procesa eventos táctiles pendientes, chequea rotación automática y llama a `pantalla->dibujar(msAhora)`.
  - API interna: `mostrarPorId(uint8_t)`, `abrirEnPila(Pantalla*)`, `volverAtras()`, `siguienteEnCarrusel()`, `anteriorEnCarrusel()`.
  - La barra superior con el botón ☰ y los dots del carrusel los pinta el `GestorPantallas` **encima** del área de contenido de la pantalla activa, para no duplicarlo en cada pantalla. La pantalla activa recibe la sub-área 320×220 para dibujar.
- `lib/pantalla_radar/` — el código de sonar actual migrado a `PantallaRadar`, dueña del sprite 240×240 8bpp. Encapsula los helpers `dibujarBaseSonar`, `dibujarBarrido`, `dibujarIconoAvion`, `dibujarAvionSonar`, `pintarPanelSonar` que hoy están en `display_radar.cpp`.
- `lib/pantalla_menu/` — `PantallaMenu`, home. Lista vertical de las vistas disponibles + entrada final "⚙ Ajustes". Recibe tap por fila.
- `lib/pantalla_ajustes/` — sub-pantallas de configuración: `PantallaAjustes` (índice), `PantallaSeleccionVistas` (checklist + reorden), `PantallaSeleccionVistaFija` (selector), `PantallaIntervalo` (picker −/+), `PantallaConfigLocalizacion` (QR LAN), `PantallaCalibrarTouch`, `PantallaConfirmarReset`.
- `lib/qr_view/` — helper `pintarQR(titulo, url, subtexto)` con quiet zone blanca y escala automática por longitud. Extraído del actual `DisplayRadar::pintarPortalQR`.

### Módulos existentes tocados

- `lib/config_store/` — extender `Config`, bump `VERSION` de 1 a 2, añadir migración v1→v2 en `deserializar`.
- `wifi_portal.cpp` — usa `qr_view` en vez de duplicar la lógica del QR. Sin cambios de API pública.
- `src/main.cpp` — registra las pantallas disponibles y arranca la task de touch y el `GestorPantallas`. La task actual de display sigue existiendo pero llama a `GestorPantallas::tick`.

### Módulos no tocados

`adsb_client`, `geocoder`, `geo_math`, `radar_state`, `status_led`, `web_server`, `http_client`.

## Modelo de datos y persistencia

### Enum de vistas

```cpp
enum class IdVista : uint8_t {
  RADAR   = 0,
  RELOJ   = 1,
  METEO   = 2,
  FUTBOL  = 3,
  MOTOGP  = 4,
  F1      = 5,
};
```

Estos ids son estables: aunque en el futuro se reordenen las pantallas dentro del código, los valores persistidos en NVS mantienen su significado.

### Extensión de `Config`

```cpp
enum class ModoVista : uint8_t { FIJO = 0, CARRUSEL = 1 };

struct Config {
  // Campos v1 (existentes) — se mantienen tal cual:
  std::string ssid;
  std::string password;
  std::string direccion;
  double lat = 0.0;
  double lon = 0.0;
  int    radio_km = 25;

  // Campos v2 nuevos:
  ModoVista modo = ModoVista::CARRUSEL;
  uint16_t  intervalo_carrusel_s = 10;
  uint8_t   vista_fija = 0;                          // id de IdVista
  std::vector<uint8_t> vistas_orden = {0, 1, 2, 3, 4, 5};  // orden del carrusel; también define subset activo (si un id no está aquí, no forma parte del carrusel)

  // Calibración táctil:
  int16_t touch_min_x = 0, touch_max_x = 0;
  int16_t touch_min_y = 0, touch_max_y = 0;
  bool    touch_calibrado = false;
};
```

### Formato binario (`ConfigStore` v2)

Se mantiene el layout binario existente con `MAGIC = 0xC0DE` en `[0..1]` y `VERSION` en `[2]`. Después de los campos v1 (`lat`, `lon`, `radio_km`, `ssid`, `password`, `direccion`), se añaden los campos v2 en este orden:

```
[..]  modo             (uint8)
[..]  intervalo        (uint16 LE)
[..]  vista_fija       (uint8)
[..]  n_vistas         (uint8)
[..]  vistas_orden[]   (n_vistas × uint8)
[..]  touch_min_x      (int16 LE)
[..]  touch_max_x      (int16 LE)
[..]  touch_min_y      (int16 LE)
[..]  touch_max_y      (int16 LE)
[..]  touch_calibrado  (uint8, 0/1)
```

### Migración v1 → v2

En `ConfigStore::deserializar`:

1. Leer `magic` y `version`.
2. Si `version == 1`: leer los campos v1 como hasta ahora; rellenar los campos v2 con los defaults del `struct Config`. Devolver true.
3. Si `version == 2`: leer todo el layout completo.
4. Cualquier otra versión: false.

La próxima vez que se llame a `ConfigStore::guardar`, se persiste ya en formato v2. No se hace escritura "eager" de migración: se convierte al primer save. Esto evita fallos silenciosos si la placa se corta antes de que el usuario abra ajustes.

Tests native cubren:
- Serializar v2 y deserializar → coincide.
- Deserializar buffer v1 conocido → carga v1 correctamente + rellena defaults v2.

## UI / UX y layouts

Pantalla física: 320×240 landscape (rotation 1).

### Barra superior común (20 px)

Pintada por `GestorPantallas`, no por cada pantalla:

- Zona tocable de `[0,0]` a `[40,20]` con icono "☰" (menú). Tap → volver al menú principal, vaciando el stack de UI.
- Título centrado (`pantalla->nombre()`).
- Zona `[290,0]` a `[320,20]` — reservada, no se pinta contenido en este bloque.

### Área de contenido

- Coordenadas `[0,20]` a `[320,240]` (320×220). La pantalla dibuja aquí. `PantallaRadar` se adapta a 320×220 (el sprite del sonar sigue siendo 220×220 centrado, panel derecho 100×220).

### Indicador de carrusel

- Sólo si `modo == CARRUSEL`.
- Dots en fila horizontal, esquina inferior derecha, dentro del área de contenido, no encima. Ancho total ~60 px, alto 6 px. El dot correspondiente a la vista actual se pinta relleno; los demás vacíos.

### Menú principal (`PantallaMenu`)

Lista vertical de las vistas disponibles + entrada final "⚙ Ajustes":

```
[Radar]           →
[Reloj]           →
[Meteo]           →
[Fútbol]          →
[MotoGP]          →
[F1]              →
[⚙ Ajustes]       →
```

Cada fila 30 px de alto, tap entra en la vista o abre `PantallaAjustes`.

### Menú de ajustes (`PantallaAjustes`)

Lista vertical con scroll si es necesario:

- `Modo: [Carrusel|Fijo]` — tap alterna el modo, se guarda inmediatamente.
- `Intervalo: 10 s` — visible sólo si `modo == CARRUSEL`. Tap → `PantallaIntervalo` (picker `−` / `+`, límites 5–120 s, paso 5 s).
- `Vistas activas y orden` — visible sólo si `modo == CARRUSEL`. Tap → `PantallaSeleccionVistas`.
- `Vista fija: <nombre>` — visible sólo si `modo == FIJO`. Tap → `PantallaSeleccionVistaFija`.
- `Reconfigurar localización` — tap → `PantallaConfigLocalizacion`.
- `Calibrar táctil` — tap → `PantallaCalibrarTouch`.
- `Reset total` — tap → `PantallaConfirmarReset`.
- `◀ Volver` — última fila, vuelve al menú.

Todos los cambios se persisten con `ConfigStore::guardar` al confirmar en cada sub-pantalla, no al salir del menú.

### Selección de vistas y orden (`PantallaSeleccionVistas`)

- Lista con las 6 vistas.
- Cada fila: `[✓] Nombre  ↑  ↓`. Tap en el check activa/desactiva. Tap en ↑/↓ mueve la fila.
- Al menos una vista activa siempre: si el subset ya tiene una sola marcada, su check aparece deshabilitado (no se puede desmarcar la última). Los demás checks funcionan normalmente.
- Botón `◀ Volver` guarda `vistas_orden` en `Config` (sólo las activas, en el orden mostrado).

### Reconfigurar localización (`PantallaConfigLocalizacion`)

- Si `WiFi.status() == WL_CONNECTED` y `WiFi.localIP()` es válida:
  - Pinta QR con `http://<ip>/config` mediante `qr_view::pintarQR`.
  - Subtexto: "Escanea con el móvil (misma WiFi de casa)".
  - Debajo, texto secundario: la URL literal en caso de que el escaneo falle.
- Si no hay IP LAN válida:
  - Pinta mensaje "Sin conexión WiFi ahora mismo".
  - Botón `Reset total` como salida (que fuerza portal AP).
- Botón `◀ Volver` de vuelta al menú.

Cuando el usuario cambia dirección o radio desde el móvil, `web_server` guarda en NVS y reinicia (comportamiento actual). La placa arranca con la nueva configuración.

### Reset total (`PantallaConfirmarReset`)

- Texto "Esto borrará TODA la configuración (WiFi, dirección, ajustes). ¿Seguro?".
- Botones "Cancelar" y "Sí, borrar".
- "Sí, borrar" → `ConfigStore::borrar()` + `ESP.restart()`.

### Calibración táctil (`PantallaCalibrarTouch`)

- Se muestra 4 cruces sucesivas en las esquinas (con margen 20 px).
- El usuario toca cada una.
- Se guarda `{min_x, max_x, min_y, max_y}` y `touch_calibrado = true` en `Config`.
- Al primer arranque tras migrar (o si `touch_calibrado == false`), el `GestorPantallas` fuerza esta pantalla antes de mostrar el menú.

## Interacción táctil

### Detección de gestos en `Touch::taskPoll`

- Poll cada 50 ms del XPT2046 (`ts.tirqTouched()` como fast-path, luego `ts.touched()`).
- Cuando pasa de "no tocado" a "tocado", guarda `(x0, y0, ms0)`.
- Cuando pasa de "tocado" a "no tocado":
  - Si `dur < 300 ms` y `sqrt(dx² + dy²) < 20` → emite `TAP` con `(x0, y0)`.
  - Si `|dx| > 60` y `|dx| > |dy|` y `dur < 400 ms` → emite `SWIPE_L` o `SWIPE_R` según signo de `dx`.
  - Si no cae en ningún caso, se ignora.
- Coordenadas transformadas al espacio de pantalla usando la calibración (`Config::touch_min_x/max_x/min_y/max_y` con la rotación landscape del CYD). Si `touch_calibrado == false`, se usan constantes por defecto tentativas para permitir que el usuario llegue al menú y ejecute `Calibrar táctil`.

### Consumo de eventos en `GestorPantallas::tick`

- Vacía la cola en cada tick.
- Si el evento es `TAP`:
  - Si cae en la zona del botón "☰" → `volverAlMenu()`.
  - Si no, pasa el tap a la pantalla activa: `activa->alTocar(x, y - 20)` (coordenadas relativas al área de contenido).
- Si el evento es `SWIPE_L|R` y hay pila UI abierta (menú/sub-menú) → se ignora, para no confundir al usuario.
- Si el evento es `SWIPE_L|R` y estamos en el carrusel → `siguienteEnCarrusel()` o `anteriorEnCarrusel()`, y se resetea el temporizador de rotación.
- Si el evento es `SWIPE_L|R` en modo FIJO → se ignora.

## Reconfigurar localización con QR (respuesta al planteamiento del usuario)

La pregunta del usuario es si tiene sentido mostrar un QR desde el menú para reconfigurar la localización sin volver al modo portal. La respuesta es que sí, y aquí hay dos escenarios distintos:

- **Cambiar sólo la localización (dirección y radio)**: la placa mantiene su conexión WiFi de casa y muestra el QR con `http://<ip-lan>/config`. El móvil, que está en la misma WiFi, escanea, abre `config.html` (ya existe en `data/config.html`), edita, envía a `POST /api/config` (endpoint que ya existe en `lib/web_server`), y la placa reinicia con la nueva configuración. No hace falta reemparejamiento.
- **Cambiar la red WiFi o empezar de cero**: opción "Reset total" del menú. Borra la config con `ConfigStore::borrar()` y reinicia. En el próximo arranque, `main.cpp` no encuentra config, entra en modo portal, levanta el AP `RadarVuelos-XXXX`, y `wifi_portal.cpp` sigue su flujo actual con el QR del AP.

De este modo, el escenario común (cambiar la dirección desde el sofá) queda a un tap y un escaneo, sin dependencias del AP; el escenario disruptivo (cambiar de red WiFi) mantiene el flujo actual sin cambios.

## Testing

Se cubren en `test/native` (Unity, sin ArduinoFake, como el resto de tests del proyecto):

### `test_gestor_pantallas`

Con un `PantallaFake` que implementa la interfaz y expone contadores (`nEntradas`, `nSalidas`, `nDibujos`, `ultimoTap`):

- Registrar 3 fakes, modo CARRUSEL con intervalo 10 s, avanzar un reloj fake → cambia de vista cada 10 s.
- Swipe manual adelanta la vista y resetea el temporizador (no cambia otra vez a los pocos ms).
- Modo FIJO ignora swipe y timer, sólo la vista fija recibe `dibujar`.
- `abrirEnPila(sub)` + `volverAtras()` restauran la vista anterior; el timer del carrusel se pausa mientras la pila UI está abierta.
- Tap en zona de botón ☰ desde una vista de contenido → vuelve a `PantallaMenu` y vacía la pila.

### `test_config_store_v2`

- Serializar un `Config` v2 completo → deserializar → coincide en todos los campos, v1 y v2.
- Deserializar un buffer v1 conocido (contenido reproducible en el test) → devuelve `Config` con campos v1 correctos y campos v2 en su valor por defecto.
- Buffer con `magic` incorrecto → false.
- Buffer con `version = 99` → false.
- Buffer truncado en cualquier posición → false, sin escribir en `out`.

### Manual en placa

- Calibración táctil funciona en las 4 esquinas.
- Swipe cambia de vista con firmeza (no saltos accidentales por tap).
- Botón "☰" tiene margen suficiente para tap con el dedo (40×20 px como mínimo).
- QR de la LAN se lee con el móvil desde ~30 cm.

## Riesgos y decisiones asumidas

- **Heap justo**: el sprite 240×240 8bpp del radar (57 KB) más WiFi + AsyncWebServer + LittleFS deja poco margen para sprites adicionales. Decisión: sólo `PantallaRadar` mantiene sprite propio, el resto de vistas dibujan directamente en el TFT. Si en placa se ve necesario, `alSalir()` de `PantallaRadar` libera el sprite y `alEntrar()` lo recrea.
- **Bus SPI del touch**: en la CYD (`ESP32-2432S028R`) el XPT2046 va en pines `T_CS=33, T_CLK=25, T_MOSI=32, T_MISO=39, T_IRQ=36`, distintos del TFT. Se declaran como build flags en `platformio.ini`. Si la placa concreta es una variante con pines distintos se recalibra en la lib `touch`.
- **Primer arranque tras migración**: `touch_calibrado == false` fuerza `PantallaCalibrarTouch` antes del menú.
- **Radar sigue funcionando durante navegación**: el poller de ADSB (task `poller`) es independiente del display; sigue actualizando el snapshot aunque el usuario esté en otro menú. Al volver a `PantallaRadar`, los datos ya están frescos.
- **Sin animaciones de transición** entre pantallas en este bloque. Corte seco. Se puede añadir en un bloque posterior si merece la pena.
- **Nombres**: `Pantalla*` como sufijo de clase y directorio `lib/pantalla_*/` para coherencia con `display_radar → pantalla_radar`.
- **No se altera `wifi_portal` funcionalmente**: sigue mostrando su QR del AP en el arranque en frío sin config. La única diferencia es que ahora usa `qr_view` como implementación compartida.
- **Splash de arranque**: el mensaje "Radar de vuelos / arrancando…" que hoy pinta `main.cpp` antes de cargar la config sigue igual y **no** pasa por el `GestorPantallas`. Se pinta con `tft_driver` directamente antes de que exista la arquitectura de pantallas. Sólo cuando la config está cargada y se decide "modo Radar" en vez de "modo Portal", `main.cpp` construye `GestorPantallas`, registra las pantallas, arranca la task de touch y cede el control.

## Fuera de alcance / trabajos siguientes

Los siguientes bloques tendrán su propio spec en `docs/superpowers/specs/` (fecha en el momento de arrancarlos):

- **Bloque 2 — Pantalla Reloj**: NTP con `configTime()`, zona horaria configurable, layout numérico grande + fecha.
- **Bloque 3 — Pantalla Meteo**: cliente Open-Meteo (HTTPS sin API key), TTL 30 min, layout actual + previsión próximas horas.
- **Bloque 4 — Pantalla Fútbol Liga española**: selección de proveedor entre `football-data.org`, `TheSportsDB` o `API-Football`, layout resultados + calendario, TTL diferenciado por endpoint.
- **Bloque 5 — Pantalla MotoGP**: proveedor incierto (TheSportsDB como primera opción), calendario + últimos resultados.
- **Bloque 6 — Pantalla F1**: Jolpica-F1 (sucesor de Ergast), calendario + últimos resultados.

Fondo común de estos bloques: un `CacheDatos` con TTLs por endpoint y una única task de refresco en background, para no depender del cambio de pantalla ni tirar el heap. Este diseño se elaborará en el spec del primer bloque que lo necesite (probablemente Bloque 3).
