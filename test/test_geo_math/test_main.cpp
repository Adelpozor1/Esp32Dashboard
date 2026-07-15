#include <unity.h>
#include "geo_math.h"
#include <cmath>

// Madrid (Puerta del Sol) → Barcelona (Sagrada Familia): ~505 km, bearing ~62°
constexpr double MAD_LAT = 40.4168;
constexpr double MAD_LON = -3.7038;
constexpr double BCN_LAT = 41.4036;
constexpr double BCN_LON = 2.1744;

void test_distancia_madrid_barcelona(void) {
  double d = geo::distanciaKm(MAD_LAT, MAD_LON, BCN_LAT, BCN_LON);
  TEST_ASSERT_FLOAT_WITHIN(10.0, 505.0, d);
}

void test_distancia_punto_sobre_si_mismo(void) {
  double d = geo::distanciaKm(MAD_LAT, MAD_LON, MAD_LAT, MAD_LON);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0, d);
}

void test_bearing_norte_puro(void) {
  int b = geo::bearingGrados(0.0, 0.0, 1.0, 0.0);
  TEST_ASSERT_INT_WITHIN(1, 0, b);
}

void test_bearing_este_puro(void) {
  int b = geo::bearingGrados(0.0, 0.0, 0.0, 1.0);
  TEST_ASSERT_INT_WITHIN(1, 90, b);
}

void test_bearing_sur_puro(void) {
  int b = geo::bearingGrados(0.0, 0.0, -1.0, 0.0);
  TEST_ASSERT_INT_WITHIN(1, 180, b);
}

void test_bearing_oeste_puro(void) {
  int b = geo::bearingGrados(0.0, 0.0, 0.0, -1.0);
  TEST_ASSERT_INT_WITHIN(1, 270, b);
}

void test_bearing_madrid_barcelona(void) {
  int b = geo::bearingGrados(MAD_LAT, MAD_LON, BCN_LAT, BCN_LON);
  TEST_ASSERT_INT_WITHIN(2, 75, b);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_distancia_madrid_barcelona);
  RUN_TEST(test_distancia_punto_sobre_si_mismo);
  RUN_TEST(test_bearing_norte_puro);
  RUN_TEST(test_bearing_este_puro);
  RUN_TEST(test_bearing_sur_puro);
  RUN_TEST(test_bearing_oeste_puro);
  RUN_TEST(test_bearing_madrid_barcelona);
  return UNITY_END();
}
