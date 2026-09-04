#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "config_store.h"
#include "http_client.h"
#include "adsb_client.h"
#include "radar_state.h"
#include "wifi_portal.h"
#include "web_server.h"
#include "status_led.h"
#include "tft_driver.h"
#include "touch.h"

#include "pantalla.h"
#include "gestor_pantallas.h"
#include "renderizador_ui.h"
#include "pantalla_radar.h"
#include "pantalla_menu.h"
#include "pantalla_proximamente.h"
#include "pantalla_reloj.h"
#include "pantalla_meteo.h"
#include "meteo_client.h"
#include "pantalla_futbol.h"
#include "futbol_client.h"
#include "pantalla_motogp.h"
#include "motogp_client.h"
#include "pantalla_f1.h"
#include "f1_client.h"
#include "pantalla_ajustes.h"
#include "pantalla_intervalo.h"
#include "pantalla_seleccion_vistas.h"
#include "pantalla_seleccion_vista_fija.h"
#include "pantalla_config_localizacion.h"
#include "pantalla_calibrar_touch.h"
#include "pantalla_confirmar_reset.h"

#include <TFT_eSPI.h>

namespace {

WifiHttpClient  g_http;
Config          g_cfg;
RadarState*     g_estado = nullptr;
uint32_t        g_ultimoIntentoWifiMs = 0;
MeteoSnapshot   g_snapMeteo;
FutbolSnapshot  g_snapFutbol;
MotogpSnapshot  g_snapMotogp;
F1Snapshot      g_snapF1;

// Renderer real: barra superior con "menú" a la izquierda, título centrado, dots
// del carrusel abajo-derecha del área de contenido.
class RenderizadorUiReal : public pantallas::IRenderizadorUi {
 public:
  void pintarBarraSuperior(const char* titulo, uint8_t dotAct, uint8_t nDots) override {
    auto& tft = tft_driver::obtenerTft();
    tft.fillRect(0, 0, 320, 20, 0x0000);
    // Botón menú
    tft.setTextColor(0x07E0, 0x0000);
    tft.setTextFont(2);
    tft.setCursor(4, 2);
    tft.print("[=]");
    // Título centrado
    int16_t ancho = tft.textWidth(titulo);
    tft.setCursor((320 - ancho) / 2, 2);
    tft.print(titulo);
    // Dots
    if (nDots > 0) {
      const int xIni = 320 - (nDots * 10) - 4;
      for (int i = 0; i < nDots; ++i) {
        if (i == dotAct) tft.fillCircle(xIni + i * 10, 232, 3, 0x07E0);
        else             tft.drawCircle(xIni + i * 10, 232, 3, 0x07E0);
      }
    }
  }
  void limpiarAreaContenido() override {
    tft_driver::obtenerTft().fillRect(0, 20, 320, 220, 0x0000);
  }
};

RenderizadorUiReal g_renderer;
pantallas::GestorPantallas* g_gestor = nullptr;

bool conectarWifi() {
  StatusLed::setEstado(EstadoLed::CONECTANDO_WIFI);
  tft_driver::pintarSplash("Conectando WiFi", g_cfg.ssid);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(g_cfg.ssid.c_str(), g_cfg.password.c_str());
  uint32_t inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 20000) delay(200);
  if (WiFi.status() != WL_CONNECTED) return false;
  Serial.printf("[wifi] conectado, IP: %s\n", WiFi.localIP().toString().c_str());
  Serial.println("[wifi] arrancando NTP + TZ Europe/Madrid");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  return true;
}

void tareaPoller(void*) {
  AdsbClient cliente(g_http);
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      g_estado->marcarStale();
      StatusLed::setEstado(EstadoLed::RADAR_ERROR);
      if (millis() - g_ultimoIntentoWifiMs > 60000) {
        Serial.println("[wifi] 60s sin conexion, reiniciando");
        ESP.restart();
      }
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
    g_ultimoIntentoWifiMs = millis();
    std::vector<Aeronave> aviones;
    bool ok = cliente.fetchCerca(g_cfg.lat, g_cfg.lon, g_cfg.radio_km, aviones);
    if (!ok) ok = cliente.fetchCerca(g_cfg.lat, g_cfg.lon, g_cfg.radio_km, aviones);
    if (ok) {
      g_estado->actualizar(aviones, millis());
      StatusLed::setEstado(EstadoLed::RADAR_OK);
    } else {
      g_estado->marcarStale();
      StatusLed::setEstado(EstadoLed::RADAR_ERROR);
    }
    vTaskDelay(pdMS_TO_TICKS(3000));
  }
}

