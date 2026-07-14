# Radar de vuelos — ESP32 (v1)

**Fecha:** 2026-07-14
**Autor:** Alberto del Pozo
**Estado:** Diseño aprobado, pendiente de plan de implementación

---

## 1. Contexto y objetivo

Firmware para una placa **ESP32-D0WD-V3 rev 3.1 (4 MB flash, WiFi + BT)** que actúa como radar casero de vuelos: consulta periódicamente aviones cercanos a un punto configurado y los expone en una pequeña interfaz web servida por la propia placa.

Objetivo v1: mínimo viable — cero hardware extra (solo la placa y USB), configuración por primer arranque, interfaz solo web accesible desde cualquier dispositivo en la LAN.

Hardware previamente descrito:
- Placa ESP32 dev con USB CH340 (`/dev/cu.usbserial-120`, MAC `6c:c8:40:44:a5:d8`).
- Backup del firmware original en `backup_flash_original_4MB.bin` (WiFiManager + app desconocida basada en Arduino/ESP-IDF v4.4.6).

Fuera de v1 (posibles v2): pantalla OLED, tira de LEDs WS2812, buzzer, histórico persistido, OTA, notificaciones push, MLAT con receptor local.

---

## 2. Requisitos funcionales

1. **Primer arranque (sin config guardada):** la placa levanta un access point `RadarVuelos-XXXX` (los 4 últimos dígitos de la MAC) y sirve un portal web con formulario para introducir SSID, password, dirección postal y radio en km.
2. **Geocoding:** al guardar el formulario, la placa resuelve la dirección postal a lat/lon usando la API pública de Nominatim (OpenStreetMap). Si la resolución falla, el formulario responde con error y **no** guarda nada.
3. **Persistencia:** la config completa (SSID, password, dirección, lat, lon, radio_km) se guarda en NVS del ESP32 y sobrevive a reinicios y cortes de corriente.
4. **Arranque normal:** con config guardada, la placa se conecta al WiFi. Si conecta en < 20 s, entra en **Modo Radar**. Si no, vuelve al portal para permitir corregir credenciales.
5. **Polling de datos:** en Modo Radar, un task hace `GET https://api.adsb.lol/v2/point/{lat}/{lon}/{radio_km}` cada 3 s y actualiza el snapshot en memoria.
6. **Interfaz web (Modo Radar):**
   - `GET /` → radar polar (canvas) con los aviones + tabla lateral (callsign, altitud, distancia, rumbo).
   - `GET /api/aircraft` → JSON con el snapshot actual (usado por el frontend con fetch cada 2 s).
   - `GET /config` → formulario de reconfiguración con valores actuales precargados.
   - `POST /api/config` → guarda config nueva y reinicia (~3 s downtime).
   - `POST /api/reset` → borra NVS y reinicia (vuelve al portal).
7. **LED de estado** (GPIO 2 interno):
   - Parpadeo 1 Hz → Portal esperando cliente.
   - Parpadeo 5 Hz → conectando WiFi.
   - Fijo → Modo Radar operativo.
   - Apagado 2 s cada minuto → error de red/API (snapshot marcado como `stale`).
8. **Recuperación automática:** si el WiFi cae y no reconecta en 60 s, la placa se reinicia.

---

## 3. Requisitos no funcionales

- **Framework:** Arduino sobre ESP32 (PlatformIO, `platform = espressif32`, `board = esp32dev`).
- **Filesystem:** LittleFS para servir `index.html`, `config.html`, `app.js`, `style.css` desde `data/`. Actualización con `pio run -t uploadfs` sin necesidad de re-flashear el firmware.
- **Web server:** `ESPAsyncWebServer` — mejor comportamiento con varios clientes refrescando cada 2 s.
- **JSON:** `ArduinoJson` v7 con parseo en streaming para evitar picos de RAM.
- **Idioma:** todo (código, comentarios, UI, commits) en español.
- **Sin OTA, sin BLE, sin autenticación** en la UI web — solo LAN doméstica.
- **Sin logging persistente** — Serial 115200 baud para debug puntual.
- **Snapshot en RAM se pierde al reiniciar** — se refresca en 3 s, no compensa persistir.

---

## 4. Arquitectura

Dos modos de operación excluyentes elegidos en boot:

- **Modo Portal (AP):** sin config o si el WiFi guardado no conecta tras 20 s. Levanta AP + formulario. Al guardar con éxito → reboot.
- **Modo Radar (STA):** con config válida. Conecta a WiFi de casa, arranca web server + poller.

