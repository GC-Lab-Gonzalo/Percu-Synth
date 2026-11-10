// Preferences (NVS) de la placa virtual: guarda en memoria mientras la placa
// está encendida. Al desenchufar los dos cables se pierde (en el ESP32 no).
#pragma once
#include "Arduino.h"
#include <map>
#include <string>
#include <vector>

class Preferences {
  std::string ns;
  bool ro = false;
  static std::map<std::string, std::vector<uint8_t>> &tabla() { static std::map<std::string, std::vector<uint8_t>> t; return t; }
  std::string k(const char *key) const { return ns + "/" + key; }
  template <typename T> size_t poner(const char *key, T v) {
    if (ro) return 0; std::vector<uint8_t> b(sizeof(T)); memcpy(b.data(), &v, sizeof(T)); tabla()[k(key)] = b; return sizeof(T); }
  template <typename T> T sacar(const char *key, T def) const {
    auto it = tabla().find(k(key)); if (it == tabla().end() || it->second.size() != sizeof(T)) return def;
    T v; memcpy(&v, it->second.data(), sizeof(T)); return v; }
public:
  bool begin(const char *name, bool readOnly = false, const char * = nullptr) { ns = name ? name : ""; ro = readOnly; return true; }
  void end() {}
  bool clear() { for (auto it = tabla().begin(); it != tabla().end();) { if (it->first.rfind(ns + "/", 0) == 0) it = tabla().erase(it); else ++it; } return true; }
  bool remove(const char *key) { return tabla().erase(k(key)) > 0; }
  bool isKey(const char *key) { return tabla().count(k(key)) > 0; }
  size_t putChar(const char *key, int8_t v) { return poner(key, v); }
  size_t putUChar(const char *key, uint8_t v) { return poner(key, v); }
  size_t putShort(const char *key, int16_t v) { return poner(key, v); }
  size_t putUShort(const char *key, uint16_t v) { return poner(key, v); }
  size_t putInt(const char *key, int32_t v) { return poner(key, v); }
  size_t putUInt(const char *key, uint32_t v) { return poner(key, v); }
  size_t putLong(const char *key, int32_t v) { return poner(key, v); }
  size_t putULong(const char *key, uint32_t v) { return poner(key, v); }
  size_t putLong64(const char *key, int64_t v) { return poner(key, v); }
  size_t putULong64(const char *key, uint64_t v) { return poner(key, v); }
  size_t putFloat(const char *key, float v) { return poner(key, v); }
  size_t putDouble(const char *key, double v) { return poner(key, v); }
  size_t putBool(const char *key, bool v) { return poner(key, (uint8_t)v); }
  size_t putString(const char *key, const char *v) { if (ro) return 0; std::string s(v ? v : ""); tabla()[k(key)] = std::vector<uint8_t>(s.begin(), s.end()); return s.size(); }
  size_t putString(const char *key, const String &v) { return putString(key, v.c_str()); }
  size_t putBytes(const char *key, const void *v, size_t n) { if (ro) return 0; const uint8_t *p = (const uint8_t *)v; tabla()[k(key)] = std::vector<uint8_t>(p, p + n); return n; }
  int8_t getChar(const char *key, int8_t d = 0) { return sacar(key, d); }
  uint8_t getUChar(const char *key, uint8_t d = 0) { return sacar(key, d); }
  int16_t getShort(const char *key, int16_t d = 0) { return sacar(key, d); }
  uint16_t getUShort(const char *key, uint16_t d = 0) { return sacar(key, d); }
  int32_t getInt(const char *key, int32_t d = 0) { return sacar(key, d); }
  uint32_t getUInt(const char *key, uint32_t d = 0) { return sacar(key, d); }
  int32_t getLong(const char *key, int32_t d = 0) { return sacar(key, d); }
  uint32_t getULong(const char *key, uint32_t d = 0) { return sacar(key, d); }
  int64_t getLong64(const char *key, int64_t d = 0) { return sacar(key, d); }
  uint64_t getULong64(const char *key, uint64_t d = 0) { return sacar(key, d); }
  float getFloat(const char *key, float d = NAN) { return sacar(key, d); }
  double getDouble(const char *key, double d = NAN) { return sacar(key, d); }
  bool getBool(const char *key, bool d = false) { return sacar(key, (uint8_t)d) != 0; }
  String getString(const char *key, const String d = String()) {
    auto it = tabla().find(k(key)); if (it == tabla().end()) return d; return String(std::string(it->second.begin(), it->second.end())); }
  size_t getString(const char *key, char *v, size_t max) {
    auto it = tabla().find(k(key)); if (it == tabla().end() || !max) return 0;
    size_t n = it->second.size() < max - 1 ? it->second.size() : max - 1; memcpy(v, it->second.data(), n); v[n] = 0; return n; }
  size_t getBytesLength(const char *key) { auto it = tabla().find(k(key)); return it == tabla().end() ? 0 : it->second.size(); }
  size_t getBytes(const char *key, void *buf, size_t max) {
    auto it = tabla().find(k(key)); if (it == tabla().end()) return 0;
    size_t n = it->second.size() < max ? it->second.size() : max; memcpy(buf, it->second.data(), n); return n; }
  size_t freeEntries() { return 500; }
};
