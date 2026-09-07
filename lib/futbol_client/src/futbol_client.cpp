#include "futbol_client.h"
#include <ArduinoJson.h>
#include <cstring>
#include <ctime>
#include <cstdio>

#ifdef ARDUINO
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace {
inline void pausaHeap() {
#ifdef ARDUINO
  vTaskDelay(pdMS_TO_TICKS(1500));   // deja que el TLS libere ~35KB entre fetches
#endif
}
}

namespace {
constexpr const char* LIGA_ID = "4335";   // LaLiga en TheSportsDB (legacy live parser)
constexpr const char* CHAMPIONS_ID = "4480";

const char* idDe(Competicion c) {
  return (c == Competicion::CHAMPIONS) ? "4480" : "4335";
}

// Season por defecto si el past no la devuelve (para primer arranque).
const char* seasonPorDefecto(Competicion c) {
  (void)c;
  return "2026-2027";
}

std::string valOrEmpty(JsonVariantConst v) {
  if (v.isNull()) return "";
  const char* s = v.as<const char*>();
  return s ? std::string(s) : "";
}

int parseGoles(JsonVariantConst v) {
  if (v.isNull()) return -1;
  if (v.is<int>()) return v.as<int>();
  const char* s = v.as<const char*>();
  if (!s || !*s) return -1;
  return std::atoi(s);
}

int parseGolesLive(JsonVariantConst v) {
  int g = parseGoles(v);
  return g < 0 ? 0 : g;
}

std::string componerFechaHora(const std::string& fecha, const std::string& hora) {
  static const char* DIAS[7]  = {"Dom","Lun","Mar","Mie","Jue","Vie","Sab"};
  static const char* MESES[12] = {"ene","feb","mar","abr","may","jun",
                                   "jul","ago","sep","oct","nov","dic"};
  if (fecha.size() < 10) return "";
  int anio = std::atoi(fecha.substr(0, 4).c_str());
  int mes  = std::atoi(fecha.substr(5, 2).c_str());
  int dia  = std::atoi(fecha.substr(8, 2).c_str());
  if (anio < 2000 || mes < 1 || mes > 12 || dia < 1 || dia > 31) return "";
  struct tm tm = {};
  tm.tm_year = anio - 1900;
  tm.tm_mon  = mes - 1;
  tm.tm_mday = dia;
  tm.tm_hour = 12;   // mediodía para evitar líos DST
  mktime(&tm);
  char buf[32];
  const int wday = (tm.tm_wday >= 0 && tm.tm_wday < 7) ? tm.tm_wday : 0;
  const int mIdx = (tm.tm_mon >= 0 && tm.tm_mon < 12) ? tm.tm_mon : 0;
  if (hora.size() >= 5) {
    std::snprintf(buf, sizeof(buf), "%s %d %s %s",
                  DIAS[wday], dia, MESES[mIdx], hora.substr(0, 5).c_str());
  } else {
    std::snprintf(buf, sizeof(buf), "%s %d %s", DIAS[wday], dia, MESES[mIdx]);
  }
  return std::string(buf);
}

// Rellena un Partido "post-partido" desde un JsonVariantConst.
void rellenarPartido(JsonVariantConst it, Partido& p) {
  p.local          = valOrEmpty(it["strHomeTeam"]);
  p.visitante      = valOrEmpty(it["strAwayTeam"]);
  p.golesLocal     = parseGoles(it["intHomeScore"]);
  p.golesVisitante = parseGoles(it["intAwayScore"]);
  p.fechaHora      = componerFechaHora(valOrEmpty(it["dateEvent"]),
                                       valOrEmpty(it["strTime"]));
}

// Calcula "YYYY-MM-DD" para hoy y hoy+1 en la TZ actual del sistema.
void calcularHoyManana(std::string& hoyISO, std::string& mananaISO) {
  time_t ahora = time(nullptr);
  if (ahora < 1000000000) {   // NTP aún no listo
    hoyISO.clear();
    mananaISO.clear();
    return;
  }
  struct tm t = {};
  localtime_r(&ahora, &t);
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
                t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
  hoyISO = buf;

  time_t mananaEpoch = ahora + 24 * 3600;
  struct tm tm2 = {};
  localtime_r(&mananaEpoch, &tm2);
  std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
                tm2.tm_year + 1900, tm2.tm_mon + 1, tm2.tm_mday);
  mananaISO = buf;
}
}  // namespace

bool FutbolClient::parsearLive(const std::string& json, PartidoLive& out) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  auto ev = doc["events"];
  if (ev.isNull()) return false;
  JsonArrayConst arr = ev.as<JsonArrayConst>();
  if (arr.isNull()) return false;
  for (JsonVariantConst it : arr) {
    std::string idL = valOrEmpty(it["idLeague"]);
    if (idL != LIGA_ID) continue;
    out.local          = valOrEmpty(it["strHomeTeam"]);
    out.visitante      = valOrEmpty(it["strAwayTeam"]);
    out.golesLocal     = parseGolesLive(it["intHomeScore"]);
    out.golesVisitante = parseGolesLive(it["intAwayScore"]);
    out.progreso       = valOrEmpty(it["strProgress"]);
    return true;
  }
  return false;
}

bool FutbolClient::parsearUltimo(const std::string& json, Partido& out) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  auto ev = doc["events"];
  if (ev.isNull()) return false;
  JsonArrayConst arr = ev.as<JsonArrayConst>();
  if (arr.isNull() || arr.size() == 0) return false;
  rellenarPartido(arr[0], out);
  return true;
}

