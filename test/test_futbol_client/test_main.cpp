#include <unity.h>
#include "futbol_client.h"
#include <string>

static const char* PAYLOAD_ULT = R"({"events":[
  {"strHomeTeam":"Real Madrid","strAwayTeam":"Barcelona","intHomeScore":"2","intAwayScore":"1","dateEvent":"2026-09-01","strTime":"20:00:00"},
  {"strHomeTeam":"Atletico","strAwayTeam":"Sevilla","intHomeScore":"0","intAwayScore":"0","dateEvent":"2026-09-02","strTime":"18:30:00"},
  {"strHomeTeam":"Betis","strAwayTeam":"Valencia","intHomeScore":"3","intAwayScore":"2","dateEvent":"2026-09-02","strTime":"20:30:00"}
]})";

static const char* PAYLOAD_NEXT = R"({"events":[
  {"strHomeTeam":"Villarreal","strAwayTeam":"Osasuna","intHomeScore":null,"intAwayScore":null,"dateEvent":"2026-09-06","strTime":"16:00:00"},
  {"strHomeTeam":"Real Sociedad","strAwayTeam":"Girona","dateEvent":"2026-09-06","strTime":"18:30:00"}
]})";

void test_parsear_eventos_pasados(void) {
  std::vector<Partido> v;
  bool ok = FutbolClient::parsearEventos(PAYLOAD_ULT, v, 5);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL(3, (int)v.size());
  TEST_ASSERT_EQUAL_STRING("Real Madrid", v[0].local.c_str());
  TEST_ASSERT_EQUAL_STRING("Barcelona", v[0].visitante.c_str());
  TEST_ASSERT_EQUAL(2, v[0].golesLocal);
  TEST_ASSERT_EQUAL(1, v[0].golesVisitante);
  TEST_ASSERT_TRUE(v[0].fechaHora.find("1 sep") != std::string::npos);
  TEST_ASSERT_TRUE(v[0].fechaHora.find("20:00") != std::string::npos);
}

void test_parsear_eventos_proximos_sin_resultado(void) {
  std::vector<Partido> v;
  bool ok = FutbolClient::parsearEventos(PAYLOAD_NEXT, v, 5);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL(2, (int)v.size());
  TEST_ASSERT_EQUAL(-1, v[0].golesLocal);
  TEST_ASSERT_EQUAL(-1, v[0].golesVisitante);
  TEST_ASSERT_EQUAL(-1, v[1].golesLocal);   // sin campo intHomeScore
}

void test_parsear_json_malformado(void) {
  std::vector<Partido> v;
  bool ok = FutbolClient::parsearEventos("{", v, 5);
  TEST_ASSERT_FALSE(ok);
}

void test_parsear_respeta_maxN(void) {
  std::vector<Partido> v;
  bool ok = FutbolClient::parsearEventos(PAYLOAD_ULT, v, 2);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL(2, (int)v.size());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_parsear_eventos_pasados);
  RUN_TEST(test_parsear_eventos_proximos_sin_resultado);
  RUN_TEST(test_parsear_json_malformado);
  RUN_TEST(test_parsear_respeta_maxN);
  return UNITY_END();
}