void tareaDisplay(void*) {
  for (;;) {
    // Reencolar cualquier evento tactil pendiente en el gestor.
    touch::EventoTactil ev;
    while (touch::esperarEvento(ev, /*timeoutMs=*/0)) {
      pantallas::EventoUi u;
      switch (ev.tipo) {
        case touch::TipoEvento::TAP:              u.tipo = pantallas::TipoEventoUi::TAP; break;
        case touch::TipoEvento::SWIPE_IZQUIERDA:  u.tipo = pantallas::TipoEventoUi::SWIPE_IZQUIERDA; break;
        case touch::TipoEvento::SWIPE_DERECHA:    u.tipo = pantallas::TipoEventoUi::SWIPE_DERECHA; break;
        case touch::TipoEvento::SWIPE_ARRIBA:     u.tipo = pantallas::TipoEventoUi::SWIPE_ARRIBA; break;
        case touch::TipoEvento::SWIPE_ABAJO:      u.tipo = pantallas::TipoEventoUi::SWIPE_ABAJO;  break;
      }
      u.x = ev.x; u.y = ev.y;
      g_gestor->encolarEvento(u);
    }
    g_gestor->tick(millis());
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

void tareaMeteoRefresh(void*) {
  MeteoClient cliente(g_http);
  // Gracia inicial para que NTP y WiFi acaben de asentarse.
  vTaskDelay(pdMS_TO_TICKS(5000));
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      MeteoSnapshot nuevo;
      if (cliente.fetch(g_cfg.lat, g_cfg.lon, nuevo)) {
        nuevo.obtenido_ms = millis();
        nuevo.stale = false;
        g_snapMeteo = nuevo;
        Serial.printf("[meteo] refresh OK t=%.1fC codigo=%d\n",
                      g_snapMeteo.temp_actual_c, g_snapMeteo.codigo_actual);
      } else {
        g_snapMeteo.stale = true;
        Serial.println("[meteo] refresh FALLO");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(30UL * 60UL * 1000UL));  // 30 min
  }
}

// Los 3 clientes deportivos usan HTTPS con WiFiClientSecure (~35 KB heap durante
// el handshake). Ejecutarlos en tasks concurrentes sobrecarga el heap y dispara
// el WDT. Se agrupan en una sola task secuencial con pausas para que el heap
// se libere entre peticiones.
void tareaDeportesRefresh(void*) {
  FutbolClient cliFut(g_http);
  MotogpClient cliMot(g_http);
  F1Client     cliF1(g_http);
  vTaskDelay(pdMS_TO_TICKS(15000));   // gracia inicial (deja que meteo acabe)
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      // --- Fútbol ---
      FutbolSnapshot nf;
      if (cliFut.fetch(nf)) {
        nf.obtenido_ms = millis();
        nf.stale = false;
        g_snapFutbol = nf;
        Serial.printf("[futbol] refresh OK ultimos=%d proximos=%d\n",
                      (int)g_snapFutbol.ultimos.size(),
                      (int)g_snapFutbol.proximos.size());
      } else {
        g_snapFutbol.stale = true;
        Serial.println("[futbol] refresh FALLO");
      }
      vTaskDelay(pdMS_TO_TICKS(5000));

      // --- MotoGP ---
      MotogpSnapshot nm;
      if (cliMot.fetch(nm)) {
        nm.obtenido_ms = millis();
        nm.stale = false;
        g_snapMotogp = nm;
        Serial.printf("[motogp] refresh OK ultimos=%d proximos=%d\n",
                      (int)g_snapMotogp.ultimos.size(),
                      (int)g_snapMotogp.proximos.size());
      } else {
        g_snapMotogp.stale = true;
        Serial.println("[motogp] refresh FALLO");
      }
      vTaskDelay(pdMS_TO_TICKS(5000));

      // --- F1 ---
      F1Snapshot n1;
      if (cliF1.fetch(n1)) {
        n1.obtenido_ms = millis();
        n1.stale = false;
        g_snapF1 = n1;
        Serial.printf("[f1] refresh OK ultima=%s proximas=%d\n",
                      g_snapF1.ultima.nombreGp.c_str(),
                      (int)g_snapF1.proximas.size());
      } else {
        g_snapF1.stale = true;
        Serial.println("[f1] refresh FALLO");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(3UL * 3600UL * 1000UL));   // 3 h (frecuencia dominante = fútbol)
  }
}

void arrancarTouch() {
  touch::CalibracionTouch cal{g_cfg.touch_min_x, g_cfg.touch_max_x,
                              g_cfg.touch_min_y, g_cfg.touch_max_y,
                              g_cfg.touch_calibrado};
  if (!cal.valida) {
    // Defaults tentativos para poder navegar al calibrar.
    cal = touch::CalibracionTouch{300, 3800, 300, 3800, true};
  }
  touch::iniciar(cal);
}