Aprovecha los dos núcleos del ESP32:
- **Core 0** → task `poller` (fetch ADSB.lol cada 3 s, actualiza `RadarState`).
- **Core 1** → task `web` (loop de Arduino / ESPAsyncWebServer, responde requests).

`RadarState` es la estructura compartida entre ambos cores, protegida con mutex (`SemaphoreHandle_t`).

---

## 5. Componentes

### 5.1 Layout del proyecto

```
src/
├── main.cpp              # orquestador: elige modo y arranca
├── config_store.{h,cpp}  # read/write config en NVS
├── wifi_portal.{h,cpp}   # AP + formulario + delega geocoding
├── geocoder.{h,cpp}      # Nominatim: dirección → lat/lon
├── adsb_client.{h,cpp}   # GET ADSB.lol + parseo JSON
├── radar_state.{h,cpp}   # snapshot en RAM (thread-safe)
├── web_server.{h,cpp}    # rutas HTTP en Modo Radar
├── status_led.{h,cpp}    # patrones del LED de estado
├── geo_math.{h,cpp}      # haversine (distancia y bearing)
└── http_client.{h,cpp}   # abstracción IHttpClient (real + mock para tests)

include/                  # sólo si aparece algo compartido con lib/

data/                     # subir con `pio run -t uploadfs`
├── index.html
├── config.html
├── app.js                # radar polar + fetch loop
└── style.css

test/
├── test_geocoder/
├── test_adsb_parser/
├── test_haversine/
└── test_config_store/

docs/superpowers/specs/
└── 2026-07-14-radar-vuelos-esp32-design.md   # este documento
```

### 5.2 Interfaces públicas (resumen)

- **`ConfigStore`**
  - `bool load(Config& out)` — false si no hay config o está corrupta.
  - `bool save(const Config& cfg)`
  - `void clear()`
- **`Config`** (struct)
  ```cpp
  struct Config {
    String ssid;
    String password;
    String direccion;
    double lat;
    double lon;
    int    radio_km;    // default 25
  };
  ```
- **`WifiPortal`**
  - `void run()` — bloquea hasta que el usuario guarda config con geocoding exitoso; al terminar hace `ESP.restart()`.
- **`Geocoder`**
  - `bool resolve(const String& direccion, double& lat, double& lon)` — usa `IHttpClient` inyectado, timeout 5 s.
- **`AdsbClient`**
  - `bool fetchNearby(double lat, double lon, int radio_km, std::vector<Aircraft>& out)`.
- **`Aircraft`** (struct)
  ```cpp
  struct Aircraft {
    String hex;
    String callsign;
    double lat;
    double lon;
    int    alt_ft;
    int    gs_kt;      // ground speed
    int    track_deg;
    double dist_km;    // calculado por la ESP32
    int    bearing;    // calculado por la ESP32
  };
  ```
- **`RadarState`**
  - `void update(const std::vector<Aircraft>& aircraft)` — actualiza snapshot y `ts`.
  - `void markStale()` — mantiene el último snapshot pero marca `stale=true`.
  - `Snapshot snapshot()` — copia bajo mutex.
- **`WebServer`**
  - `void begin(const Config& cfg, RadarState& state)` — monta rutas y arranca.
- **`StatusLed`**
  - `enum class State { PORTAL, CONNECTING_WIFI, RADAR_OK, RADAR_ERROR }`
  - `void begin()`, `void setState(State)`.
- **`IHttpClient`** (interfaz)
  - `bool get(const String& url, String& body, int timeout_ms)`.
  - Impl real: `WifiHttpClient`. Impl test: `MockHttpClient` (devuelve strings predefinidos).

---

## 6. Flujo de datos

### 6.1 Boot

```
boot
 └─ ConfigStore.load(cfg)
     ├─ ok    → WiFi.begin(cfg.ssid, cfg.password), esperar 20 s
     │          ├─ conectado → Modo Radar
     │          └─ timeout   → Modo Portal
     └─ vacío → Modo Portal
```

### 6.2 Modo Portal

```
WiFi.softAP("RadarVuelos-XXXX")
StatusLed.setState(PORTAL)
WebServer sirve /portal.html
 └─ POST /save (ssid, password, direccion, radio_km)
     └─ Geocoder.resolve(direccion) → lat, lon
         ├─ ok    → ConfigStore.save(cfg) → respuesta OK → ESP.restart()
         └─ fallo → 400 "dirección no encontrada, prueba a añadir ciudad/país"
                    (config NO se guarda)
```

