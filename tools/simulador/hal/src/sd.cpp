// microSD de la placa virtual: un mapa ruta → bytes en memoria, con un candado
// de giro (la interfaz también lo lee desde el hilo principal del navegador,
// donde no se puede bloquear con un mutex).
#include "SD.h"
#include <emscripten.h>
#include <atomic>
#include <map>
#include <vector>
#include <string>

namespace fs {
struct SimNodo {
  std::vector<uint8_t> datos;
};
struct SimAbierto {
  std::string ruta;
  std::string nombre;                       // sin la carpeta, como File::name() de arduino-esp32 3.x
  std::shared_ptr<SimNodo> nodo;            // nulo = carpeta
  size_t pos = 0;
  bool escribe = false;
  std::vector<std::string> hijos;           // carpeta: lo que había al abrirla
  size_t hijo = 0;
};
}  // namespace fs
using namespace fs;

static std::map<std::string, std::shared_ptr<SimNodo>> g_arch;
static std::map<std::string, bool> g_dirs;
static std::atomic_flag g_cerrojo = ATOMIC_FLAG_INIT;
struct Cerrojo {
  Cerrojo() { while (g_cerrojo.test_and_set(std::memory_order_acquire)) {} }
  ~Cerrojo() { g_cerrojo.clear(std::memory_order_release); }
};
static void cambio() { __atomic_add_fetch(&sim_bus()->sd_seq, 1, __ATOMIC_RELEASE); }
static bool puesta() { return sim_bus()->sd_on != 0; }

static std::string normal(const char *p) {
  std::string s = p ? p : "";
  if (s.empty() || s[0] != '/') s = "/" + s;
  while (s.size() > 1 && s.back() == '/') s.pop_back();
  return s;
}
static std::string carpeta(const std::string &r) { size_t k = r.rfind('/'); return k == 0 ? "/" : r.substr(0, k); }
static std::string base(const std::string &r) { size_t k = r.rfind('/'); return r.substr(k + 1); }
static bool es_dir(const std::string &r) { return r == "/" || g_dirs.count(r); }

fs::SDFS SD;

bool SDFS::begin(uint8_t, SPIClass &, uint32_t, const char *, uint8_t, bool) { return puesta(); }
sdcard_type_t SDFS::cardType() { return puesta() ? CARD_SDHC : CARD_NONE; }
uint64_t SDFS::usedBytes() {
  Cerrojo c; uint64_t s = 0;
  for (auto &kv : g_arch) s += kv.second->datos.size();
  return s;
}
bool SDFS::exists(const char *path) {
  if (!puesta()) return false;
  std::string r = normal(path); Cerrojo c;
  return g_arch.count(r) || es_dir(r);
}
bool SDFS::remove(const char *path) {
  if (!puesta()) return false;
  std::string r = normal(path);
  { Cerrojo c; if (!g_arch.erase(r)) return false; }
  cambio(); return true;
}
bool SDFS::rename(const char *from, const char *to) {
  if (!puesta()) return false;
  std::string a = normal(from), b = normal(to);
  { Cerrojo c; auto it = g_arch.find(a); if (it == g_arch.end()) return false; g_arch[b] = it->second; g_arch.erase(a); }
  cambio(); return true;
}
bool SDFS::mkdir(const char *path) { if (!puesta()) return false; Cerrojo c; g_dirs[normal(path)] = true; return true; }
bool SDFS::rmdir(const char *path) { if (!puesta()) return false; Cerrojo c; return g_dirs.erase(normal(path)) > 0; }

File SDFS::open(const char *path, const char *mode, bool) {
  if (!puesta()) return File();
  std::string r = normal(path), m = mode ? mode : "r";
  auto a = std::make_shared<SimAbierto>();
  a->ruta = r; a->nombre = base(r);
  {
    Cerrojo c;
    if (es_dir(r)) {
      if (m[0] != 'r') return File();
      std::string pre = r == "/" ? "/" : r + "/";
      for (auto &kv : g_arch) if (kv.first.compare(0, pre.size(), pre) == 0 && kv.first.find('/', pre.size()) == std::string::npos) a->hijos.push_back(kv.first);
      for (auto &kv : g_dirs) if (kv.first.compare(0, pre.size(), pre) == 0 && kv.first.find('/', pre.size()) == std::string::npos) a->hijos.push_back(kv.first);
      if (r == "/") a->nombre = "/";
      return File(a);
    }
    if (!es_dir(carpeta(r))) return File();
    auto it = g_arch.find(r);
    if (m[0] == 'r') {
      if (it == g_arch.end()) return File();
      a->nodo = it->second; a->escribe = m.find('+') != std::string::npos;
    } else if (m[0] == 'w') {
      a->nodo = std::make_shared<SimNodo>(); g_arch[r] = a->nodo; a->escribe = true;
    } else {   // 'a'
      if (it == g_arch.end()) { a->nodo = std::make_shared<SimNodo>(); g_arch[r] = a->nodo; }
      else a->nodo = it->second;
      a->pos = a->nodo->datos.size(); a->escribe = true;
    }
  }
  if (a->escribe) cambio();
  return File(a);
}

