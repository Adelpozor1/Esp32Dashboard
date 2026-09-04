# Pantalla F1 (Jolpica) — diseño

- **Fecha:** 2026-09-04
- **Bloque:** 6 de 6 (F1)

## Contexto

Último bloque de la iniciativa carrusel. Sustituye el placeholder F1 por una vista real usando **Jolpica-F1** (sucesor de la extinta Ergast API), gratis y sin API key, con JSON limpio.

## Objetivos

- `PantallaF1` con dos sub-vistas alternables por swipe vertical:
  - **Última carrera** (top 3 del último GP terminado).
  - **Calendario** (próxima carrera + siguientes 4).
- Cliente `F1Client` que llama a `/current/last/results` y `/current/next` / calendar.
- Refresh cada 6 h.

## Arquitectura

### `lib/f1_client/`

Jolpica devuelve JSON MRData con estructura anidada. Simplificamos a dos snapshots.

```cpp
struct F1Piloto {
  int         posicion;
  std::string nombre;      // "Verstappen"
  std::string equipo;      // "Red Bull"
  std::string tiempo;      // "1:32:35.045" o "+5.123" o "" si no acabó
};

struct F1Carrera {
  int         ronda = 0;
  std::string nombreGp;
  std::string circuito;
  std::string fechaHora;   // "YYYY-MM-DD HH:MM"
};

struct F1Snapshot {
  bool ok = false;
  uint32_t obtenido_ms = 0;
  bool stale = false;
  // Última carrera con resultados (top 3):
  F1Carrera ultima;
  std::vector<F1Piloto> podio;
  // Calendario (próxima + siguientes 4):
  std::vector<F1Carrera> proximas;
};

class F1Client {
 public:
  explicit F1Client(IHttpClient& http) : http_(http) {}
  bool fetch(F1Snapshot& out);
  static bool parsearUltima(const std::string& json, F1Carrera& carrera,
                            std::vector<F1Piloto>& podio);
  static bool parsearCalendario(const std::string& json,
                                std::vector<F1Carrera>& proximas, size_t maxN);
 private:
  IHttpClient& http_;
};
```

Endpoints:
- Última con resultados: `http://api.jolpi.ca/ergast/f1/current/last/results.json`
- Calendario actual: `http://api.jolpi.ca/ergast/f1/current.json`

Del JSON de Jolpica (formato Ergast):
- `MRData.RaceTable.Races[0]` — última con `Results` (podium tomando los 3 primeros).
- Cada `Race` tiene `raceName`, `Circuit.circuitName`, `date` (`YYYY-MM-DD`), `time` (`HH:MM:SS[.Z]`), `round`.
- Cada `Result` tiene `position`, `Driver.familyName`, `Constructor.name`, `Time.time` (para 1º) o `Time.time` con `+delta` para los demás. Si no acabó, `status="DNF"` y sin `Time`.

Filtrar calendario a próximas 5 comparando `date` con la fecha actual (RTC del ESP32 vía `time()`).

### `lib/pantalla_f1/`

Estructura idéntica a fútbol/motogp. Sub-vistas:
- **Última**: título "Ultima carrera" + nombre GP + circuito en gris + 3 filas del podio.
- **Calendario**: título "Calendario" + hasta 5 filas GP + circuito + fecha.

### `src/main.cpp`

- Global `F1Snapshot g_snapF1;`.
- Task `tareaF1Refresh` (core 0, prio 1, stack 10240 B — Jolpica es más pesada). Gracia 20 s, refresh 6 h.
- Swap `PantallaProximamente(5, "F1")` → `PantallaF1(g_snapF1)`.

## Riesgos

- **Flash a 89.8%**. Jolpica es más denso. Estimación: +0.5-0.7 pp → ~90.5%. Aún dentro pero apretado.
- **HTTP puro a Jolpica**: verificado en docs que `http://api.jolpi.ca/ergast/f1/...` funciona sin redirect a HTTPS.