void FutbolClient::filtrarHoyManana(const std::string& json,
                                    std::vector<Partido>& out,
                                    const std::string& hoyISO,
                                    const std::string& mananaISO) {
  out.clear();
  if (hoyISO.empty() && mananaISO.empty()) return;
  JsonDocument doc;
  if (deserializeJson(doc, json)) return;
  auto ev = doc["events"];
  if (ev.isNull()) return;
  JsonArrayConst arr = ev.as<JsonArrayConst>();
  if (arr.isNull()) return;
  for (JsonVariantConst it : arr) {
    if (out.size() >= 5) break;
    std::string fecha = valOrEmpty(it["dateEvent"]);
    if (fecha.empty()) continue;
    if (fecha != hoyISO && fecha != mananaISO) continue;
    Partido p;
    rellenarPartido(it, p);
    out.push_back(p);
  }
}

bool FutbolClient::parsearJornada(const std::string& json,
                                  std::vector<Partido>& out) {
  out.clear();
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  auto ev = doc["events"].as<JsonArrayConst>();
  if (ev.isNull()) return false;
  for (JsonVariantConst it : ev) {
    Partido p;
    rellenarPartido(it, p);
    out.push_back(p);
  }
  return true;
}

bool FutbolClient::parsearUltimaRondaYSeason(const std::string& json,
                                             int& outRonda,
                                             std::string& outSeason) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  auto ev = doc["events"].as<JsonArrayConst>();
  if (ev.isNull() || ev.size() == 0) return false;
  auto e = ev[0];
  outRonda = std::atoi(valOrEmpty(e["intRound"]).c_str());
  outSeason = valOrEmpty(e["strSeason"]);
  return outRonda > 0 && !outSeason.empty();
}

bool FutbolClient::detectarChampionsActiva(const std::string& jsonNext) {
  JsonDocument doc;
  if (deserializeJson(doc, jsonNext)) return false;
  auto ev = doc["events"].as<JsonArrayConst>();
  return !ev.isNull() && ev.size() > 0;
}

bool FutbolClient::fetch(FutbolSnapshot& out) {
  out.competicion = competicion_;
  out.partidosJornada.clear();
  out.partidosSiguiente.clear();
  out.jornadaActual = 0;
  out.jornadaSiguiente = 0;
  // hayChampionsDisponible se calcula abajo; NO se resetea si estamos en
  // Champions (obvio: si estamos viéndolo, está disponible).
  out.hayChampionsDisponible = (competicion_ == Competicion::CHAMPIONS);

  const char* id = idDe(competicion_);
  std::string body; int status = 0;

  // 1) Past para averiguar última ronda + season.
  char urlPast[160];
  std::snprintf(urlPast, sizeof(urlPast),
    "https://www.thesportsdb.com/api/v1/json/3/eventspastleague.php?id=%s", id);
  int ultimaRonda = 0;
  std::string season = seasonPorDefecto(competicion_);
  if (http_.get(urlPast, body, status, 20000) && status == 200 && !body.empty()) {
    parsearUltimaRondaYSeason(body, ultimaRonda, season);
#ifdef ARDUINO
    ::Serial.printf("[futbol] past OK ronda=%d season=%s\n", ultimaRonda, season.c_str());
#endif
  } else {
#ifdef ARDUINO
    ::Serial.printf("[futbol] past fallo status=%d\n", status);
#endif
  }
  pausaHeap();

  // 2) Jornada actual = ronda de la última carrera jugada.
  if (ultimaRonda > 0 && !season.empty()) {
    char urlR[200];
    std::snprintf(urlR, sizeof(urlR),
      "https://www.thesportsdb.com/api/v1/json/3/eventsround.php?id=%s&r=%d&s=%s",
      id, ultimaRonda, season.c_str());
    body.clear(); status = 0;
    if (http_.get(urlR, body, status, 20000) && status == 200 && !body.empty()) {
      std::vector<Partido> pj;
      if (parsearJornada(body, pj)) {
        out.partidosJornada = std::move(pj);
        out.jornadaActual = ultimaRonda;
      }
    } else {
#ifdef ARDUINO
      ::Serial.printf("[futbol] ronda %d fallo status=%d\n", ultimaRonda, status);
#endif
    }
    pausaHeap();

    // 3) Próxima jornada = ronda + 1.
    std::snprintf(urlR, sizeof(urlR),
      "https://www.thesportsdb.com/api/v1/json/3/eventsround.php?id=%s&r=%d&s=%s",
      id, ultimaRonda + 1, season.c_str());
    body.clear(); status = 0;
    if (http_.get(urlR, body, status, 20000) && status == 200 && !body.empty()) {
      std::vector<Partido> ps;
      if (parsearJornada(body, ps)) {
        out.partidosSiguiente = std::move(ps);
        out.jornadaSiguiente = ultimaRonda + 1;
      }
    } else {
#ifdef ARDUINO
      ::Serial.printf("[futbol] ronda+1 %d fallo status=%d\n", ultimaRonda + 1, status);
#endif
    }
    pausaHeap();
  }

  // 4) ¿Champions activa? Sólo consultamos si NO estamos ya en Champions.
  if (competicion_ == Competicion::LALIGA) {
    body.clear(); status = 0;
    if (http_.get("https://www.thesportsdb.com/api/v1/json/3/eventsnextleague.php?id=4480",
                  body, status, 20000) && status == 200 && !body.empty()) {
      out.hayChampionsDisponible = detectarChampionsActiva(body);
    }
  }

  out.ok = !out.partidosJornada.empty() || !out.partidosSiguiente.empty();
  return out.ok;
}
