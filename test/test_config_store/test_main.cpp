#include <unity.h>
#include "config_store.h"
#include <vector>
#include <cstdint>

void test_serializar_y_deserializar_roundtrip(void) {
  Config in;
  in.ssid = "MiWifi";
  in.password = "secreto123";
  in.direccion = "Calle Mayor 1, Madrid";
  in.lat = 40.4168;
  in.lon = -3.7038;
  in.radio_km = 25;

  std::vector<uint8_t> buf;
  ConfigStore::serializar(in, buf);
  TEST_ASSERT_TRUE(buf.size() > 0);

  Config out;
  bool ok = ConfigStore::deserializar(buf.data(), buf.size(), out);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL_STRING("MiWifi", out.ssid.c_str());
  TEST_ASSERT_EQUAL_STRING("secreto123", out.password.c_str());
  TEST_ASSERT_EQUAL_STRING("Calle Mayor 1, Madrid", out.direccion.c_str());
  TEST_ASSERT_FLOAT_WITHIN(0.0001, 40.4168, out.lat);
  TEST_ASSERT_FLOAT_WITHIN(0.0001, -3.7038, out.lon);
  TEST_ASSERT_EQUAL(25, out.radio_km);
}

void test_deserializar_magic_byte_erroneo(void) {
  std::vector<uint8_t> buf(64, 0xFF);   // simula NVS vacía o corrupta
  Config out;
  bool ok = ConfigStore::deserializar(buf.data(), buf.size(), out);
  TEST_ASSERT_FALSE(ok);
}

void test_deserializar_buffer_muy_pequeno(void) {
  uint8_t buf[2] = {0xC0, 0xDE};
  Config out;
  bool ok = ConfigStore::deserializar(buf, sizeof(buf), out);
  TEST_ASSERT_FALSE(ok);
}

void test_deserializar_buffer_nulo(void) {
  Config out;
  bool ok = ConfigStore::deserializar(nullptr, 0, out);
  TEST_ASSERT_FALSE(ok);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_serializar_y_deserializar_roundtrip);
  RUN_TEST(test_deserializar_magic_byte_erroneo);
  RUN_TEST(test_deserializar_buffer_muy_pequeno);
  RUN_TEST(test_deserializar_buffer_nulo);
  return UNITY_END();
}
