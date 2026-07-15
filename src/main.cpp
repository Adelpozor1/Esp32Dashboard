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

namespace {

RadarState*     g_estado = nullptr;
WifiHttpClient  g_http;
Config          g_cfg;
uint32_t        g_ultimoIntentoWifiMs = 0;

// Conecta al WiFi guardado. Devuelve true si conecta en <= 20 s.
bool conectarWifi() {
  StatusLed::setEstado(EstadoLed::CONECTANDO_WIFI);
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
    // Watchdog implícito: si el fetch se cuelga > 15 s se dispara el TWDT (activo por defecto).
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
      // 1 reintento inmediato
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

void modoRadar() {
  g_estado = new RadarState(g_cfg.lat, g_cfg.lon, g_cfg.radio_km);
  RadarWebServer::iniciar(g_cfg, *g_estado, g_http);
  xTaskCreatePinnedToCore(tareaPoller, "poller", 8192, nullptr, 1, nullptr, 0);
  Serial.println("[radar] modo operativo");
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n===== Radar de vuelos ESP32 =====");

  if (!LittleFS.begin(true)) {
    Serial.println("[fs] error montando LittleFS");
  }

  StatusLed::iniciar(2);

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
  WifiPortal::ejecutar(g_http);   // no retorna
}

void loop() {
  // Todo en tasks.
  delay(1000);
}
