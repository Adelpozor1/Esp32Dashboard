#include "futbol_client.h"
#include <ArduinoJson.h>
#include <cstring>

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
  // fecha "YYYY-MM-DD" (>=10 chars), hora "HH:MM:SS" (>=5 chars). Devolvemos "YYYY-MM-DD HH:MM".
  std::string out;
  if (fecha.size() >= 10) out.append(fecha.substr(0, 10));
  if (hora.size() >= 5) {
    if (!out.empty()) out.push_back(' ');
    out.append(hora.substr(0, 5));
  }
  return out;
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
