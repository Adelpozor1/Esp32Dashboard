#include <unity.h>
#include "meteo_client.h"
#include <string>

// Payload real minificado de Open-Meteo (Madrid, sept 2026), recortado a
// las cabeceras que consume el cliente. Comprobado con curl real.
static const char* PAYLOAD_OK = R"({
  "current_weather": {"temperature":22.3, "windspeed":12.4, "weathercode":1, "time":"2026-09-03T14:00"},
  "hourly": {
    "time": ["2026-09-03T13:00","2026-09-03T14:00","2026-09-03T15:00","2026-09-03T16:00","2026-09-03T17:00","2026-09-03T18:00","2026-09-03T19:00","2026-09-03T20:00","2026-09-03T21:00","2026-09-03T22:00","2026-09-03T23:00","2026-09-04T00:00","2026-09-04T01:00","2026-09-04T02:00"],
    "temperature_2m": [21.0,22.3,23.5,23.0,22.0,20.5,19.0,18.0,17.5,17.0,16.5,16.0,15.5,15.0],
    "weather_code": [1,1,2,2,3,3,61,61,3,2,1,0,0,0]
  },
  "daily": {
    "time": ["2026-09-03","2026-09-04","2026-09-05","2026-09-06","2026-09-07"],
    "temperature_2m_max": [25.0,24.0,22.0,20.5,26.0],
    "temperature_2m_min": [15.0,14.0,13.5,13.0,15.5],
    "weather_code": [1,2,61,3,0]
  }
})";

void test_parsear_payload_valido(void) {
  MeteoSnapshot s;
  bool ok = MeteoClient::parsear(PAYLOAD_OK, s);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_TRUE(s.ok);
  TEST_ASSERT_FLOAT_WITHIN(0.1, 22.3, s.temp_actual_c);
  TEST_ASSERT_EQUAL(1, s.codigo_actual);
  TEST_ASSERT_EQUAL(12, s.viento_kmh);      // 12.4 truncado a int
  TEST_ASSERT_TRUE(s.horas.size() >= 6);
  TEST_ASSERT_TRUE(s.horas.size() <= 12);
  TEST_ASSERT_EQUAL(14, s.horas[0].hora);
  TEST_ASSERT_EQUAL(5, (int)s.dias.size());
  TEST_ASSERT_EQUAL(3, s.dias[0].dia_mes);
  TEST_ASSERT_FLOAT_WITHIN(0.1, 25.0, s.dias[0].tmax);
  TEST_ASSERT_FLOAT_WITHIN(0.1, 15.0, s.dias[0].tmin);
}

void test_parsear_json_malformado(void) {
  MeteoSnapshot s;
  bool ok = MeteoClient::parsear("{", s);
  TEST_ASSERT_FALSE(ok);
}

void test_parsear_sin_hourly(void) {
  const char* payload = R"({
    "current_weather": {"temperature":10, "windspeed":5, "weathercode":0, "time":"2026-09-03T08:00"},
    "daily": {"time":["2026-09-03"], "temperature_2m_max":[15], "temperature_2m_min":[5], "weather_code":[0]}
  })";
  MeteoSnapshot s;
  bool ok = MeteoClient::parsear(payload, s);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL(0, (int)s.horas.size());
  TEST_ASSERT_EQUAL(1, (int)s.dias.size());
}

void test_categoria_wmo_95_es_tormenta(void) {
  TEST_ASSERT_EQUAL(static_cast<int>(IconoMeteo::TORMENTA),
                    static_cast<int>(MeteoClient::categoria(95)));
  TEST_ASSERT_EQUAL(static_cast<int>(IconoMeteo::SOL),
                    static_cast<int>(MeteoClient::categoria(0)));
  TEST_ASSERT_EQUAL(static_cast<int>(IconoMeteo::LLUVIA),
                    static_cast<int>(MeteoClient::categoria(61)));
  TEST_ASSERT_EQUAL(static_cast<int>(IconoMeteo::NIEBLA),
                    static_cast<int>(MeteoClient::categoria(45)));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_parsear_payload_valido);
  RUN_TEST(test_parsear_json_malformado);
  RUN_TEST(test_parsear_sin_hourly);
  RUN_TEST(test_categoria_wmo_95_es_tormenta);
  return UNITY_END();
}
