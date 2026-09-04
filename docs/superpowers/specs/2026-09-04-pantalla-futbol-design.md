# Pantalla Fútbol Liga española — diseño

- **Fecha:** 2026-09-04
- **Bloque:** 4 de 6 (Fútbol Liga española)
- **Estado:** aprobado en modo autónomo por Alberto ("continúa sin mí hasta el final")

## Contexto

Bloques 1-3 dejaron el carrusel táctil + reloj + meteo funcionando. Este bloque sustituye el placeholder Fútbol por una vista real con datos de La Liga (Primera División española) desde **TheSportsDB** (API pública gratuita sin key). Aprovecha la infra de swipe vertical del bloque 3 para dos sub-vistas: **Última jornada** (5 resultados jugados) y **Próxima jornada** (5 partidos programados con fecha/hora).

## Objetivos

- `PantallaFutbol` con dos sub-vistas alternables por swipe vertical.
- Cliente `FutbolClient` para TheSportsDB (`eventspastleague.php` y `eventsnextleague.php` con `id=4335` = La Liga). Parser JSON puro testable en native.
- Fallo elegante: si no hay datos, mensaje "Sin datos" centrado.
- Refresh cada 3 h en task dedicada de core 0.

## No-objetivos

- Clasificación de la liga, goleadores, estadísticas de jugadores.
- Otras competiciones (Copa del Rey, Champions).
- Notificaciones o alarmas.

## Arquitectura

### `lib/futbol_client/`

Sin dependencia del display. Depende de `IHttpClient` + ArduinoJson.

```cpp
struct Partido {
  std::string local;
  std::string visitante;
  std::string fechaHora;   // "2026-09-06 18:30" tras normalizar TheSportsDB
  int         golesLocal    = -1;   // -1 si no jugado
  int         golesVisitante = -1;
};

struct FutbolSnapshot {
  bool ok = false;
  uint32_t obtenido_ms = 0;
  bool stale = false;
  std::vector<Partido> ultimos;   // hasta 5 más recientes
  std::vector<Partido> proximos;  // hasta 5 siguientes
};

class FutbolClient {
 public:
  explicit FutbolClient(IHttpClient& http) : http_(http) {}
  bool fetch(FutbolSnapshot& out);
  static bool parsearEventos(const std::string& json,
                             std::vector<Partido>& out, size_t maxN);
 private:
  IHttpClient& http_;
};
```

Endpoints:
- Últimos: `http://www.thesportsdb.com/api/v1/json/3/eventspastleague.php?id=4335`
- Próximos: `http://www.thesportsdb.com/api/v1/json/3/eventsnextleague.php?id=4335`

Formato TheSportsDB (relevante):
- Array `events` con objetos que incluyen `strHomeTeam`, `strAwayTeam`, `intHomeScore`, `intAwayScore`, `dateEvent` (`YYYY-MM-DD`), `strTime` (`HH:MM:SS`).
- Cuando aún no se ha jugado, `intHomeScore` y `intAwayScore` son `null` o vienen sin campo.

Parser extrae hasta N eventos. Fecha/hora se compone como `YYYY-MM-DD HH:MM`.

### `lib/pantalla_futbol/`

Depende de `pantallas`, `tft_driver`, `paleta_dark`, `futbol_client`.

```cpp
class PantallaFutbol : public pantallas::Pantalla {
 public:
  explicit PantallaFutbol(const FutbolSnapshot& snapshot);
  const char* nombre() const override { return "Futbol"; }
  uint8_t id() const override { return 3; }
  // ...
};
```

Layout dark. Sub-vista **Última jornada**:
- Título "Ultima jornada" arriba en `COL_ACENTO`.
- Hasta 5 filas: `LOCAL 2-1 VISITANTE` en fila 22 px, nombres truncados a 12 chars, resultado centrado en negrita.

Sub-vista **Próxima jornada**:
- Título "Proxima jornada" arriba.
- 5 filas: `LOCAL   VISITANTE\nMar 6 sep 18:30` (dos líneas por partido = 30 px cada uno, sólo 4 partidos si no cabe).

Indicador de sub-vista (dos dots arriba-derecha) igual que meteo.

Swipe vertical alterna sub-vistas.

### `src/main.cpp`

- Include `pantalla_futbol.h`, `futbol_client.h`.
- Global `FutbolSnapshot g_snapFutbol;`.
- Task `tareaFutbolRefresh` (core 0, prio 1, stack 8192 B): gracia 10 s, luego cada 3 h `fetch()`.
- Swap `PantallaProximamente(3, "Futbol")` por `PantallaFutbol(g_snapFutbol)`.

## Testing

- `test/test_futbol_client/test_main.cpp` con payload TheSportsDB pegado:
  - `test_parsear_eventos_pasados_extrae_resultados`
  - `test_parsear_eventos_proximos_sin_resultados`
  - `test_parsear_json_malformado`
  - `test_parsear_respeta_maxN`

## Riesgos

- **TheSportsDB puede devolver menos de 5 partidos entre jornadas de liga**: aceptado; la vista dibuja los que haya.
- **Flash 88.8%** actual → sube previsiblemente a ~90%. Si aprieta se atacan mejoras en el bloque 6 (F1) o antes.
- **HTTP puro a TheSportsDB**: comprobado que responde 200 sin obligar HTTPS.
- **Sin verificación en placa este bloque** (modo autónomo); firmware queda compilado. El próximo arranque del user con la placa lo comprobará.
