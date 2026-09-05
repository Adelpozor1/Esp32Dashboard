#include "futbol_client.h"
#include <ArduinoJson.h>
#include <cstring>
#include <ctime>
#include <cstdio>

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace {
constexpr const char* LIGA_ID = "4335";   // LaLiga en TheSportsDB

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

bool FutbolClient::fetch(FutbolSnapshot& out) {
  const char* URL_LIVE = "https://www.thesportsdb.com/api/v1/json/3/livescore.php?s=Soccer";
  const char* URL_PAST = "https://www.thesportsdb.com/api/v1/json/3/eventspastleague.php?id=4335";
  const char* URL_NEXT = "https://www.thesportsdb.com/api/v1/json/3/eventsnextleague.php?id=4335";

  out.hayLive   = false;
  out.hayUltimo = false;
  out.hoyManana.clear();

  std::string body;
  int status = 0;

  // 1) Livescore. No es error crítico si falla, sólo que no habrá "live".
  if (http_.get(URL_LIVE, body, status, 20000) && status == 200 && !body.empty()) {
    PartidoLive pl;
    if (parsearLive(body, pl)) {
      out.live    = pl;
      out.hayLive = true;
    }
  } else {
#ifdef ARDUINO
    ::Serial.printf("[futbol] live fallo status=%d body=%u\n",
                    status, (unsigned)body.size());
#endif
  }

  // 2) Último jugado — sólo si no hay live.
  if (!out.hayLive) {
    body.clear();
    status = 0;
    if (http_.get(URL_PAST, body, status, 20000) && status == 200 && !body.empty()) {
      Partido u;
      if (parsearUltimo(body, u)) {
        out.ultimoJugado = u;
        out.hayUltimo    = true;
      }
    } else {
#ifdef ARDUINO
      ::Serial.printf("[futbol] past fallo status=%d body=%u\n",
                      status, (unsigned)body.size());
#endif
    }
  }

  // 3) Hoy y mañana.
  body.clear();
  status = 0;
  if (http_.get(URL_NEXT, body, status, 20000) && status == 200 && !body.empty()) {
    std::string hoyISO, mananaISO;
    calcularHoyManana(hoyISO, mananaISO);
    filtrarHoyManana(body, out.hoyManana, hoyISO, mananaISO);
  } else {
#ifdef ARDUINO
    ::Serial.printf("[futbol] next fallo status=%d body=%u\n",
                    status, (unsigned)body.size());
#endif
  }

  out.ok = out.hayLive || out.hayUltimo || !out.hoyManana.empty();
  return out.ok;
}
