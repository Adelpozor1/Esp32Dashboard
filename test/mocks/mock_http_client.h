#pragma once
#include "http_client.h"
#include <string>
#include <vector>
#include <utility>

// Mock determinístico: se le programa una lista de (url_substring, body, status) y
// cada get() busca la primera entrada cuyo substring aparezca en la url solicitada.
class MockHttpClient : public IHttpClient {
 public:
  struct Respuesta {
    std::string urlSubstring;
    std::string body;
    int status;
    bool exito;   // false → simula fallo de conexión / timeout
  };

  std::vector<Respuesta> respuestas;
  std::vector<std::string> urlsLlamadas;

  bool get(const std::string& url,
           std::string& bodyOut,
           int& statusOut,
           int /*timeoutMs*/ = 5000) override {
    urlsLlamadas.push_back(url);
    for (const auto& r : respuestas) {
      if (url.find(r.urlSubstring) != std::string::npos) {
        bodyOut = r.body;
        statusOut = r.status;
        return r.exito;
      }
    }
    bodyOut = "";
    statusOut = 0;
    return false;
  }
};
