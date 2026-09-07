#include "f1_client.h"
#include <ArduinoJson.h>
#include <cstdio>
#include <ctime>

#ifdef ARDUINO
#include <Arduino.h>
#include <Stream.h>
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

bool F1Client::parsearUltima(const std::string& json,
                             F1Carrera& carrera,
                             std::vector<F1Piloto>& podio) {
  podio.clear();
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  auto races = doc["MRData"]["RaceTable"]["Races"].as<JsonArrayConst>();
  if (races.isNull() || races.size() == 0) return false;
  auto race = races[0];
  carrera.ronda = atoi(valOrEmpty(race["round"]).c_str());
  carrera.nombreGp = valOrEmpty(race["raceName"]);
  carrera.circuito = valOrEmpty(race["Circuit"]["circuitName"]);
  carrera.fechaHora = componerFechaHora(valOrEmpty(race["date"]), valOrEmpty(race["time"]));

  auto results = race["Results"].as<JsonArrayConst>();
  if (results.isNull()) return true;
  for (JsonVariantConst r : results) {
    if (podio.size() >= 3) break;
    F1Piloto p;
    p.posicion = atoi(valOrEmpty(r["position"]).c_str());
    p.nombre = valOrEmpty(r["Driver"]["familyName"]);
    p.equipo = valOrEmpty(r["Constructor"]["name"]);
    p.tiempo = valOrEmpty(r["Time"]["time"]);
    podio.push_back(p);
  }
  return true;
}

bool F1Client::parsearCalendario(const std::string& json,
                                 std::vector<F1Carrera>& proximas,
                                 size_t maxN) {
  proximas.clear();
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  auto races = doc["MRData"]["RaceTable"]["Races"].as<JsonArrayConst>();
  if (races.isNull()) return false;

  // Fecha actual como "YYYY-MM-DD" para filtrar próximas
  char hoy[11] = {0};
  time_t now = time(nullptr);
  struct tm tm;
  localtime_r(&now, &tm);
  std::snprintf(hoy, sizeof(hoy), "%04d-%02d-%02d",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);

  for (JsonVariantConst r : races) {
    if (proximas.size() >= maxN) break;
    std::string fecha = valOrEmpty(r["date"]);
    // Sólo próximas: fecha >= hoy (comparación lexicográfica funciona en ISO)
    if (fecha.size() >= 10 && std::string(hoy).size() == 10 && fecha < hoy) continue;
    F1Carrera c;
    c.ronda = atoi(valOrEmpty(r["round"]).c_str());
    c.nombreGp = valOrEmpty(r["raceName"]);
    c.circuito = valOrEmpty(r["Circuit"]["circuitName"]);
    c.fechaHora = componerFechaHora(fecha, valOrEmpty(r["time"]));
    proximas.push_back(c);
  }
  return true;
}

bool F1Client::parsearClasificacion(const std::string& json,
                                    std::vector<F1PilotoClas>& out,
                                    size_t maxN) {
  out.clear();
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  auto sl = doc["MRData"]["StandingsTable"]["StandingsLists"].as<JsonArrayConst>();
  if (sl.isNull() || sl.size() == 0) return false;
  auto ds = sl[0]["DriverStandings"].as<JsonArrayConst>();
  if (ds.isNull()) return false;
  for (JsonVariantConst r : ds) {
    if (out.size() >= maxN) break;
    F1PilotoClas p;
    p.posicion = atoi(valOrEmpty(r["position"]).c_str());
    p.nombre = valOrEmpty(r["Driver"]["familyName"]);
    auto cons = r["Constructors"].as<JsonArrayConst>();
    if (!cons.isNull() && cons.size() > 0) p.equipo = valOrEmpty(cons[0]["name"]);
    p.puntos = atoi(valOrEmpty(r["points"]).c_str());
    out.push_back(p);
  }
  return true;
}

