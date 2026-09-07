#include "motogp_client.h"
#include <ArduinoJson.h>
#include <ctime>
#include <cstdio>
#include <cctype>
#include <algorithm>

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

bool MotogpClient::parsearUltimaRondaYSeason(const std::string& json,
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

std::string MotogpClient::extraerPaisDeSesion(const std::string& s) {
  // Buscar la primera aparición de cualquier sufijo de sesión/GP y quedarnos
  // con el prefijo. Aceptamos también "GP" y "Grand Prix" para past.
  static const char* SUFIJOS[] = {
    " Free Practice", " Practice", " Sprint Race", " Sprint",
    " Qualifying", " Warm Up", " Warm-up", " Race",
    " Grand Prix", " GP", " Test", " Shakedown"
  };
  size_t corte = std::string::npos;
  for (const char* suf : SUFIJOS) {
    size_t p = s.find(suf);
    if (p != std::string::npos && p < corte) corte = p;
  }
  std::string out = (corte == std::string::npos) ? s : s.substr(0, corte);
  // trim derecha
  while (!out.empty() && (out.back() == ' ' || out.back() == '\t')) out.pop_back();
  return out;
}

bool MotogpClient::parsearRondaComoGp(const std::string& json, EventoMotor& out) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  auto ev = doc["events"].as<JsonArrayConst>();
  if (ev.isNull() || ev.size() == 0) return false;
  std::string pais;
  std::string fechaMax;
  std::string horaCarrera;
  for (JsonVariantConst it : ev) {
    std::string nombre = valOrEmpty(it["strEvent"]);
    if (pais.empty() && !nombre.empty()) pais = extraerPaisDeSesion(nombre);
    std::string f = valOrEmpty(it["dateEvent"]);
    if (f.size() >= 10 && f > fechaMax) {
      fechaMax = f;
      horaCarrera = valOrEmpty(it["strTime"]);
    }
  }
  if (pais.empty() || fechaMax.empty()) return false;
  out.nombre = pais + " GP";
  // Fecha en formato ISO YYYY-MM-DD, igual que F1 (pantalla lo formatea sola).
  out.fechaHora = fechaMax;
  (void)horaCarrera;
  out.ganador.clear();
  out.resultado.clear();
  return true;
}

bool MotogpClient::parsearClasificacion(const std::string& json,
                                        std::vector<MotogpPilotoClas>& out,
                                        size_t maxN) {
  out.clear();
  // El JSON de la API oficial es grande (~24KB) con muchos campos por piloto.
  // Filtramos con un JsonDocument-filter para descartar los campos que no
  // usamos y ahorrar heap durante deserializeJson.
  JsonDocument filter;
  filter["classification"][0]["position"] = true;
  filter["classification"][0]["points"] = true;
  filter["classification"][0]["rider"]["full_name"] = true;
  filter["classification"][0]["team"]["name"] = true;
  filter["classification"][0]["constructor"]["name"] = true;

  JsonDocument doc;
  if (deserializeJson(doc, json, DeserializationOption::Filter(filter))) return false;
  auto cls = doc["classification"].as<JsonArrayConst>();
  if (cls.isNull()) return false;
  for (JsonVariantConst r : cls) {
    if (out.size() >= maxN) break;
    MotogpPilotoClas p;
    p.posicion = r["position"].as<int>();
    p.puntos   = r["points"].as<int>();
    p.nombre   = valOrEmpty(r["rider"]["full_name"]);
    p.equipo   = valOrEmpty(r["team"]["name"]);
    p.marca    = valOrEmpty(r["constructor"]["name"]);
    if (p.posicion <= 0) continue;
    out.push_back(p);
  }
  return true;
}

