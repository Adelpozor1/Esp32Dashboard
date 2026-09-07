#pragma once
#include <string>
#include <functional>

class IHttpClient {
 public:
  virtual ~IHttpClient() = default;

  // GET síncrono. Rellena `bodyOut` con la respuesta y `statusOut` con el código HTTP.
  // Devuelve true si el request se completó (con cualquier código); false si hubo timeout
  // o error de conexión.
  virtual bool get(const std::string& url,
                   std::string& bodyOut,
                   int& statusOut,
                   int timeoutMs = 5000) = 0;

  // GET con streaming del body a un callback. Útil para respuestas grandes que
  // no caben cómodamente en un String (>10 KB) — el callback lee chunks del
  // stream que da el HTTPClient, sin acumular todo en RAM. Solo tiene
  // implementación real en Arduino; el default aquí devuelve false para no
  // obligar a los mocks a implementarlo.
  //
  // El callback recibe un puntero opaco a Stream* (en Arduino) o similar.
  // Se llama solo si el status es 200 y hay cuerpo.
  virtual bool getStreamed(const std::string& /*url*/,
                           int& /*statusOut*/,
                           int /*timeoutMs*/,
                           std::function<bool(void* stream)> /*cb*/) {
    return false;
  }
};

// -----------------------------------------------------------------------------
// Implementación real usando WiFiClientSecure / HTTPClient (Arduino ESP32)
// -----------------------------------------------------------------------------
#ifdef ARDUINO
class WifiHttpClient : public IHttpClient {
 public:
  bool get(const std::string& url,
           std::string& bodyOut,
           int& statusOut,
           int timeoutMs = 5000) override;
  bool getStreamed(const std::string& url,
                   int& statusOut,
                   int timeoutMs,
                   std::function<bool(void* stream)> cb) override;
};
#endif
