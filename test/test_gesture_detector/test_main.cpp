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

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_tap_corto_produce_evento_tap);
  RUN_TEST(test_pulsacion_larga_no_produce_evento);
  RUN_TEST(test_desplazamiento_grande_no_es_tap);
  RUN_TEST(test_swipe_derecha_produce_evento);
  RUN_TEST(test_swipe_izquierda_produce_evento);
  RUN_TEST(test_swipe_vertical_no_produce_evento);
  RUN_TEST(test_swipe_muy_lento_no_produce_evento);
  return UNITY_END();
}
