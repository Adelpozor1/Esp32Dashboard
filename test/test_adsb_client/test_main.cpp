#include <unity.h>
#include "adsb_client.h"
#include "mock_http_client.h"

// Muestra reducida de la respuesta de ADSB.lol /v2/point/{lat}/{lon}/{radio}
// Estructura real: { "ac": [ {...}, {...} ], "msg": "No error", ... }
static const char* RESP_DOS_AVIONES = R"({
  "ac": [
    {
      "hex": "4b1806",
      "flight": "IBE3456 ",
      "lat": 40.45,
      "lon": -3.68,
      "alt_baro": 8500,
      "gs": 320.5,
      "track": 85.2
    },
    {
      "hex": "406b1a",
      "flight": "BAW789  ",
      "lat": 40.30,
      "lon": -3.60,
      "alt_baro": 12000,
      "gs": 410,
      "track": 190
    }
  ]
})";

static const char* RESP_VACIA = R"({"ac":[]})";
static const char* RESP_MAL = "no es json";

// Punto de referencia: Madrid Puerta del Sol.
constexpr double CENTRO_LAT = 40.4168;
constexpr double CENTRO_LON = -3.7038;

void test_adsb_parseo_normal(void) {
  MockHttpClient http;
  http.respuestas.push_back({"adsb.lol", RESP_DOS_AVIONES, 200, true});
  AdsbClient c(http);
  std::vector<Aeronave> aviones;
  bool ok = c.fetchCerca(CENTRO_LAT, CENTRO_LON, 25, aviones);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL(2u, aviones.size());
  TEST_ASSERT_EQUAL_STRING("4b1806", aviones[0].hex.c_str());
  TEST_ASSERT_EQUAL_STRING("IBE3456", aviones[0].callsign.c_str());   // trimmed
  TEST_ASSERT_EQUAL(8500, aviones[0].alt_ft);
  TEST_ASSERT_EQUAL(320, aviones[0].gs_kt);
  TEST_ASSERT_EQUAL(85, aviones[0].track_deg);
  TEST_ASSERT_FLOAT_WITHIN(1.0, 4.5, aviones[0].dist_km);   // aproximado
  TEST_ASSERT_INT_WITHIN(20, 29, aviones[0].bearing);       // NE aprox.
}

void test_adsb_respuesta_vacia(void) {
  MockHttpClient http;
  http.respuestas.push_back({"adsb.lol", RESP_VACIA, 200, true});
  AdsbClient c(http);
  std::vector<Aeronave> aviones;
  bool ok = c.fetchCerca(CENTRO_LAT, CENTRO_LON, 25, aviones);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL(0u, aviones.size());
}

void test_adsb_respuesta_malformada(void) {
  MockHttpClient http;
  http.respuestas.push_back({"adsb.lol", RESP_MAL, 200, true});
  AdsbClient c(http);
  std::vector<Aeronave> aviones;
  bool ok = c.fetchCerca(CENTRO_LAT, CENTRO_LON, 25, aviones);
  TEST_ASSERT_FALSE(ok);
}

void test_adsb_fallo_http(void) {
  MockHttpClient http;
  http.respuestas.push_back({"adsb.lol", "", 0, false});
  AdsbClient c(http);
  std::vector<Aeronave> aviones;
  bool ok = c.fetchCerca(CENTRO_LAT, CENTRO_LON, 25, aviones);
  TEST_ASSERT_FALSE(ok);
}

void test_adsb_avion_sin_posicion_se_descarta(void) {
  static const char* SIN_POS = R"({"ac":[{"hex":"aaa","flight":"XXX"}]})";
  MockHttpClient http;
  http.respuestas.push_back({"adsb.lol", SIN_POS, 200, true});
  AdsbClient c(http);
  std::vector<Aeronave> aviones;
  bool ok = c.fetchCerca(CENTRO_LAT, CENTRO_LON, 25, aviones);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL(0u, aviones.size());
}

void test_adsb_trunca_a_50_por_distancia(void) {
  // Genera 60 aviones a distancias crecientes; espera 50, ordenados por dist.
  std::string body = "{\"ac\":[";
  for (int i = 0; i < 60; ++i) {
    if (i > 0) body += ",";
    char buf[128];
    // cada avión a 0.01° más al norte → distancias crecientes en km
    std::snprintf(buf, sizeof(buf),
                  "{\"hex\":\"a%03d\",\"flight\":\"F%d\",\"lat\":%f,\"lon\":%f,\"alt_baro\":10000,\"gs\":300,\"track\":0}",
                  i, i, CENTRO_LAT + 0.01 * (i + 1), CENTRO_LON);
    body += buf;
  }
  body += "]}";
  MockHttpClient http;
  http.respuestas.push_back({"adsb.lol", body, 200, true});
  AdsbClient c(http);
  std::vector<Aeronave> aviones;
  bool ok = c.fetchCerca(CENTRO_LAT, CENTRO_LON, 100, aviones);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL(50u, aviones.size());
  // deben estar ordenados por distancia asc
  for (size_t i = 1; i < aviones.size(); ++i) {
    TEST_ASSERT_TRUE(aviones[i - 1].dist_km <= aviones[i].dist_km);
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_adsb_parseo_normal);
  RUN_TEST(test_adsb_respuesta_vacia);
  RUN_TEST(test_adsb_respuesta_malformada);
  RUN_TEST(test_adsb_fallo_http);
  RUN_TEST(test_adsb_avion_sin_posicion_se_descarta);
  RUN_TEST(test_adsb_trunca_a_50_por_distancia);
  return UNITY_END();
}