size_t File::write(uint8_t c) { return write(&c, 1); }
size_t File::write(const uint8_t *buf, size_t n) {
  if (!a || !a->nodo || !a->escribe || !puesta()) return 0;
  {
    Cerrojo c;
    auto &d = a->nodo->datos;
    if (a->pos + n > d.size()) d.resize(a->pos + n);
    memcpy(d.data() + a->pos, buf, n);
    a->pos += n;
  }
  cambio();
  return n;
}
int File::available() {
  if (!a || !a->nodo) return 0;
  Cerrojo c; size_t s = a->nodo->datos.size();
  return s > a->pos ? (int)(s - a->pos) : 0;
}
int File::read() { uint8_t c; return read(&c, 1) == 1 ? c : -1; }
int File::peek() {
  if (!a || !a->nodo) return -1;
  Cerrojo c; auto &d = a->nodo->datos;
  return a->pos < d.size() ? d[a->pos] : -1;
}
size_t File::read(uint8_t *buf, size_t n) {
  if (!a || !a->nodo || !puesta()) return 0;
  Cerrojo c; auto &d = a->nodo->datos;
  if (a->pos >= d.size()) return 0;
  if (n > d.size() - a->pos) n = d.size() - a->pos;
  memcpy(buf, d.data() + a->pos, n);
  a->pos += n;
  return n;
}
bool File::seek(uint32_t pos, SeekMode mode) {
  if (!a || !a->nodo) return false;
  Cerrojo c; size_t s = a->nodo->datos.size();
  long long p = mode == SeekSet ? (long long)pos : mode == SeekCur ? (long long)a->pos + pos : (long long)s + pos;
  if (p < 0 || (!a->escribe && (size_t)p > s)) return false;
  a->pos = (size_t)p;
  return true;
}
size_t File::position() const { return a ? a->pos : 0; }
size_t File::size() const { if (!a || !a->nodo) return 0; Cerrojo c; return a->nodo->datos.size(); }
const char *File::path() const { return a ? a->ruta.c_str() : ""; }
const char *File::name() const { return a ? a->nombre.c_str() : ""; }
bool File::isDirectory() const { return a && !a->nodo; }
File File::openNextFile(const char *mode) {
  if (!a || a->nodo || a->hijo >= a->hijos.size()) return File();
  return SD.open(a->hijos[a->hijo++].c_str(), mode);
}
String File::getNextFileName() {
  if (!a || a->nodo || a->hijo >= a->hijos.size()) return String("");
  return String(a->hijos[a->hijo++].c_str());
}
void File::rewindDirectory() { if (a) a->hijo = 0; }

// ---------------------------------------------------------------- la interfaz
// Se llaman desde el hilo principal del navegador (index.html, a través del iframe).
static std::string g_lista;
static std::vector<uint8_t> g_copia;
extern "C" EMSCRIPTEN_KEEPALIVE const char *sim_sd_lista() {
  Cerrojo c;
  g_lista = "[";
  for (auto &kv : g_arch) {
    if (g_lista.size() > 1) g_lista += ",";
    g_lista += "{\"n\":\"" + kv.first + "\",\"b\":" + std::to_string(kv.second->datos.size()) + "}";
  }
  g_lista += "]";
  return g_lista.c_str();
}
// copia el archivo a un búfer fijo y devuelve su dirección (el largo, con sim_sd_largo)
extern "C" EMSCRIPTEN_KEEPALIVE const uint8_t *sim_sd_copia(const char *ruta) {
  Cerrojo c;
  auto it = g_arch.find(normal(ruta));
  if (it == g_arch.end()) { g_copia.clear(); return nullptr; }
  g_copia = it->second->datos;
  return g_copia.data();
}
extern "C" EMSCRIPTEN_KEEPALIVE int sim_sd_largo() { return (int)g_copia.size(); }
// la interfaz pide memoria, copia los bytes y entrega el archivo (sim_sd_poner se queda con ellos)
extern "C" EMSCRIPTEN_KEEPALIVE uint8_t *sim_sd_reservar(int n) { return (uint8_t *)malloc(n > 0 ? n : 1); }
extern "C" EMSCRIPTEN_KEEPALIVE void sim_sd_poner(const char *ruta, uint8_t *datos, int n) {
  auto nodo = std::make_shared<SimNodo>();
  nodo->datos.assign(datos, datos + n);
  free(datos);
  std::string r = normal(ruta);
  {
    Cerrojo c;
    g_arch[r] = nodo;
    for (std::string d = carpeta(r); d != "/"; d = carpeta(d)) g_dirs[d] = true;
  }
  cambio();
}
extern "C" EMSCRIPTEN_KEEPALIVE void sim_sd_borrar(const char *ruta) {
  { Cerrojo c; g_arch.erase(normal(ruta)); }
  cambio();
}
