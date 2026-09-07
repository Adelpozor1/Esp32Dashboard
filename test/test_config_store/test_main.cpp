#include <unity.h>
#include "config_store.h"
#include <vector>
#include <cstdint>

void test_serializar_y_deserializar_v2_roundtrip(void) {
  Config in;
  in.ssid = "MiWifi";
  in.password = "secreto123";
  in.direccion = "Calle Mayor 1, Madrid";
  in.lat = 40.4168;
  in.lon = -3.7038;
  in.radio_km = 25;
  in.modo = ModoVista::FIJO;
  in.intervalo_carrusel_s = 30;
  in.vista_fija = 2;
  in.vistas_orden = {0, 2, 5};
  in.touch_min_x = 200;
  in.touch_max_x = 3800;
  in.touch_min_y = 240;
  in.touch_max_y = 3900;
  in.touch_calibrado = true;

  std::vector<uint8_t> buf;
  ConfigStore::serializar(in, buf);
  TEST_ASSERT_TRUE(buf.size() > 0);

  Config out;
  bool ok = ConfigStore::deserializar(buf.data(), buf.size(), out);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL_STRING("MiWifi", out.ssid.c_str());
  TEST_ASSERT_EQUAL(25, out.radio_km);
  TEST_ASSERT_EQUAL(static_cast<int>(ModoVista::FIJO), static_cast<int>(out.modo));
  TEST_ASSERT_EQUAL(30, out.intervalo_carrusel_s);
  TEST_ASSERT_EQUAL(2, out.vista_fija);
  TEST_ASSERT_EQUAL(3, (int)out.vistas_orden.size());
  TEST_ASSERT_EQUAL(0, out.vistas_orden[0]);
  TEST_ASSERT_EQUAL(2, out.vistas_orden[1]);
  TEST_ASSERT_EQUAL(5, out.vistas_orden[2]);
  TEST_ASSERT_EQUAL(200, out.touch_min_x);
  TEST_ASSERT_EQUAL(3800, out.touch_max_x);
  TEST_ASSERT_TRUE(out.touch_calibrado);
  TEST_ASSERT_EQUAL_STRING("secreto123", out.password.c_str());
  TEST_ASSERT_EQUAL_STRING("Calle Mayor 1, Madrid", out.direccion.c_str());
  TEST_ASSERT_FLOAT_WITHIN(0.0001, 40.4168, out.lat);
  TEST_ASSERT_FLOAT_WITHIN(0.0001, -3.7038, out.lon);
  TEST_ASSERT_EQUAL(240, out.touch_min_y);
  TEST_ASSERT_EQUAL(3900, out.touch_max_y);
}

void test_deserializar_buffer_v1_rellena_defaults_v2(void) {
  // Construimos manualmente un buffer v1 (magic 0xC0DE, version 1, lat, lon,
  // radio, ssid, password, direccion). Sin campos v2.
  std::vector<uint8_t> buf;
  // magic 0xC0DE
  buf.push_back(0xDE); buf.push_back(0xC0);
  // version 1
  buf.push_back(0x01);
  // lat = 41.0 (double LE)
  double lat = 41.0, lon = 2.0;
  const uint8_t* platlat = reinterpret_cast<const uint8_t*>(&lat);
  buf.insert(buf.end(), platlat, platlat + sizeof(double));
  const uint8_t* platlon = reinterpret_cast<const uint8_t*>(&lon);
  buf.insert(buf.end(), platlon, platlon + sizeof(double));
  // radio 30 (u16 LE)
  buf.push_back(30); buf.push_back(0);
  // ssid "Red" (u16 len + bytes)
  const char* ssid = "Red";
  buf.push_back(3); buf.push_back(0);
  buf.insert(buf.end(), ssid, ssid + 3);
  // password "clave"
  const char* pass = "clave";
  buf.push_back(5); buf.push_back(0);
  buf.insert(buf.end(), pass, pass + 5);
  // direccion "BCN"
  const char* dir = "BCN";
  buf.push_back(3); buf.push_back(0);
  buf.insert(buf.end(), dir, dir + 3);

  Config out;
  bool ok = ConfigStore::deserializar(buf.data(), buf.size(), out);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL_STRING("Red", out.ssid.c_str());
  TEST_ASSERT_EQUAL_STRING("clave", out.password.c_str());
  TEST_ASSERT_EQUAL_STRING("BCN", out.direccion.c_str());
  TEST_ASSERT_EQUAL(30, out.radio_km);
  // Defaults v2:
  TEST_ASSERT_EQUAL(static_cast<int>(ModoVista::CARRUSEL), static_cast<int>(out.modo));
  TEST_ASSERT_EQUAL(10, out.intervalo_carrusel_s);
  TEST_ASSERT_EQUAL(0, out.vista_fija);
  // Reloj (id=1) eliminado — defaults ahora {0, 2, 3, 4, 5}.
  TEST_ASSERT_EQUAL(5, (int)out.vistas_orden.size());
  TEST_ASSERT_FALSE(out.touch_calibrado);
}

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

void test_deserializar_modo_invalido_rechaza(void) {
  // Un buffer v2 mínimo y válido salvo por el byte de modo = 0x99.
  Config in;
  in.ssid = "x"; in.password = "y"; in.direccion = "z";
  std::vector<uint8_t> buf;
  ConfigStore::serializar(in, buf);
  // Recorremos hasta el byte de modo: 2 (magic) + 1 (version) + 8 (lat) + 8 (lon)
  // + 2 (radio) + 2+1 (ssid) + 2+1 (pass) + 2+1 (dir) = 30
  TEST_ASSERT_TRUE(buf.size() > 30);
  buf[30] = 0x99;
  Config out;
  bool ok = ConfigStore::deserializar(buf.data(), buf.size(), out);
  TEST_ASSERT_FALSE(ok);
}

void test_deserializar_vista_orden_fuera_de_rango_rechaza(void) {
  // Serializamos un Config con un id inválido (99). El serializador acepta cualquier
  // uint8_t (no tiene por qué validar el dominio), pero el deserializador SÍ debe
  // rechazarlo — es la puerta de entrada desde NVS al resto del sistema.
  Config in;
  in.ssid = "x"; in.password = "y"; in.direccion = "z";
  in.vistas_orden = {0, 99};
  std::vector<uint8_t> buf;
  ConfigStore::serializar(in, buf);
  Config out;
  bool ok = ConfigStore::deserializar(buf.data(), buf.size(), out);
  TEST_ASSERT_FALSE(ok);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_serializar_y_deserializar_roundtrip);
  RUN_TEST(test_deserializar_magic_byte_erroneo);
  RUN_TEST(test_deserializar_buffer_muy_pequeno);
  RUN_TEST(test_deserializar_buffer_nulo);
  RUN_TEST(test_serializar_y_deserializar_v2_roundtrip);
  RUN_TEST(test_deserializar_buffer_v1_rellena_defaults_v2);
  RUN_TEST(test_deserializar_modo_invalido_rechaza);
  RUN_TEST(test_deserializar_vista_orden_fuera_de_rango_rechaza);
  return UNITY_END();
}
