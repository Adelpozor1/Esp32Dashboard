#include <unity.h>
#include "geocoder.h"
#include "mock_http_client.h"

// Respuesta típica de Nominatim con resultado
static const char* RESP_MADRID = R"([
  {
    "place_id": 1,
    "lat": "40.4167047",
    "lon": "-3.7035825",
    "display_name": "Madrid, España"
  }
])";

// Respuesta vacía (dirección no encontrada)
static const char* RESP_VACIA = R"([])";

// Respuesta malformada
static const char* RESP_MAL = "esto no es json";

void test_geocoding_direccion_encontrada(void) {
  MockHttpClient http;
  http.respuestas.push_back({"nominatim", RESP_MADRID, 200, true});
  Geocoder g(http);
  double lat = 0, lon = 0;
  bool ok = g.resolver("Madrid, España", lat, lon);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 40.4167, lat);
  TEST_ASSERT_FLOAT_WITHIN(0.01, -3.7036, lon);
}

void test_geocoding_direccion_no_encontrada(void) {
  MockHttpClient http;
  http.respuestas.push_back({"nominatim", RESP_VACIA, 200, true});
  Geocoder g(http);
  double lat = 99, lon = 99;
  bool ok = g.resolver("Asdfghjkl", lat, lon);
  TEST_ASSERT_FALSE(ok);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 99.0, lat);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 99.0, lon);
}

void test_geocoding_respuesta_malformada(void) {
  MockHttpClient http;
  http.respuestas.push_back({"nominatim", RESP_MAL, 200, true});
  Geocoder g(http);
  double lat = 0, lon = 0;
  bool ok = g.resolver("Cualquier cosa", lat, lon);
  TEST_ASSERT_FALSE(ok);
}

void test_geocoding_fallo_http(void) {
  MockHttpClient http;
  http.respuestas.push_back({"nominatim", "", 0, false});
  Geocoder g(http);
  double lat = 0, lon = 0;
  bool ok = g.resolver("Madrid", lat, lon);
  TEST_ASSERT_FALSE(ok);
}

void test_geocoding_incluye_direccion_url_encoded(void) {
  MockHttpClient http;
  http.respuestas.push_back({"nominatim", RESP_MADRID, 200, true});
  Geocoder g(http);
  double lat = 0, lon = 0;
  g.resolver("Calle Mayor 1, Madrid", lat, lon);
  TEST_ASSERT_EQUAL(1u, http.urlsLlamadas.size());
  const std::string& url = http.urlsLlamadas[0];
  TEST_ASSERT_TRUE(url.find("Calle%20Mayor%201") != std::string::npos ||
                   url.find("Calle+Mayor+1") != std::string::npos);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_geocoding_direccion_encontrada);
  RUN_TEST(test_geocoding_direccion_no_encontrada);
  RUN_TEST(test_geocoding_respuesta_malformada);
  RUN_TEST(test_geocoding_fallo_http);
  RUN_TEST(test_geocoding_incluye_direccion_url_encoded);
  return UNITY_END();
}
