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
#include "display_radar.h"

namespace {

RadarState*     g_estado = nullptr;
WifiHttpClient  g_http;
Config          g_cfg;
uint32_t        g_ultimoIntentoWifiMs = 0;

bool conectarWifi() {
  StatusLed::setEstado(EstadoLed::CONECTANDO_WIFI);
  DisplayRadar::pintarMensaje("Conectando WiFi", g_cfg.ssid);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(g_cfg.ssid.c_str(), g_cfg.password.c_str());
  uint32_t inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 20000) {
    delay(200);
  }
  if (WiFi.status() != WL_CONNECTED) return false;
  Serial.printf("[wifi] conectado, IP: %s\n", WiFi.localIP().toString().c_str());
  return true;
}

// Task del poller: cada 3 s consulta ADSB.lol y actualiza el snapshot.
void tareaPoller(void*) {
  AdsbClient cliente(g_http);
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      g_estado->marcarStale();
      StatusLed::setEstado(EstadoLed::RADAR_ERROR);
      if (millis() - g_ultimoIntentoWifiMs > 60000) {
        Serial.println("[wifi] 60s sin conexión, reiniciando");
        ESP.restart();
      }
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
    g_ultimoIntentoWifiMs = millis();
    std::vector<Aeronave> aviones;
    bool ok = cliente.fetchCerca(g_cfg.lat, g_cfg.lon, g_cfg.radio_km, aviones);
    if (!ok) {
      ok = cliente.fetchCerca(g_cfg.lat, g_cfg.lon, g_cfg.radio_km, aviones);
    }
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

// Task del display: cada 1 s toma snapshot y lo pinta en el TFT.
void tareaDisplay(void*) {
  for (;;) {
    Snapshot snap = g_estado->snapshot();
    DisplayRadar::pintarRadar(snap);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void modoRadar() {
  g_estado = new RadarState(g_cfg.lat, g_cfg.lon, g_cfg.radio_km);
  RadarWebServer::iniciar(g_cfg, *g_estado, g_http);
  xTaskCreatePinnedToCore(tareaPoller,  "poller",  8192, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(tareaDisplay, "display", 4096, nullptr, 1, nullptr, 1);
  Serial.println("[radar] modo operativo");
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n===== Radar de vuelos ESP32 =====");

  // Arrancar display antes de cualquier feedback visual
  DisplayRadar::iniciar();
  DisplayRadar::pintarMensaje("Radar de vuelos", "arrancando...");

  if (!LittleFS.begin(true)) {
    Serial.println("[fs] error montando LittleFS");
  }

  // LED de estado en GPIO 4 (LED rojo del RGB integrado del ESP32-2432S028,
  // active-low). GPIO 2 está reservado para el pin DC del display TFT.
  StatusLed::iniciar(4, /*activoBajo=*/true);

  bool tieneCfg = ConfigStore::cargar(g_cfg);
  if (tieneCfg) {
    Serial.printf("[cfg] cargada: ssid=%s dir=%s lat=%.4f lon=%.4f r=%d\n",
                  g_cfg.ssid.c_str(), g_cfg.direccion.c_str(),
                  g_cfg.lat, g_cfg.lon, g_cfg.radio_km);
    if (conectarWifi()) {
      modoRadar();
      return;
    }
    Serial.println("[wifi] no conecta, entrando en portal");
  } else {
    Serial.println("[cfg] no hay config, entrando en portal");
  }

  // Modo Portal: pintar QR con la URL del portal + instrucciones con el SSID.
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char sufijo[5];
  snprintf(sufijo, sizeof(sufijo), "%02X%02X", mac[4], mac[5]);
  std::string ap = std::string("RadarVuelos-") + sufijo;
  DisplayRadar::pintarPortalQR(ap, "http://192.168.4.1/");
  WifiPortal::ejecutar(g_http);   // no retorna
}

void loop() {
  delay(1000);
}