bool F1Client::fetch(F1Snapshot& out) {
  const char* URL_CURRENT   = "https://api.jolpi.ca/ergast/f1/current.json?limit=15";
  const char* URL_STANDINGS = "https://api.jolpi.ca/ergast/f1/current/driverstandings.json?limit=10";

  int status = 0;

#ifdef ARDUINO
  // Streaming: deserializeJson lee directamente del socket TLS. Evita el
  // acumular ~10 KB en un String de HTTPClient::getString(), que ya se ha
  // demostrado que revienta el heap tras varios fetches encadenados.
  bool okCal = http_.getStreamed(URL_CURRENT, status, 20000,
    [&out](void* streamPtr) -> bool {
      Stream* stream = static_cast<Stream*>(streamPtr);
      if (!stream) return false;
      JsonDocument filter;
      filter["MRData"]["RaceTable"]["Races"][0]["round"] = true;
      filter["MRData"]["RaceTable"]["Races"][0]["raceName"] = true;
      filter["MRData"]["RaceTable"]["Races"][0]["date"] = true;
      filter["MRData"]["RaceTable"]["Races"][0]["time"] = true;
      filter["MRData"]["RaceTable"]["Races"][0]["Circuit"]["circuitName"] = true;
      JsonDocument doc;
      if (deserializeJson(doc, *stream, DeserializationOption::Filter(filter))) return false;
      auto races = doc["MRData"]["RaceTable"]["Races"].as<JsonArrayConst>();
      if (races.isNull()) return false;
      char hoy[11] = {0};
      time_t now = time(nullptr);
      struct tm tm;
      localtime_r(&now, &tm);
      std::snprintf(hoy, sizeof(hoy), "%04d-%02d-%02d",
                    tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
      for (JsonVariantConst r : races) {
        if (out.proximas.size() >= 5) break;
        std::string fecha = valOrEmpty(r["date"]);
        if (fecha.size() >= 10 && fecha < hoy) continue;
        F1Carrera c;
        c.ronda = atoi(valOrEmpty(r["round"]).c_str());
        c.nombreGp = valOrEmpty(r["raceName"]);
        c.circuito = valOrEmpty(r["Circuit"]["circuitName"]);
        c.fechaHora = componerFechaHora(fecha, valOrEmpty(r["time"]));
        out.proximas.push_back(c);
      }
      return true;
    });
  ::Serial.printf("[f1] current %s status=%d proximas=%d\n",
                  okCal ? "OK" : "FALLO", status, (int)out.proximas.size());
  if (!okCal) return false;

  status = 0;
  bool okClas = http_.getStreamed(URL_STANDINGS, status, 20000,
    [&out](void* streamPtr) -> bool {
      Stream* stream = static_cast<Stream*>(streamPtr);
      if (!stream) return false;
      JsonDocument filter;
      filter["MRData"]["StandingsTable"]["StandingsLists"][0]["DriverStandings"][0]["position"] = true;
      filter["MRData"]["StandingsTable"]["StandingsLists"][0]["DriverStandings"][0]["points"] = true;
      filter["MRData"]["StandingsTable"]["StandingsLists"][0]["DriverStandings"][0]["Driver"]["familyName"] = true;
      filter["MRData"]["StandingsTable"]["StandingsLists"][0]["DriverStandings"][0]["Constructors"][0]["name"] = true;
      JsonDocument doc;
      if (deserializeJson(doc, *stream, DeserializationOption::Filter(filter))) return false;
      auto sl = doc["MRData"]["StandingsTable"]["StandingsLists"].as<JsonArrayConst>();
      if (sl.isNull() || sl.size() == 0) return false;
      auto ds = sl[0]["DriverStandings"].as<JsonArrayConst>();
      if (ds.isNull()) return false;
      for (JsonVariantConst r : ds) {
        if (out.clasificacion.size() >= 10) break;
        F1PilotoClas p;
        p.posicion = atoi(valOrEmpty(r["position"]).c_str());
        p.nombre = valOrEmpty(r["Driver"]["familyName"]);
        auto cons = r["Constructors"].as<JsonArrayConst>();
        if (!cons.isNull() && cons.size() > 0) p.equipo = valOrEmpty(cons[0]["name"]);
        p.puntos = atoi(valOrEmpty(r["points"]).c_str());
        out.clasificacion.push_back(p);
      }
      return true;
    });
  ::Serial.printf("[f1] standings %s status=%d n=%d\n",
                  okClas ? "OK" : "FALLO", status, (int)out.clasificacion.size());
#else
  // Path native (tests): usa http_.get + parseadores puros. Los tests pasan sus
  // JSON como strings.
  std::string body;
  if (!http_.get(URL_CURRENT, body, status, 20000) || status != 200 || body.empty()) return false;
  std::vector<F1Carrera> proximas;
  if (!parsearCalendario(body, proximas, 5)) return false;
  out.proximas = std::move(proximas);
  body.clear();
  if (http_.get(URL_STANDINGS, body, status, 20000) && status == 200 && !body.empty()) {
    std::vector<F1PilotoClas> cls;
    if (parsearClasificacion(body, cls, 10)) out.clasificacion = std::move(cls);
  }
#endif
  out.ok = true;
  return true;
}
