#include <unity.h>
#include "motogp_client.h"
#include <string>

static const char* PAYLOAD_PAST = R"({"events":[
  {"strEvent":"Spanish Grand Prix","dateEvent":"2026-08-30","strTime":"14:00:00","strHomeTeam":"Rider A","strResult":"1) A 2) B 3) C"},
  {"strEvent":"Italian Grand Prix","dateEvent":"2026-09-06","strTime":"14:00:00","strHomeTeam":"","strResult":""}
]})";

static const char* PAYLOAD_NEXT = R"({"events":[
  {"strEvent":"San Marino Free Practice 1","dateEvent":"2026-09-11","strTime":"09:00:00"},
  {"strEvent":"San Marino Qualifying","dateEvent":"2026-09-12","strTime":"14:00:00"},
  {"strEvent":"San Marino GP","dateEvent":"2026-09-13","strTime":"14:00:00"},
  {"strEvent":"Aragon GP","dateEvent":"2026-09-20","strTime":"14:00:00"}
]})";

void test_parsear_eventos_past(void) {
  std::vector<EventoMotor> v;
  TEST_ASSERT_TRUE(MotogpClient::parsearEventos(PAYLOAD_PAST, v, 5));
  TEST_ASSERT_EQUAL(2, (int)v.size());
  TEST_ASSERT_EQUAL_STRING("Spanish Grand Prix", v[0].nombre.c_str());
  TEST_ASSERT_EQUAL_STRING("Rider A", v[0].ganador.c_str());
  TEST_ASSERT_TRUE(v[0].fechaHora.find("30 ago") != std::string::npos);
  TEST_ASSERT_TRUE(v[0].fechaHora.find("14:00") != std::string::npos);
}

void test_parsear_eventos_proximos(void) {
  std::vector<EventoMotor> v;
  TEST_ASSERT_TRUE(MotogpClient::parsearEventos(PAYLOAD_NEXT, v, 5));
  // Sólo GPs — se filtran FP y Qualifying.
  TEST_ASSERT_EQUAL(2, (int)v.size());
  TEST_ASSERT_EQUAL_STRING("San Marino GP", v[0].nombre.c_str());
  TEST_ASSERT_EQUAL_STRING("", v[0].ganador.c_str());
}

void test_parsear_json_malformado(void) {
  std::vector<EventoMotor> v;
  TEST_ASSERT_FALSE(MotogpClient::parsearEventos("{", v, 5));
}

void test_parsear_respeta_maxN(void) {
  std::vector<EventoMotor> v;
  TEST_ASSERT_TRUE(MotogpClient::parsearEventos(PAYLOAD_PAST, v, 1));
  TEST_ASSERT_EQUAL(1, (int)v.size());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_parsear_eventos_past);
  RUN_TEST(test_parsear_eventos_proximos);
  RUN_TEST(test_parsear_json_malformado);
  RUN_TEST(test_parsear_respeta_maxN);
  return UNITY_END();
}
