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

void test_extraer_pais_de_sesion(void) {
  TEST_ASSERT_EQUAL_STRING("San Marino",
      MotogpClient::extraerPaisDeSesion("San Marino Free Practice 1").c_str());
  TEST_ASSERT_EQUAL_STRING("Thailand",
      MotogpClient::extraerPaisDeSesion("Thailand Sprint Race").c_str());
  TEST_ASSERT_EQUAL_STRING("Portugal",
      MotogpClient::extraerPaisDeSesion("Portugal Qualifying 2").c_str());
  TEST_ASSERT_EQUAL_STRING("Aragón",
      MotogpClient::extraerPaisDeSesion("Aragón GP").c_str());
  TEST_ASSERT_EQUAL_STRING("Spanish",
      MotogpClient::extraerPaisDeSesion("Spanish Grand Prix").c_str());
}

static const char* PAYLOAD_RONDA_14 = R"({"events":[
  {"strEvent":"San Marino Free Practice 1","dateEvent":"2026-09-11","strTime":"08:45:00","intRound":"14","strSeason":"2026"},
  {"strEvent":"San Marino Practice","dateEvent":"2026-09-11","strTime":"13:00:00","intRound":"14","strSeason":"2026"},
  {"strEvent":"San Marino Sprint Race","dateEvent":"2026-09-12","strTime":"13:00:00","intRound":"14","strSeason":"2026"},
  {"strEvent":"San Marino Qualifying 1","dateEvent":"2026-09-12","strTime":"08:50:00","intRound":"14","strSeason":"2026"},
  {"strEvent":"San Marino Qualifying 2","dateEvent":"2026-09-13","strTime":"09:15:00","intRound":"14","strSeason":"2026"}
]})";

void test_parsear_ronda_como_gp(void) {
  EventoMotor gp;
  TEST_ASSERT_TRUE(MotogpClient::parsearRondaComoGp(PAYLOAD_RONDA_14, gp));
  TEST_ASSERT_EQUAL_STRING("San Marino GP", gp.nombre.c_str());
  // Fecha máxima ISO = 2026-09-13 (domingo).
  TEST_ASSERT_EQUAL_STRING("2026-09-13", gp.fechaHora.c_str());
}

static const char* PAYLOAD_CLASIFICACION = R"({
  "classification":[
    {"position":1,"points":256,"rider":{"full_name":"Jorge Martin"},"team":{"name":"Aprilia Racing"},"constructor":{"name":"Aprilia"}},
    {"position":2,"points":220,"rider":{"full_name":"Marc Marquez"},"team":{"name":"Ducati Lenovo Team"},"constructor":{"name":"Ducati"}},
    {"position":3,"points":198,"rider":{"full_name":"Pedro Acosta"},"team":{"name":"Red Bull KTM Factory"},"constructor":{"name":"KTM"}}
  ]
})";

void test_parsear_clasificacion_motogp(void) {
  std::vector<MotogpPilotoClas> v;
  TEST_ASSERT_TRUE(MotogpClient::parsearClasificacion(PAYLOAD_CLASIFICACION, v, 5));
  TEST_ASSERT_EQUAL(3, (int)v.size());
  TEST_ASSERT_EQUAL(1, v[0].posicion);
  TEST_ASSERT_EQUAL_STRING("Jorge Martin", v[0].nombre.c_str());
  TEST_ASSERT_EQUAL_STRING("Aprilia Racing", v[0].equipo.c_str());
  TEST_ASSERT_EQUAL_STRING("Aprilia", v[0].marca.c_str());
  TEST_ASSERT_EQUAL(256, v[0].puntos);
}

void test_parsear_ultima_ronda_y_season(void) {
  static const char* PAST = R"({"events":[
    {"strEvent":"Aragón GP","dateEvent":"2026-08-30","intRound":"13","strSeason":"2026"}
  ]})";
  int r = 0; std::string s;
  TEST_ASSERT_TRUE(MotogpClient::parsearUltimaRondaYSeason(PAST, r, s));
  TEST_ASSERT_EQUAL(13, r);
  TEST_ASSERT_EQUAL_STRING("2026", s.c_str());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_parsear_eventos_past);
  RUN_TEST(test_parsear_eventos_proximos);
  RUN_TEST(test_parsear_json_malformado);
  RUN_TEST(test_parsear_respeta_maxN);
  RUN_TEST(test_extraer_pais_de_sesion);
  RUN_TEST(test_parsear_ronda_como_gp);
  RUN_TEST(test_parsear_ultima_ronda_y_season);
  RUN_TEST(test_parsear_clasificacion_motogp);
  return UNITY_END();
}
