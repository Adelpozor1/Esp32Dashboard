#include "motogp_client.h"
#include <ArduinoJson.h>

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace {
std::string valOrEmpty(JsonVariantConst v) {
  if (v.isNull()) return "";
  const char* s = v.as<const char*>();
  return s ? std::string(s) : "";
}
std::string componerFechaHora(const std::string& fecha, const std::string& hora) {
  std::string out;
  if (fecha.size() >= 10) out.append(fecha.substr(0, 10));
  if (hora.size() >= 5) {
    if (!out.empty()) out.push_back(' ');
    out.append(hora.substr(0, 5));
  }
  return out;
}
}  // namespace

bool MotogpClient::parsearEventos(const std::string& json,
                                  std::vector<EventoMotor>& out,
                                  size_t maxN) {
  out.clear();
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  auto ev = doc["events"];
  if (ev.isNull()) return true;
  JsonArrayConst arr = ev.as<JsonArrayConst>();
  if (arr.isNull()) return false;
  for (JsonVariantConst it : arr) {
    if (out.size() >= maxN) break;
    EventoMotor e;
    e.nombre = valOrEmpty(it["strEvent"]);
    if (e.nombre.empty()) e.nombre = valOrEmpty(it["strFilename"]);
    e.fechaHora = componerFechaHora(valOrEmpty(it["dateEvent"]), valOrEmpty(it["strTime"]));
    e.ganador   = valOrEmpty(it["strHomeTeam"]);
    e.resultado = valOrEmpty(it["strResult"]);
    out.push_back(e);
  }
  return true;
}

bool MotogpClient::fetch(MotogpSnapshot& out) {
  const char* URL_PAST = "http://www.thesportsdb.com/api/v1/json/3/eventspastleague.php?id=4407";
  const char* URL_NEXT = "http://www.thesportsdb.com/api/v1/json/3/eventsnextleague.php?id=4407";
  std::string body; int status = 0;

  std::vector<EventoMotor> ultimos;
  if (!http_.get(URL_PAST, body, status, 20000) || status != 200 || body.empty()) {
#ifdef ARDUINO
    ::Serial.printf("[motogp] past fallo status=%d body=%u\n", status, (unsigned)body.size());
#endif
    return false;
  }
  if (!parsearEventos(body, ultimos, 5)) return false;

  body.clear();
  std::vector<EventoMotor> proximos;
  if (!http_.get(URL_NEXT, body, status, 20000) || status != 200 || body.empty()) {
#ifdef ARDUINO
    ::Serial.printf("[motogp] next fallo status=%d body=%u\n", status, (unsigned)body.size());
#endif
    return false;
  }
  if (!parsearEventos(body, proximos, 5)) return false;

  out.ultimos  = std::move(ultimos);
  out.proximos = std::move(proximos);
  out.ok = true;
  return true;
}
