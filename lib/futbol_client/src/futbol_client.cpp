#include "futbol_client.h"
#include <ArduinoJson.h>
#include <cstring>
#include <ctime>
#include <cstdio>

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace {
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
}  // namespace

bool FutbolClient::parsearEventos(const std::string& json,
                                  std::vector<Partido>& out,
                                  size_t maxN) {
  out.clear();
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) return false;

  // TheSportsDB devuelve `events`. Puede ser null si no hay eventos.
  auto ev = doc["events"];
  if (ev.isNull()) return true;   // no hay eventos, no es error
  JsonArrayConst arr = ev.as<JsonArrayConst>();
  if (arr.isNull()) return false;

  for (JsonVariantConst it : arr) {
    if (out.size() >= maxN) break;
    Partido p;
    p.local     = valOrEmpty(it["strHomeTeam"]);
    p.visitante = valOrEmpty(it["strAwayTeam"]);
    p.golesLocal    = parseGoles(it["intHomeScore"]);
    p.golesVisitante = parseGoles(it["intAwayScore"]);
    p.fechaHora = componerFechaHora(valOrEmpty(it["dateEvent"]),
                                     valOrEmpty(it["strTime"]));
    out.push_back(p);
  }
  return true;
}

bool FutbolClient::fetch(FutbolSnapshot& out) {
  const char* URL_PAST = "https://www.thesportsdb.com/api/v1/json/3/eventspastleague.php?id=4335";
  const char* URL_NEXT = "https://www.thesportsdb.com/api/v1/json/3/eventsnextleague.php?id=4335";

  std::string body;
  int status = 0;

  std::vector<Partido> ultimos;
  if (!http_.get(URL_PAST, body, status, 20000) || status != 200 || body.empty()) {
#ifdef ARDUINO
    ::Serial.printf("[futbol] past fallo status=%d body=%u:%s\n", status, (unsigned)body.size(), body.substr(0, 200).c_str());
#endif
    return false;
  }
  if (!parsearEventos(body, ultimos, 5)) return false;

  body.clear();
  std::vector<Partido> proximos;
  if (!http_.get(URL_NEXT, body, status, 20000) || status != 200 || body.empty()) {
#ifdef ARDUINO
    ::Serial.printf("[futbol] next fallo status=%d body=%u\n", status, (unsigned)body.size());
#endif
    return false;
  }
  if (!parsearEventos(body, proximos, 5)) return false;

  out.ultimos  = std::move(ultimos);
  out.proximos = std::move(proximos);
  out.ok = true;
  return true;
}