### 6.3 Modo Radar — loop de datos

```
task poller (core 0, cada 3 s)
 └─ AdsbClient.fetchNearby(cfg.lat, cfg.lon, cfg.radio_km)
     ├─ ok    → RadarState.update(aircraft[])  → StatusLed → RADAR_OK
     └─ fallo → RadarState.markStale()          → StatusLed → RADAR_ERROR

task web (core 1, on-demand por request)
 └─ GET /api/aircraft
     └─ RadarState.snapshot() → JSON serializado
```

### 6.4 Frontend (`app.js`)

```
window.onload
 └─ dibujar canvas base (círculos de referencia a 5/10/15/20/25 km)
 └─ setInterval(2000) → fetch('/api/aircraft')
                      → dibujar aviones (posición polar) + tabla
                      → si stale=true, tint gris + banner "sin conexión"
```

### 6.5 Formato JSON — `/api/aircraft`

```json
{
  "center":   { "lat": 40.42, "lon": -3.70 },
  "radio_km": 25,
  "ts":       1234567890,
  "stale":    false,
  "aircraft": [
    {
      "hex":     "4b1806",
      "cs":      "IBE3456",
      "lat":     40.45,
      "lon":     -3.68,
      "alt_ft":  8500,
      "gs":      320,
      "trk":     85,
      "dist_km": 3.4,
      "bearing": 45
    }
  ]
}
```

La ESP32 calcula `dist_km` y `bearing` (haversine) — el frontend no hace trigonometría.

### 6.6 Reconfiguración en caliente

```
GET /config          → sirve config.html (LittleFS), precargado
POST /api/config     → valida
                       ├─ si cambia la dirección → Geocoder.resolve()
                       │   └─ si falla → 400 "no encontrado" (no guarda)
                       ├─ ConfigStore.save(cfg_nueva)
                       └─ ESP.restart()   (~3 s de downtime)
POST /api/reset      → ConfigStore.clear() → ESP.restart() → Portal
```

Cualquier cambio dispara reboot — simplifica el código y en un radar casero 3 s no importan.

---

## 7. Manejo de errores

### 7.1 Config corrupta

`ConfigStore.load()` valida magic byte + longitudes máximas de strings. Si detecta corrupción → tratar como "sin config" → Modo Portal.

### 7.2 WiFi caído (Modo Radar)

- `WiFi.onEvent(SYSTEM_EVENT_STA_DISCONNECTED)` → `StatusLed` en error, el poller sigue intentando (fallará → snapshot stale).
- `WiFi.setAutoReconnect(true)` para reconexión automática de la stack.
- Contador interno: si tras 60 s sigue sin conectar → `ESP.restart()` — más simple y limpio que gestionar reintentos con backoff.

### 7.3 ADSB.lol devuelve error / timeout

- Timeout HTTP a 5 s.
- 1 reintento inmediato; si falla otra vez → `RadarState.markStale()`, LED error. Siguiente polling en 3 s (sin backoff — la API aguanta y el ritmo es sano).

### 7.4 Geocoding (Nominatim) falla

- **En Portal:** `POST /save` responde 400 con mensaje "dirección no encontrada, prueba a añadir ciudad/país". La config **no** se guarda.
- **En `/config`:** mismo tratamiento — se rechaza el POST y la config antigua se mantiene intacta.

### 7.5 JSON malformado

`ArduinoJson` con `DeserializationError` → tratar como fallo de fetch (stale + LED error). Nunca parsear a medias.

### 7.6 Snapshot con demasiados aviones

Límite duro de **50 aviones** en el snapshot (protege RAM). Si vienen más:
- Ordenar por `dist_km` ascendente.
- Descartar los más lejanos hasta llegar a 50.
- Log por Serial: `"[WARN] snapshot truncado: N aviones recibidos, 50 conservados"`.

### 7.7 Watchdog

- Task Watchdog Timer activo en el poller con 15 s. Si el fetch se cuelga (por ejemplo, cliente HTTP mal cerrado) → reboot automático.

### 7.8 Sin logging persistente

Todo por Serial 115200. Si en el futuro se necesita logging remoto, se añade Syslog UDP sin tocar el resto de módulos.

---

## 8. Testing

Testing pragmático, sin CI. Foco en las partes que pueden romperse en silencio (parsers y matemáticas).

