#include "meteo_client.h"
#include <ArduinoJson.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace {
int parseHora(const char* iso) {
  if (!iso || std::strlen(iso) < 13) return -1;
  return (iso[11] - '0') * 10 + (iso[12] - '0');
}
int parseDiaMes(const char* iso) {
  if (!iso || std::strlen(iso) < 10) return -1;
  return (iso[8] - '0') * 10 + (iso[9] - '0');
}
int parseMes(const char* iso) {
  if (!iso || std::strlen(iso) < 10) return -1;
  return (iso[5] - '0') * 10 + (iso[6] - '0');
}
int parseAnio(const char* iso) {
  if (!iso || std::strlen(iso) < 10) return -1;
  return (iso[0]-'0')*1000 + (iso[1]-'0')*100 + (iso[2]-'0')*10 + (iso[3]-'0');
}
// Devuelve día de la semana (0=Dom..6=Sab) para ISO "YYYY-MM-DD".
int parseDiaSemana(const char* iso) {
  int y = parseAnio(iso), m = parseMes(iso), d = parseDiaMes(iso);
  if (y < 2000 || m < 1 || d < 1) return -1;
  // Algoritmo de Zeller para calendario gregoriano.
  if (m < 3) { m += 12; y -= 1; }
  int k = y % 100, j = y / 100;
  int h = (d + (13*(m+1))/5 + k + k/4 + j/4 + 5*j) % 7;
  // Zeller devuelve 0=sábado. Convertimos a 0=domingo.
  return (h + 6) % 7;
}
}  // namespace

IconoMeteo MeteoClient::categoria(int wmo) {
  if (wmo == 0 || wmo == 1) return IconoMeteo::SOL;        // clear / mainly clear
  if (wmo == 2)             return IconoMeteo::SOL_NUBE;   // partly cloudy
  if (wmo == 3)             return IconoMeteo::NUBE;       // overcast
  if (wmo == 45 || wmo == 48) return IconoMeteo::NIEBLA;
  if ((wmo >= 51 && wmo <= 67) || (wmo >= 80 && wmo <= 82)) return IconoMeteo::LLUVIA;
  if ((wmo >= 71 && wmo <= 77) || wmo == 85 || wmo == 86) return IconoMeteo::NIEVE;
  if (wmo >= 95 && wmo <= 99) return IconoMeteo::TORMENTA;
  return IconoMeteo::NUBE;
}

bool MeteoClient::parsear(const std::string& json, MeteoSnapshot& out) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) return false;

  auto cw = doc["current_weather"];
  if (cw.isNull()) return false;
  out.temp_actual_c = cw["temperature"].as<float>();
  out.codigo_actual = cw["weathercode"].as<int>();
  out.viento_kmh = static_cast<int>(cw["windspeed"].as<float>());
  const char* horaActualIso = cw["time"] | (const char*)nullptr;
  const int horaActual = parseHora(horaActualIso);

  out.horas.clear();
  auto ht = doc["hourly"]["time"].as<JsonArrayConst>();
  auto ht2m = doc["hourly"]["temperature_2m"].as<JsonArrayConst>();
  auto hwc = doc["hourly"]["weather_code"].as<JsonArrayConst>();
  if (!ht.isNull() && !ht2m.isNull() && !hwc.isNull() && horaActual >= 0) {
    size_t inicio = 0;
    for (size_t i = 0; i < ht.size(); ++i) {
      if (parseHora(ht[i].as<const char*>()) >= horaActual) { inicio = i; break; }
    }
    for (size_t i = inicio; i < ht.size() && out.horas.size() < 12; ++i) {
      MeteoSnapshot::Hora h;
      h.hora   = static_cast<int8_t>(parseHora(ht[i].as<const char*>()));
      h.temp_c = ht2m[i].as<float>();
      h.codigo = hwc[i].as<int>();
      out.horas.push_back(h);
    }
  }

  out.dias.clear();
  auto dt = doc["daily"]["time"].as<JsonArrayConst>();
  auto dmax = doc["daily"]["temperature_2m_max"].as<JsonArrayConst>();
  auto dmin = doc["daily"]["temperature_2m_min"].as<JsonArrayConst>();
  auto dwc  = doc["daily"]["weather_code"].as<JsonArrayConst>();
  if (!dt.isNull() && !dmax.isNull() && !dmin.isNull() && !dwc.isNull()) {
    for (size_t i = 0; i < dt.size() && out.dias.size() < 6; ++i) {
      const char* iso = dt[i].as<const char*>();
      MeteoSnapshot::Dia d;
      d.dia_mes    = static_cast<int8_t>(parseDiaMes(iso));
      d.mes        = static_cast<int8_t>(parseMes(iso));
      d.dia_semana = static_cast<int8_t>(parseDiaSemana(iso));
      d.tmax       = dmax[i].as<float>();
      d.tmin       = dmin[i].as<float>();
      d.codigo     = dwc[i].as<int>();
      out.dias.push_back(d);
    }
  }

  out.ok = true;
  return true;
}

bool MeteoClient::fetch(double lat, double lon, MeteoSnapshot& out) {
  char url[512];
  std::snprintf(url, sizeof(url),
                "http://api.open-meteo.com/v1/forecast"
                "?latitude=%.4f&longitude=%.4f"
                "&current_weather=true"
                "&hourly=temperature_2m,weather_code"
                "&daily=temperature_2m_max,temperature_2m_min,weather_code"
                "&forecast_days=5&timezone=Europe%%2FMadrid",
                lat, lon);
  std::string body;
  int status = 0;
  if (!http_.get(url, body, status, 20000)) {
#ifdef ARDUINO
    ::Serial.printf("[meteo] http.get fallo (status=%d body=%u)\n",
                    status, (unsigned)body.size());
#endif
    return false;
  }
  if (status != 200 || body.empty()) {
#ifdef ARDUINO
    ::Serial.printf("[meteo] respuesta inutil (status=%d body=%u)\n",
                    status, (unsigned)body.size());
#endif
    return false;
  }
  MeteoSnapshot tmp;
  if (!parsear(body, tmp)) {
#ifdef ARDUINO
    ::Serial.println("[meteo] parseo JSON fallo");
#endif
    return false;
  }
  out = tmp;
  return true;
}