bool MotogpClient::fetch(MotogpSnapshot& out) {
  const char* URL_PAST = "https://www.thesportsdb.com/api/v1/json/3/eventspastleague.php?id=4407";
  std::string body; int status = 0;

  if (!http_.get(URL_PAST, body, status, 20000) || status != 200 || body.empty()) {
#ifdef ARDUINO
    ::Serial.printf("[motogp] past fallo status=%d body=%u\n", status, (unsigned)body.size());
#endif
    return false;
  }
  std::vector<EventoMotor> ultimos;
  if (!parsearEventos(body, ultimos, 5)) return false;
  int ultimaRonda = 0;
  std::string season;
  parsearUltimaRondaYSeason(body, ultimaRonda, season);

  // TheSportsDB no expone el "GP raíz" para rondas futuras; sólo sesiones
  // sueltas (FP, Qualifying, Sprint...). Componemos un "GP virtual" pidiendo
  // eventsround.php por cada una de las próximas 3 rondas y agrupando.
  std::vector<EventoMotor> proximos;
  if (ultimaRonda > 0 && !season.empty()) {
    for (int r = ultimaRonda + 1; r <= ultimaRonda + 3; ++r) {
      char url[160];
      std::snprintf(url, sizeof(url),
        "https://www.thesportsdb.com/api/v1/json/3/eventsround.php?id=4407&r=%d&s=%s",
        r, season.c_str());
      body.clear();
      status = 0;
      if (!http_.get(url, body, status, 20000) || status != 200 || body.empty()) {
#ifdef ARDUINO
        ::Serial.printf("[motogp] ronda %d fallo status=%d\n", r, status);
#endif
        continue;
      }
      EventoMotor gp;
      if (parsearRondaComoGp(body, gp)) proximos.push_back(gp);
    }
  }

  out.ultimos  = std::move(ultimos);
  out.proximos = std::move(proximos);

  // Clasificación del mundial via API oficial motogp.com.
  // UUIDs de la temporada 2026 (actualizar cada temporada). categoryUuid MotoGP
  // es estable, pero seasonUuid cambia — consultable en /motogp/v1/results/seasons.
  //
  // El body pesa ~24 KB. getString() lo revienta el heap TLS. Usamos streaming:
  // deserializeJson lee del stream directamente sin acumular String.
  const char* URL_CLAS =
    "https://api.motogp.pulselive.com/motogp/v1/results/standings?"
    "seasonUuid=e88b4e43-2209-47aa-8e83-0e0b1cedde6e&"
    "categoryUuid=e8c110ad-64aa-4e8e-8a86-f2f152f6a942";
  status = 0;
#ifdef ARDUINO
  bool okClas = http_.getStreamed(URL_CLAS, status, 20000,
    [&out](void* streamPtr) -> bool {
      Stream* stream = static_cast<Stream*>(streamPtr);
      if (!stream) return false;
      JsonDocument filter;
      filter["classification"][0]["position"] = true;
      filter["classification"][0]["points"] = true;
      filter["classification"][0]["rider"]["full_name"] = true;
      filter["classification"][0]["team"]["name"] = true;
      filter["classification"][0]["constructor"]["name"] = true;
      JsonDocument doc;
      DeserializationError err = deserializeJson(doc, *stream,
                                                 DeserializationOption::Filter(filter));
      if (err) {
        ::Serial.printf("[motogp] deserializeJson err=%s\n", err.c_str());
        return false;
      }
      auto cls = doc["classification"].as<JsonArrayConst>();
      if (cls.isNull()) return false;
      for (JsonVariantConst r : cls) {
        if (out.clasificacion.size() >= 10) break;
        MotogpPilotoClas p;
        p.posicion = r["position"].as<int>();
        p.puntos   = r["points"].as<int>();
        p.nombre   = valOrEmpty(r["rider"]["full_name"]);
        p.equipo   = valOrEmpty(r["team"]["name"]);
        p.marca    = valOrEmpty(r["constructor"]["name"]);
        if (p.posicion <= 0) continue;
        out.clasificacion.push_back(p);
      }
      return true;
    });
  ::Serial.printf("[motogp] clasificacion %s n=%d status=%d\n",
                  okClas ? "OK" : "FALLO",
                  (int)out.clasificacion.size(), status);
#endif

  out.ok = !out.ultimos.empty() || !out.proximos.empty() || !out.clasificacion.empty();
  return out.ok;
}
