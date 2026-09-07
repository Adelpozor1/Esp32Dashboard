#pragma once
#include <cstdint>
#include <string>

// Metadatos de una versión publicada en GitHub Releases.
struct OtaVersionInfo {
  std::string version;   // p.ej. "20260907-121530-abc1234"
  std::string sha;
  uint32_t    size = 0;
  std::string url;       // firmware.bin URL (Release asset, con redirects)
};

class OtaUpdater {
 public:
  // Devuelve la versión con la que se compiló este firmware. Viene inyectada
  // desde platformio.ini/CI vía macro FIRMWARE_VERSION. En builds locales sin
  // CI la versión es "dev-local".
  static const char* versionActual();

  // GET al firmware.json de la release "latest" del repo GitHub configurado.
  // Devuelve true y rellena `out` si consigue leer y parsear la metadata.
  static bool comprobarVersionRemota(OtaVersionInfo& out);

  // Descarga firmware.bin y aplica Update.write() sobre la partición inactiva.
  // Si tiene éxito llama a ESP.restart() y NO retorna. Devuelve false si algo
  // falla antes del restart.
  static bool aplicarActualizacion(const OtaVersionInfo& info);
};
