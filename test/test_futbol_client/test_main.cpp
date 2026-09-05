#include <unity.h>
#include "futbol_client.h"
#include <string>

static const char* PAYLOAD_LIVE = R"({"events":[
  {"idLeague":"4900","strHomeTeam":"Otro FC","strAwayTeam":"Otro CF","intHomeScore":"1","intAwayScore":"0","strProgress":"55"},
  {"idLeague":"4335","strHomeTeam":"Real Madrid","strAwayTeam":"Barcelona","intHomeScore":"2","intAwayScore":"1","strProgress":"78"}
]})";

static const char* PAYLOAD_LIVE_SIN_LALIGA = R"({"events":[
  {"idLeague":"4900","strHomeTeam":"Otro FC","strAwayTeam":"Otro CF","intHomeScore":"1","intAwayScore":"0","strProgress":"55"},
  {"idLeague":"4331","strHomeTeam":"Bayern","strAwayTeam":"Dortmund","intHomeScore":"0","intAwayScore":"0","strProgress":"12"}
]})";

static const char* PAYLOAD_ULT = R"({"events":[
  {"strHomeTeam":"Real Betis","strAwayTeam":"Real Madrid","intHomeScore":"1","intAwayScore":"0","dateEvent":"2026-09-04","strTime":"19:00:00"}
]})";

static const char* PAYLOAD_NEXT_MIXTO = R"({"events":[
  {"strHomeTeam":"Athletic","strAwayTeam":"Atletico","dateEvent":"2026-09-05","strTime":"14:15:00"},
  {"strHomeTeam":"Sevilla","strAwayTeam":"Elche","dateEvent":"2026-09-06","strTime":"18:30:00"},
  {"strHomeTeam":"Valencia","strAwayTeam":"Levante","dateEvent":"2026-09-10","strTime":"20:00:00"}
]})";

void test_parsear_live_encuentra_partido_de_la_liga(void) {
  PartidoLive out;
  bool ok = FutbolClient::parsearLive(PAYLOAD_LIVE, out);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL_STRING("Real Madrid", out.local.c_str());
  TEST_ASSERT_EQUAL_STRING("Barcelona",  out.visitante.c_str());
  TEST_ASSERT_EQUAL(2, out.golesLocal);
  TEST_ASSERT_EQUAL(1, out.golesVisitante);
  TEST_ASSERT_EQUAL_STRING("78", out.progreso.c_str());
}

void test_parsear_live_sin_la_liga_devuelve_false(void) {
  PartidoLive out;
  bool ok = FutbolClient::parsearLive(PAYLOAD_LIVE_SIN_LALIGA, out);
  TEST_ASSERT_FALSE(ok);
}

void test_parsear_ultimo_extrae_primero(void) {
  Partido out;
  bool ok = FutbolClient::parsearUltimo(PAYLOAD_ULT, out);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL_STRING("Real Betis",  out.local.c_str());
  TEST_ASSERT_EQUAL_STRING("Real Madrid", out.visitante.c_str());
  TEST_ASSERT_EQUAL(1, out.golesLocal);
  TEST_ASSERT_EQUAL(0, out.golesVisitante);
}

void test_filtrar_hoy_manana_devuelve_solo_esos(void) {
  std::vector<Partido> out;
  FutbolClient::filtrarHoyManana(PAYLOAD_NEXT_MIXTO, out,
                                 "2026-09-05", "2026-09-06");
  TEST_ASSERT_EQUAL(2, (int)out.size());
  TEST_ASSERT_EQUAL_STRING("Athletic", out[0].local.c_str());
  TEST_ASSERT_EQUAL_STRING("Sevilla",  out[1].local.c_str());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_parsear_live_encuentra_partido_de_la_liga);
  RUN_TEST(test_parsear_live_sin_la_liga_devuelve_false);
  RUN_TEST(test_parsear_ultimo_extrae_primero);
  RUN_TEST(test_filtrar_hoy_manana_devuelve_solo_esos);
  return UNITY_END();
}
