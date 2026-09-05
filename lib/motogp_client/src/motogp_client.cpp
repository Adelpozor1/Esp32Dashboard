#include "motogp_client.h"
#include <ArduinoJson.h>
#include <ctime>
#include <cstdio>
#include <cctype>
#include <algorithm>

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
    // Filtrar sesiones que no son la carrera: FP1/FP2/FP3, Q1/Q2, Warm Up, Sprint.
    // Aceptamos si el nombre contiene "GP" o "Grand Prix" y NO contiene esas keywords.
    const std::string& n = e.nombre;
    auto contieneCI = [](const std::string& s, const char* needle) {
      if (s.empty() || !needle) return false;
      std::string a = s, b = needle;
      for (auto& c : a) c = (char)std::tolower((unsigned char)c);
      for (auto& c : b) c = (char)std::tolower((unsigned char)c);
      return a.find(b) != std::string::npos;
    };
    if (contieneCI(n, "free practice") || contieneCI(n, "qualifying") ||
        contieneCI(n, "warm up")      || contieneCI(n, "warm-up") ||
        contieneCI(n, "sprint")       || contieneCI(n, "practice ")) {
      continue;
    }
    e.fechaHora = componerFechaHora(valOrEmpty(it["dateEvent"]), valOrEmpty(it["strTime"]));
    e.ganador   = valOrEmpty(it["strHomeTeam"]);
    e.resultado = valOrEmpty(it["strResult"]);
    out.push_back(e);
  }
  return true;
}

bool MotogpClient::fetch(MotogpSnapshot& out) {
  const char* URL_PAST = "https://www.thesportsdb.com/api/v1/json/3/eventspastleague.php?id=4407";
  const char* URL_NEXT = "https://www.thesportsdb.com/api/v1/json/3/eventsnextleague.php?id=4407";
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
