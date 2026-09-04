# Pantalla MotoGP — diseño

- **Fecha:** 2026-09-04
- **Bloque:** 5 de 6 (MotoGP)
- **Estado:** aprobado en modo autónomo

## Contexto

Sustituye el placeholder MotoGP por una vista real. Datos de **TheSportsDB** (mismo proveedor que Fútbol) usando el league id **4407** (MotoGP). Estructura calcada al bloque 4 (Fútbol) para maximizar reuso y minimizar riesgo. Aprovecha `paleta_dark` y `swipe vertical` ya en el proyecto.

## Objetivos

- `PantallaMotogp` con dos sub-vistas alternables por swipe vertical:
  - **Últimas carreras** (hasta 5 GPs recientes con ganador si TheSportsDB lo trae, o sólo nombre/fecha).
  - **Calendario** (hasta 5 GPs próximos con fecha).
- Cliente `MotogpClient` que llama a `eventspastleague.php?id=4407` y `eventsnextleague.php?id=4407`.
- Fallo elegante ("MotoGP: sin datos").
- Refresh cada **6 h** (los eventos MotoGP no cambian a la hora).

## No-objetivos

- Clasificación de pilotos, tiempos por vuelta, información de circuito detallada.
- Live timing.
- Detalles de otras clases (Moto2, Moto3).

## Arquitectura

TheSportsDB no siempre devuelve resultados numéricos para MotoGP (a veces sólo `strResult` con texto). La estructura del cliente es simétrica al fútbol pero interpreta menos campos:

```cpp
struct EventoMotor {
  std::string nombre;       // strEvent o strFilename
  std::string fechaHora;    // "YYYY-MM-DD HH:MM"
  std::string ganador;      // strHomeTeam si viene, o "" si aún sin resultado
  std::string resultado;    // strResult (texto libre) o "" si no aplica
};

struct MotogpSnapshot {
  bool ok = false;
  uint32_t obtenido_ms = 0;
  bool stale = false;
  std::vector<EventoMotor> ultimos;
  std::vector<EventoMotor> proximos;
};

class MotogpClient {
 public:
  explicit MotogpClient(IHttpClient& http) : http_(http) {}
  bool fetch(MotogpSnapshot& out);
  static bool parsearEventos(const std::string& json,
                             std::vector<EventoMotor>& out, size_t maxN);
 private:
  IHttpClient& http_;
};
```

Endpoints:
- `http://www.thesportsdb.com/api/v1/json/3/eventspastleague.php?id=4407`
- `http://www.thesportsdb.com/api/v1/json/3/eventsnextleague.php?id=4407`

Campos JSON que consumimos por evento:
- `strEvent` — nombre visible ("Spanish Grand Prix", etc.). Si null, `strFilename`.
- `dateEvent` + `strTime` — fecha/hora local.
- `strHomeTeam` — algunos endpoints ponen el ganador aquí (verificar en placa).
- `strResult` — texto libre con clasificación si existe.

### `lib/pantalla_motogp/`

Estructura calcada a `PantallaFutbol`:
- `id() -> 4`, `nombre() -> "MotoGP"`.
- `SubVista::ULTIMOS | CALENDARIO`.
- Sub-vista **Últimos**: título "Ultimas carreras" + 4 filas: nombre GP truncado + ganador debajo (o "-").
- Sub-vista **Calendario**: título "Calendario" + 5 filas: nombre GP + fecha/hora en gris.
- Indicador dos dots arriba-derecha.
- Swipe vertical alterna.

### `src/main.cpp`

- Include `pantalla_motogp.h`, `motogp_client.h`.
- Global `MotogpSnapshot g_snapMotogp;`.
- Task `tareaMotogpRefresh` core 0, prio 1, stack 8192 B, gracia 15 s, refresh 6 h.
- Swap `PantallaProximamente(4, "MotoGP")` → `PantallaMotogp(g_snapMotogp)`.

## Testing

`test/test_motogp_client/test_main.cpp` con payload TheSportsDB (formato conocido de eventos). 3-4 tests para parsear, respetar maxN, rechazar JSON malformado.

## Riesgos

- **TheSportsDB MotoGP puede tener metadata limitada**: pilotos ganadores tal vez no vengan en el endpoint gratuito. Fallback: mostrar sólo nombres y fechas — funcional aunque menos rico.
- **Flash 89.3%**: sube previsiblemente a ~90-91%. Vigilar.