### 8.1 Tests unitarios (Unity + PlatformIO `test/`, ejecutados en `native`)

- **`test_geocoder`** — parseo de respuestas típicas de Nominatim (con resultado, vacía, malformada) usando `MockHttpClient`.
- **`test_adsb_parser`** — parseo de respuestas de `/v2/point/...` con casos: normal, vacía, campos ausentes, malformada, > 50 aviones (verifica truncado).
- **`test_haversine`** — distancia y bearing con casos conocidos (Madrid → Barcelona ≈ 505 km, N/S/E/W puros, punto sobre sí mismo).
- **`test_config_store`** — serialización/deserialización de `Config`, detección de corrupción por magic byte, comportamiento en NVS vacía.

Los HTTP se abstraen tras la interfaz `IHttpClient` para permitir el mock sin placa.

### 8.2 Validación manual end-to-end (checklist antes de "dar por hecho")

1. Flash en placa vacía → arranca en Portal (LED lento, AP visible).
2. Conectar al AP `RadarVuelos-XXXX`, formulario, meter dirección real → geocoding OK → reboot.
3. Placa arranca en Modo Radar (LED fijo), conecta a WiFi de casa.
4. Abrir `http://<ip-esp>/` desde el móvil → radar polar y tabla pobladas en < 5 s.
5. `/config` con dirección nueva → reboot → nuevas coords activas.
6. Apagar el router 60 s → LED error → auto-reboot.
7. Portal con dirección inventada → geocoding falla → mensaje, NO guarda.
8. `POST /api/reset` desde curl → borra NVS → vuelve al portal.

### 8.3 Sin CI, sin cobertura, sin tests de JS

Para el volumen y contexto (proyecto casero de una persona) no compensa. Se añade si el proyecto crece.

---

## 9. Dependencias (PlatformIO)

`platformio.ini` a completar durante implementación con:

```ini
[env:esp32dev]
platform    = espressif32
board       = esp32dev
framework   = arduino
monitor_speed = 115200
board_build.filesystem = littlefs
lib_deps =
  bblanchon/ArduinoJson@^7.0.0
  esphome/ESPAsyncWebServer-esphome@^3.2.0
  esphome/AsyncTCP-esphome@^2.1.0
```

(Nota: las versiones son placeholders razonables — el plan de implementación fijará las exactas.)

---

## 10. Fuera de alcance (v1)

- Pantalla OLED / TFT.
- Tira de LEDs WS2812.
- Buzzer.
- Persistencia de histórico de vuelos.
- OTA (updates por red).
- Notificaciones push.
- Autenticación de la UI web.
- Filtrado por altitud / tipo de aeronave.
- Modo AP simultáneo con STA (dual mode).
- Receptor SDR local.
- Múltiples ubicaciones simultáneas.

---

## 11. Riesgos y mitigaciones

| Riesgo | Mitigación |
|---|---|
| ADSB.lol cambia el endpoint o schema | `IHttpClient` + parser aislado permite adaptar rápido y testear sin placa. |
| Nominatim rate-limits (uso educado: 1 request/s máx.) | Solo se llama al guardar config (uso puntual), no en el hot loop. |
| RAM insuficiente en zonas con mucho tráfico | Límite duro de 50 aviones + JSON en streaming con ArduinoJson. |
| Config guardada con WiFi que ya no existe | Timeout 20 s en conexión + fallback automático al Portal. |
| Snapshot stale sin que el usuario se dé cuenta | Frontend renderiza en gris + banner "sin conexión" cuando `stale=true`. |

---

## 12. Referencias

- ADSB.lol API: https://api.adsb.lol/docs
- Nominatim (OpenStreetMap): https://nominatim.org/release-docs/latest/api/Search/
- ESP-IDF NVS: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
- ESPAsyncWebServer: https://github.com/esphome/ESPAsyncWebServer
- ArduinoJson v7: https://arduinojson.org/v7/

---

**Backup del firmware original:** `backup_flash_original_4MB.bin` (SHA-256 `df42848a3d29b307f8a8573b376bd792b9030c02885e643b9a78ee0e26a31f24`) en la raíz del proyecto. Restauración:

```
~/.platformio/penv/bin/python ~/.platformio/packages/tool-esptoolpy/esptool.py \
  --chip esp32 --port /dev/cu.usbserial-120 --baud 460800 \
  write_flash 0x0 backup_flash_original_4MB.bin
```
