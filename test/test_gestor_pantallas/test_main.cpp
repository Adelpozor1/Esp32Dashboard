#include <unity.h>
#include "gestor_pantallas.h"
#include "pantalla.h"
#include "renderizador_ui.h"

using pantallas::EventoUi;
using pantallas::GestorPantallas;
using pantallas::IRenderizadorUi;
using pantallas::ModoGestor;
using pantallas::Pantalla;
using pantallas::TipoEventoUi;

class PantallaFake : public Pantalla {
 public:
  const char* nombreFake;
  uint8_t idFake;
  int nEntradas = 0, nSalidas = 0, nDibujos = 0;
  int ultTapX = -1, ultTapY = -1, ultSwipe = 0;
  PantallaFake(const char* n, uint8_t i) : nombreFake(n), idFake(i) {}
  const char* nombre() const override { return nombreFake; }
  uint8_t id() const override { return idFake; }
  void alEntrar() override { ++nEntradas; }
  void alSalir() override { ++nSalidas; }
  void alTocar(int x, int y) override { ultTapX = x; ultTapY = y; }
  void alDeslizar(int d) override { ultSwipe = d; }
  void dibujar(uint32_t) override { ++nDibujos; }
};

class RendererFake : public IRenderizadorUi {
 public:
  int nBarras = 0, nLimpiezas = 0;
  uint8_t ultDot = 0, ultN = 0;
  void pintarBarraSuperior(const char*, uint8_t d, uint8_t n) override {
    ++nBarras; ultDot = d; ultN = n;
  }
  void limpiarAreaContenido() override { ++nLimpiezas; }
};

void test_modo_carrusel_rota_al_pasar_el_intervalo(void) {
  RendererFake r;
  PantallaFake a("A", 0), b("B", 1), c("C", 2);
  GestorPantallas g(r);
  g.registrar(&a); g.registrar(&b); g.registrar(&c);
  g.configurarModo(ModoGestor::CARRUSEL, /*intervaloS=*/5,
                   /*vistasOrden=*/{0, 1, 2}, /*idFija=*/0);
  g.iniciar(/*msAhora=*/0);

  TEST_ASSERT_EQUAL(1, a.nEntradas);
  g.tick(1000); TEST_ASSERT_EQUAL(0, b.nEntradas);
  g.tick(5001); TEST_ASSERT_EQUAL(1, a.nSalidas);
  TEST_ASSERT_EQUAL(1, b.nEntradas);
  g.tick(10002); TEST_ASSERT_EQUAL(1, c.nEntradas);
  g.tick(15003); TEST_ASSERT_EQUAL(2, a.nEntradas);  // vuelta al principio
}

void test_swipe_manual_adelanta_y_resetea_timer(void) {
  RendererFake r;
  PantallaFake a("A", 0), b("B", 1);
  GestorPantallas g(r);
  g.registrar(&a); g.registrar(&b);
  g.configurarModo(ModoGestor::CARRUSEL, 5, {0, 1}, 0);
  g.iniciar(0);

  g.tick(2000);
  g.encolarEvento({TipoEventoUi::SWIPE_IZQUIERDA, 0, 0});
  g.tick(2000);  // procesa evento
  TEST_ASSERT_EQUAL(1, b.nEntradas);

  // Timer reseteado: 3 s más NO debería rotar (necesitamos otros 5).
  g.tick(5000);
  TEST_ASSERT_EQUAL(0, a.nEntradas - 1);  // a.nEntradas sigue en 1
  g.tick(7500);  // 5.5 s desde el swipe -> rota
  TEST_ASSERT_EQUAL(2, a.nEntradas);
}

void test_modo_fijo_ignora_timer_y_swipe(void) {
  RendererFake r;
  PantallaFake a("A", 0), b("B", 1);
  GestorPantallas g(r);
  g.registrar(&a); g.registrar(&b);
  g.configurarModo(ModoGestor::FIJO, 5, {0, 1}, /*idFija=*/1);
  g.iniciar(0);
  TEST_ASSERT_EQUAL(1, b.nEntradas);
  g.tick(60000);
  TEST_ASSERT_EQUAL(1, b.nEntradas);  // ninguna nueva entrada
  g.encolarEvento({TipoEventoUi::SWIPE_IZQUIERDA, 0, 0});
  g.tick(60000);
  TEST_ASSERT_EQUAL(0, a.nEntradas);
}

void test_tap_en_zona_menu_vuelve_a_home_y_vacia_pila(void) {
  RendererFake r;
  PantallaFake home("Home", 10);
  PantallaFake radar("Radar", 0);
  PantallaFake sub("Sub", 99);
  GestorPantallas g(r);
  g.setHome(&home);
  g.registrar(&home); g.registrar(&radar); g.registrar(&sub);
  g.configurarModo(ModoGestor::CARRUSEL, 5, {0}, 0);
  g.iniciar(0);
  g.mostrarPorId(0);       // entra en radar
  g.abrirEnPila(&sub);     // push sub
  TEST_ASSERT_EQUAL(1, sub.nEntradas);

  // Tap en (10, 10) — zona del botón menú (0..40 × 0..20).
  g.encolarEvento({TipoEventoUi::TAP, 10, 10});
  g.tick(1000);
  TEST_ASSERT_EQUAL(1, sub.nSalidas);
  // Con modo CARRUSEL, iniciar() arranca en orden[0]=radar (no en home),
  // así que home sólo entra 1 vez: la del tap en el botón menú.
  TEST_ASSERT_EQUAL(1, home.nEntradas);
}

void test_pila_ui_pausa_el_carrusel(void) {
  RendererFake r;
  PantallaFake home("Home", 10);
  PantallaFake a("A", 0), b("B", 1);
  PantallaFake sub("Sub", 99);
  GestorPantallas g(r);
  g.setHome(&home);
  g.registrar(&home); g.registrar(&a); g.registrar(&b);
  g.configurarModo(ModoGestor::CARRUSEL, 5, {0, 1}, 0);
  g.iniciar(0);
  g.mostrarPorId(0);
  g.abrirEnPila(&sub);
  g.tick(20000);  // mucho tiempo, pero sub está en la pila
  TEST_ASSERT_EQUAL(0, b.nEntradas);
  g.volverAtras();
  TEST_ASSERT_EQUAL(1, sub.nSalidas);
  // Timer resetea al volver → 5 s más para rotar.
  g.tick(20500);
  TEST_ASSERT_EQUAL(0, b.nEntradas);
  g.tick(25500);
  TEST_ASSERT_EQUAL(1, b.nEntradas);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_modo_carrusel_rota_al_pasar_el_intervalo);
  RUN_TEST(test_swipe_manual_adelanta_y_resetea_timer);
  RUN_TEST(test_modo_fijo_ignora_timer_y_swipe);
  RUN_TEST(test_tap_en_zona_menu_vuelve_a_home_y_vacia_pila);
  RUN_TEST(test_pila_ui_pausa_el_carrusel);
  return UNITY_END();
}