void modoRadar() {
  g_estado = new RadarState(g_cfg.lat, g_cfg.lon, g_cfg.radio_km);
  RadarWebServer::iniciar(g_cfg, *g_estado, g_http);

  arrancarTouch();

  // Construir pantallas.
  auto* radar   = new PantallaRadar(*g_estado);
  auto* reloj   = new PantallaReloj();
  auto* meteo   = new PantallaMeteo(g_snapMeteo);
  auto* futbol  = new PantallaFutbol(g_snapFutbol);
  auto* motogp  = new PantallaMotogp(g_snapMotogp);
  auto* f1      = new PantallaF1(g_snapF1);

  static pantallas::GestorPantallas gestor(g_renderer);
  g_gestor = &gestor;

  // Sub-pantallas de ajustes.
  auto* intervalo    = new PantallaIntervalo(gestor, g_cfg);
  auto* selVistas    = new PantallaSeleccionVistas(gestor, g_cfg);
  auto* selVistaFija = new PantallaSeleccionVistaFija(gestor, g_cfg);
  auto* configLoc    = new PantallaConfigLocalizacion(gestor);
  auto* calibrar     = new PantallaCalibrarTouch(gestor, g_cfg);
  auto* confReset    = new PantallaConfirmarReset(gestor);
  PantallaAjustes::SubPantallas subs{intervalo, selVistas, selVistaFija,
                                     configLoc, calibrar, confReset};
  auto* ajustes = new PantallaAjustes(gestor, g_cfg, subs);

  // Menú (home).
  auto* menu = new PantallaMenu(gestor, ajustes);
  menu->configurarEntradas({
    {0, "Radar"}, {1, "Reloj"}, {2, "Meteo"},
    {3, "Futbol"}, {4, "MotoGP"}, {5, "F1"},
  });

  gestor.setHome(menu);
  pantallas::Pantalla* aRegistrar[] = {
    static_cast<pantallas::Pantalla*>(radar),
    static_cast<pantallas::Pantalla*>(reloj),
    static_cast<pantallas::Pantalla*>(meteo),
    static_cast<pantallas::Pantalla*>(futbol),
    static_cast<pantallas::Pantalla*>(motogp),
    static_cast<pantallas::Pantalla*>(f1),
    static_cast<pantallas::Pantalla*>(menu),
    static_cast<pantallas::Pantalla*>(ajustes),
    static_cast<pantallas::Pantalla*>(intervalo),
    static_cast<pantallas::Pantalla*>(selVistas),
    static_cast<pantallas::Pantalla*>(selVistaFija),
    static_cast<pantallas::Pantalla*>(configLoc),
    static_cast<pantallas::Pantalla*>(calibrar),
    static_cast<pantallas::Pantalla*>(confReset),
  };
  for (auto* p : aRegistrar) gestor.registrar(p);

  gestor.configurarModo(
    g_cfg.modo == ModoVista::CARRUSEL ? pantallas::ModoGestor::CARRUSEL
                                      : pantallas::ModoGestor::FIJO,
    g_cfg.intervalo_carrusel_s,
    g_cfg.vistas_orden,
    g_cfg.vista_fija);
  gestor.iniciar(millis());

  // Forzar calibración si no está calibrado.
  if (!g_cfg.touch_calibrado) gestor.abrirEnPila(calibrar);

  xTaskCreatePinnedToCore(tareaPoller,  "poller",  8192, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(tareaDisplay, "display", 4096, nullptr, 1, nullptr, 1);
  xTaskCreatePinnedToCore(tareaMeteoRefresh, "meteo", 6144, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(tareaDeportesRefresh, "deportes", 20480, nullptr, 1, nullptr, 0);
  Serial.println("[radar] modo operativo con carrusel");
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n===== Radar de vuelos ESP32 =====");

  tft_driver::iniciar();
  tft_driver::pintarSplash("Radar de vuelos", "arrancando...");

  if (!LittleFS.begin(true)) Serial.println("[fs] error montando LittleFS");
  StatusLed::iniciar(4, /*activoBajo=*/true);

  bool tieneCfg = ConfigStore::cargar(g_cfg);
  if (tieneCfg) {
    if (conectarWifi()) { modoRadar(); return; }
    Serial.println("[wifi] no conecta, entrando en portal");
  } else {
    Serial.println("[cfg] no hay config, entrando en portal");
  }

  uint8_t mac[6];
  WiFi.macAddress(mac);
  char sufijo[5];
  snprintf(sufijo, sizeof(sufijo), "%02X%02X", mac[4], mac[5]);
  std::string ap = std::string("RadarVuelos-") + sufijo;
  // El portal sigue reutilizando el helper QR ya existente.
  // (`WifiPortal::ejecutar` pinta su propia UI internamente.)
  WifiPortal::ejecutar(g_http);  // no retorna
}

void loop() { delay(1000); }
