#include "config_store.h"
#include <cstring>

#ifndef UNIT_TEST
#include <Preferences.h>
static const char* NVS_NAMESPACE = "radarvuelos";
static const char* NVS_KEY = "cfg";
#endif

namespace {

void escribirU16(std::vector<uint8_t>& v, uint16_t x) {
  v.push_back(static_cast<uint8_t>(x & 0xFF));
  v.push_back(static_cast<uint8_t>((x >> 8) & 0xFF));
}

void escribirString(std::vector<uint8_t>& v, const std::string& s) {
  uint16_t n = static_cast<uint16_t>(s.size());
  if (n > ConfigStore::MAX_STR) n = ConfigStore::MAX_STR;
  escribirU16(v, n);
  v.insert(v.end(), s.begin(), s.begin() + n);
}

void escribirDouble(std::vector<uint8_t>& v, double d) {
  uint8_t buf[sizeof(double)];
  std::memcpy(buf, &d, sizeof(double));
  v.insert(v.end(), buf, buf + sizeof(double));
}

bool leerU16(const uint8_t* data, size_t size, size_t& pos, uint16_t& out) {
  if (pos + 2 > size) return false;
  out = static_cast<uint16_t>(data[pos]) |
        (static_cast<uint16_t>(data[pos + 1]) << 8);
  pos += 2;
  return true;
}

bool leerDouble(const uint8_t* data, size_t size, size_t& pos, double& out) {
  if (pos + sizeof(double) > size) return false;
  std::memcpy(&out, data + pos, sizeof(double));
  pos += sizeof(double);
  return true;
}

bool leerString(const uint8_t* data, size_t size, size_t& pos, std::string& out) {
  uint16_t n;
  if (!leerU16(data, size, pos, n)) return false;
  if (n > ConfigStore::MAX_STR) return false;
  if (pos + n > size) return false;
  out.assign(reinterpret_cast<const char*>(data + pos), n);
  pos += n;
  return true;
}

}  // namespace

void ConfigStore::serializar(const Config& in, std::vector<uint8_t>& out) {
  out.clear();
  escribirU16(out, MAGIC);
  out.push_back(VERSION);
  escribirDouble(out, in.lat);
  escribirDouble(out, in.lon);
  escribirU16(out, static_cast<uint16_t>(in.radio_km));
  escribirString(out, in.ssid);
  escribirString(out, in.password);
  escribirString(out, in.direccion);
}

bool ConfigStore::deserializar(const uint8_t* data, size_t size, Config& out) {
  if (!data || size < 4) return false;
  size_t pos = 0;
  uint16_t magic;
  if (!leerU16(data, size, pos, magic) || magic != MAGIC) return false;
  if (data[pos++] != VERSION) return false;
  double lat, lon;
  if (!leerDouble(data, size, pos, lat)) return false;
  if (!leerDouble(data, size, pos, lon)) return false;
  uint16_t radio;
  if (!leerU16(data, size, pos, radio)) return false;
  std::string ssid, pass, dir;
  if (!leerString(data, size, pos, ssid)) return false;
  if (!leerString(data, size, pos, pass)) return false;
  if (!leerString(data, size, pos, dir))  return false;
  out.ssid = std::move(ssid);
  out.password = std::move(pass);
  out.direccion = std::move(dir);
  out.lat = lat;
  out.lon = lon;
  out.radio_km = radio;
  return true;
}

#ifndef UNIT_TEST

bool ConfigStore::cargar(Config& out) {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, /*readOnly=*/true)) return false;
  size_t sz = prefs.getBytesLength(NVS_KEY);
  if (sz == 0 || sz > 4096) { prefs.end(); return false; }
  std::vector<uint8_t> buf(sz);
  size_t leido = prefs.getBytes(NVS_KEY, buf.data(), sz);
  prefs.end();
  if (leido != sz) return false;
  return deserializar(buf.data(), sz, out);
}

bool ConfigStore::guardar(const Config& cfg) {
  std::vector<uint8_t> buf;
  serializar(cfg, buf);
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, /*readOnly=*/false)) return false;
  size_t escrito = prefs.putBytes(NVS_KEY, buf.data(), buf.size());
  prefs.end();
  return escrito == buf.size();
}

void ConfigStore::borrar() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, /*readOnly=*/false)) return;
  prefs.remove(NVS_KEY);
  prefs.end();
}

#endif
