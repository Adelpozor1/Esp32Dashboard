#include "f1_client.h"
#include <ArduinoJson.h>
#include <cstdio>
#include <ctime>

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

bool F1Client::fetch(F1Snapshot& out) {
  const char* URL_LAST     = "https://api.jolpi.ca/ergast/f1/current/last/results.json";
  const char* URL_CURRENT  = "https://api.jolpi.ca/ergast/f1/current.json?limit=25";

  std::string body; int status = 0;

  if (!http_.get(URL_LAST, body, status, 20000) || status != 200 || body.empty()) {
#ifdef ARDUINO
    ::Serial.printf("[f1] last fallo status=%d body=%u\n", status, (unsigned)body.size());
#endif
    return false;
  }
  F1Carrera ultima; std::vector<F1Piloto> podio;
  if (!parsearUltima(body, ultima, podio)) return false;

  body.clear();
  if (!http_.get(URL_CURRENT, body, status, 20000) || status != 200 || body.empty()) {
#ifdef ARDUINO
    ::Serial.printf("[f1] current fallo status=%d body=%u\n", status, (unsigned)body.size());
#endif
    return false;
  }
  std::vector<F1Carrera> proximas;
  if (!parsearCalendario(body, proximas, 5)) return false;

  out.ultima = std::move(ultima);
  out.podio = std::move(podio);
  out.proximas = std::move(proximas);
  out.ok = true;
  return true;
}
