# Plan — Pantalla Fútbol Liga (Bloque 4)

**Spec:** `2026-09-04-pantalla-futbol-design.md`

## Task 1: `lib/futbol_client/` + tests native

**Files:** create `lib/futbol_client/library.json`, `lib/futbol_client/src/futbol_client.h`, `lib/futbol_client/src/futbol_client.cpp`, `test/test_futbol_client/test_main.cpp`.

- [ ] TDD: escribir 4 tests, verificar rojo, implementar, verde, commit.

**Test payload** (pegado en el test_main):
```
{"events":[
  {"strHomeTeam":"Real Madrid","strAwayTeam":"Barcelona","intHomeScore":"2","intAwayScore":"1","dateEvent":"2026-09-01","strTime":"20:00:00"},
  {"strHomeTeam":"Atlético","strAwayTeam":"Sevilla","intHomeScore":"0","intAwayScore":"0","dateEvent":"2026-09-02","strTime":"18:30:00"}
]}
```

**Commit:** `feat(futbol_client): cliente TheSportsDB para La Liga con parser puro`

## Task 2: `lib/pantalla_futbol/`

**Files:** create `lib/pantalla_futbol/{library.json,src/pantalla_futbol.h,src/pantalla_futbol.cpp}`.

- Sub-vistas `ULTIMOS`/`PROXIMOS` alternables por swipe vertical.
- Fondo dark. Título arriba. Filas de partido: LOCAL RESULTADO VISITANTE (últimos) o LOCAL VISITANTE + fecha (próximos).
- Truncar nombres a 12 chars con `substr(0,11)+"."` si sobrepasan.

**Commit:** `feat(pantalla_futbol): vista Liga con sub-vistas últimos y próximos`

## Task 3: `main.cpp` — task refresh + swap

**Files:** modify `src/main.cpp`.

- Include `pantalla_futbol.h` + `futbol_client.h`.
- Global `FutbolSnapshot g_snapFutbol;`.
- Task `tareaFutbolRefresh` cada 3 h (con gracia 10 s inicial).
- Swap `PantallaProximamente(3, "Futbol")` → `PantallaFutbol(g_snapFutbol)`.

**Commit:** `feat(main): task refresh fútbol + activar PantallaFutbol`

## Task 4: Compile-only (autónomo)

- `pio run -e esp32dev` → `[SUCCESS]`. Reportar Flash %.
- `pio test -e native` → total 58/58 (54 previos + 4 nuevos).
- Sin upload; verificación humana pospuesta.
