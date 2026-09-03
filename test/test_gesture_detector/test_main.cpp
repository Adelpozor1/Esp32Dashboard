#include <unity.h>
#include "gesture_detector.h"

using touch::EventoTactil;
using touch::GestureDetector;
using touch::TipoEvento;

void test_tap_corto_produce_evento_tap(void) {
  GestureDetector g;
  g.onPress(100, 120, /*ms=*/1000);
  auto ev = g.onRelease(102, 118, /*ms=*/1100);
  TEST_ASSERT_TRUE(ev.has_value());
  TEST_ASSERT_EQUAL(static_cast<int>(TipoEvento::TAP), static_cast<int>(ev->tipo));
  TEST_ASSERT_EQUAL(100, ev->x);
  TEST_ASSERT_EQUAL(120, ev->y);
}

void test_pulsacion_larga_no_produce_evento(void) {
  GestureDetector g;
  g.onPress(50, 50, 1000);
  auto ev = g.onRelease(50, 50, 1400);  // 400 ms > umbral tap 300 ms
  TEST_ASSERT_FALSE(ev.has_value());
}

void test_desplazamiento_grande_no_es_tap(void) {
  GestureDetector g;
  g.onPress(50, 50, 1000);
  auto ev = g.onRelease(80, 60, 1200);  // dx=30 > umbral 20
  TEST_ASSERT_FALSE(ev.has_value());
}

void test_swipe_derecha_produce_evento(void) {
  GestureDetector g;
  g.onPress(20, 100, 1000);
  auto ev = g.onRelease(120, 105, 1200);  // dx=100 en 200 ms, dy pequeño
  TEST_ASSERT_TRUE(ev.has_value());
  TEST_ASSERT_EQUAL(static_cast<int>(TipoEvento::SWIPE_DERECHA), static_cast<int>(ev->tipo));
}

void test_swipe_izquierda_produce_evento(void) {
  GestureDetector g;
  g.onPress(200, 100, 1000);
  auto ev = g.onRelease(100, 100, 1200);
  TEST_ASSERT_TRUE(ev.has_value());
  TEST_ASSERT_EQUAL(static_cast<int>(TipoEvento::SWIPE_IZQUIERDA), static_cast<int>(ev->tipo));
}

void test_swipe_vertical_no_produce_evento(void) {
  GestureDetector g;
  g.onPress(100, 20, 1000);
  auto ev = g.onRelease(105, 180, 1200);  // dy grande, dx pequeño
  TEST_ASSERT_FALSE(ev.has_value());
}

void test_swipe_muy_lento_no_produce_evento(void) {
  GestureDetector g;
  g.onPress(20, 100, 1000);
  auto ev = g.onRelease(120, 100, 1600);  // 600 ms > umbral 400 ms
  TEST_ASSERT_FALSE(ev.has_value());
}

void test_release_sin_press_devuelve_nullopt(void) {
  GestureDetector g;
  auto ev = g.onRelease(100, 100, 1000);
  TEST_ASSERT_FALSE(ev.has_value());
}

void test_release_doble_consecutivo_devuelve_nullopt(void) {
  GestureDetector g;
  g.onPress(50, 50, 1000);
  auto ev1 = g.onRelease(50, 50, 1100);
  TEST_ASSERT_TRUE(ev1.has_value());
  auto ev2 = g.onRelease(50, 50, 1200);  // sin nuevo press
  TEST_ASSERT_FALSE(ev2.has_value());
}

void test_dead_zone_entre_tap_y_swipe(void) {
  // dx=40 supera el umbral TAP (20 estricto) pero no llega al SWIPE (60 estricto):
  // el detector debe devolver nullopt para no confundir dos gestos.
  GestureDetector g;
  g.onPress(50, 100, 1000);
  auto ev = g.onRelease(90, 100, 1100);
  TEST_ASSERT_FALSE(ev.has_value());
}

void test_bordes_exactos_de_umbral_son_estrictos(void) {
  // dur=300 no es TAP (< estricto), |dx|=60 no es SWIPE (> estricto).
  GestureDetector g1;
  g1.onPress(50, 50, 1000);
  auto ev1 = g1.onRelease(50, 50, 1300);   // dur exactamente 300 ms
  TEST_ASSERT_FALSE(ev1.has_value());

  GestureDetector g2;
  g2.onPress(50, 100, 1000);
  auto ev2 = g2.onRelease(110, 100, 1100);  // dx exactamente 60
  TEST_ASSERT_FALSE(ev2.has_value());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_tap_corto_produce_evento_tap);
  RUN_TEST(test_pulsacion_larga_no_produce_evento);
  RUN_TEST(test_desplazamiento_grande_no_es_tap);
  RUN_TEST(test_swipe_derecha_produce_evento);
  RUN_TEST(test_swipe_izquierda_produce_evento);
  RUN_TEST(test_swipe_vertical_no_produce_evento);
  RUN_TEST(test_swipe_muy_lento_no_produce_evento);
  RUN_TEST(test_release_sin_press_devuelve_nullopt);
  RUN_TEST(test_release_doble_consecutivo_devuelve_nullopt);
  RUN_TEST(test_dead_zone_entre_tap_y_swipe);
  RUN_TEST(test_bordes_exactos_de_umbral_son_estrictos);
  return UNITY_END();
}
