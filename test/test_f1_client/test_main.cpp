#include <unity.h>
#include "f1_client.h"
#include <string>

// Payload Jolpica reducido — última carrera con podio
static const char* PAYLOAD_LAST = R"({
"MRData":{"RaceTable":{"Races":[{
  "season":"2026","round":"15","raceName":"Italian Grand Prix",
  "Circuit":{"circuitName":"Autodromo Nazionale Monza"},
  "date":"2026-09-01","time":"13:00:00Z",
  "Results":[
    {"position":"1","Driver":{"familyName":"Verstappen"},"Constructor":{"name":"Red Bull"},"Time":{"time":"1:22:35.045"}},
    {"position":"2","Driver":{"familyName":"Hamilton"},"Constructor":{"name":"Ferrari"},"Time":{"time":"+5.123"}},
    {"position":"3","Driver":{"familyName":"Norris"},"Constructor":{"name":"McLaren"},"Time":{"time":"+9.456"}},
    {"position":"4","Driver":{"familyName":"Russell"},"Constructor":{"name":"Mercedes"},"Time":{"time":"+15.001"}}
  ]}]}}
})";

// Payload calendario — con fechas variadas
static const char* PAYLOAD_CAL = R"({
"MRData":{"RaceTable":{"Races":[
  {"round":"1","raceName":"Bahrain GP","Circuit":{"circuitName":"Sakhir"},"date":"2026-03-01","time":"15:00:00Z"},
  {"round":"18","raceName":"Brazil GP","Circuit":{"circuitName":"Interlagos"},"date":"2099-11-01","time":"17:00:00Z"},
  {"round":"19","raceName":"Vegas GP","Circuit":{"circuitName":"Vegas Street"},"date":"2099-11-22","time":"03:00:00Z"}
]}}
})";

void test_parsear_ultima_devuelve_top3(void) {
  F1Carrera c; std::vector<F1Piloto> p;
  TEST_ASSERT_TRUE(F1Client::parsearUltima(PAYLOAD_LAST, c, p));
  TEST_ASSERT_EQUAL_STRING("Italian Grand Prix", c.nombreGp.c_str());
  TEST_ASSERT_EQUAL_STRING("Autodromo Nazionale Monza", c.circuito.c_str());
  TEST_ASSERT_EQUAL(15, c.ronda);
  TEST_ASSERT_EQUAL(3, (int)p.size());
  TEST_ASSERT_EQUAL_STRING("Verstappen", p[0].nombre.c_str());
  TEST_ASSERT_EQUAL_STRING("Red Bull", p[0].equipo.c_str());
}

void test_parsear_calendario_filtra_pasadas(void) {
  std::vector<F1Carrera> v;
  TEST_ASSERT_TRUE(F1Client::parsearCalendario(PAYLOAD_CAL, v, 5));
  // La fecha 2026-03-01 puede o no ser pasada según el reloj del test host;
  // las de 2099 SÍ deben quedar. Verificamos al menos que las 2099 salen y
  // el orden se preserva.
  bool hayBrazil = false, hayVegas = false;
  for (const auto& c : v) {
    if (c.nombreGp == "Brazil GP") hayBrazil = true;
    if (c.nombreGp == "Vegas GP")  hayVegas = true;
  }
  TEST_ASSERT_TRUE(hayBrazil);
  TEST_ASSERT_TRUE(hayVegas);
}

void test_parsear_json_malformado(void) {
  F1Carrera c; std::vector<F1Piloto> p;
  TEST_ASSERT_FALSE(F1Client::parsearUltima("{", c, p));
  std::vector<F1Carrera> v;
  TEST_ASSERT_FALSE(F1Client::parsearCalendario("{", v, 5));
}

void test_parsear_ultima_sin_races(void) {
  const char* payload = R"({"MRData":{"RaceTable":{"Races":[]}}})";
  F1Carrera c; std::vector<F1Piloto> p;
  TEST_ASSERT_FALSE(F1Client::parsearUltima(payload, c, p));
}

// Payload standings — clasificación mundial de pilotos
static const char* PAYLOAD_STANDINGS = R"({
"MRData":{"StandingsTable":{"StandingsLists":[{
  "season":"2026","round":"15",
  "DriverStandings":[
    {"position":"1","points":"242","Driver":{"familyName":"Antonelli"},"Constructors":[{"name":"Mercedes"}]},
    {"position":"2","points":"183","Driver":{"familyName":"Russell"},"Constructors":[{"name":"Mercedes"}]},
    {"position":"3","points":"183","Driver":{"familyName":"Hamilton"},"Constructors":[{"name":"Ferrari"}]},
    {"position":"4","points":"159","Driver":{"familyName":"Norris"},"Constructors":[{"name":"McLaren"}]}
  ]}]}}
})";

void test_parsear_clasificacion_devuelve_top4(void) {
  std::vector<F1PilotoClas> v;
  TEST_ASSERT_TRUE(F1Client::parsearClasificacion(PAYLOAD_STANDINGS, v, 4));
  TEST_ASSERT_EQUAL(4, (int)v.size());
  TEST_ASSERT_EQUAL_STRING("Antonelli", v[0].nombre.c_str());
  TEST_ASSERT_EQUAL_STRING("Mercedes", v[0].equipo.c_str());
  TEST_ASSERT_EQUAL(242, v[0].puntos);
  TEST_ASSERT_EQUAL(4, v[3].posicion);
}

void test_parsear_clasificacion_json_malformado(void) {
  std::vector<F1PilotoClas> v;
  TEST_ASSERT_FALSE(F1Client::parsearClasificacion("{", v, 4));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_parsear_ultima_devuelve_top3);
  RUN_TEST(test_parsear_calendario_filtra_pasadas);
  RUN_TEST(test_parsear_json_malformado);
  RUN_TEST(test_parsear_ultima_sin_races);
  RUN_TEST(test_parsear_clasificacion_devuelve_top4);
  RUN_TEST(test_parsear_clasificacion_json_malformado);
  return UNITY_END();
}
