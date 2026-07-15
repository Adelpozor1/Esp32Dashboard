#pragma once
#include <string>

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
};
