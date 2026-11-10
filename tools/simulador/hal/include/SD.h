// microSD de la placa virtual (SPI 14/15/16/17). La tarjeta es una memoria con
// archivos: se pone o se saca desde el panel «Placa», y lo que el sketch graba
// se puede descargar desde la interfaz (y al revés: un .wav soltado sobre la
// página va a la tarjeta). Los archivos sobreviven a reinicios y recompilaciones,
// como una tarjeta de verdad.
//
// API de arduino-esp32 (SD.h + FS.h): begin/end/cardType/open/exists/remove/
// mkdir/rename/totalBytes/usedBytes y File con read/write/seek/position/size/
// openNextFile/name. La velocidad de la tarjeta NO se simula: una escritura
// nunca se traba 200 ms como en la placa.
#pragma once
#include "Arduino.h"
#include "SPI.h"
#include <memory>

#define FILE_READ   "r"
#define FILE_WRITE  "w"
#define FILE_APPEND "a"

typedef enum { CARD_NONE, CARD_MMC, CARD_SD, CARD_SDHC, CARD_UNKNOWN } sdcard_type_t;

namespace fs {
enum SeekMode { SeekSet = 0, SeekCur = 1, SeekEnd = 2 };
struct SimNodo;
struct SimAbierto;

class File : public Stream {
  std::shared_ptr<SimAbierto> a;
public:
  File() {}
  explicit File(std::shared_ptr<SimAbierto> p) : a(p) {}
  size_t write(uint8_t c) override;
  size_t write(const uint8_t *buf, size_t n) override;
  using Print::write;
  int available() override;
  int read() override;
  int peek() override;
  size_t read(uint8_t *buf, size_t n);
  size_t readBytes(char *buf, size_t n) { return read((uint8_t *)buf, n); }
  void flush() override {}
  bool seek(uint32_t pos, SeekMode mode);
  bool seek(uint32_t pos) { return seek(pos, SeekSet); }
  size_t position() const;
  size_t size() const;
  bool setBufferSize(size_t) { return true; }
  void close() { a.reset(); }
  operator bool() const { return (bool)a; }
  time_t getLastWrite() { return 0; }
  const char *path() const;
  const char *name() const;
  bool isDirectory() const;
  File openNextFile(const char *mode = FILE_READ);
  String getNextFileName();
  void rewindDirectory();
};

class SDFS {
public:
  bool begin(uint8_t ssPin = 17, SPIClass &spi = SPI, uint32_t frequency = 4000000, const char *mountpoint = "/sd",
             uint8_t max_files = 5, bool format_if_empty = false);
  void end() {}
  sdcard_type_t cardType();
  uint64_t cardSize() { return 8ULL << 30; }
  size_t numSectors() { return (size_t)(cardSize() / 512); }
  size_t sectorSize() { return 512; }
  uint64_t totalBytes() { return 8ULL << 30; }
  uint64_t usedBytes();
  File open(const char *path, const char *mode = FILE_READ, bool create = false);
  File open(const String &path, const char *mode = FILE_READ, bool create = false) { return open(path.c_str(), mode, create); }
  bool exists(const char *path);
  bool exists(const String &path) { return exists(path.c_str()); }
  bool remove(const char *path);
  bool remove(const String &path) { return remove(path.c_str()); }
  bool rename(const char *from, const char *to);
  bool rename(const String &from, const String &to) { return rename(from.c_str(), to.c_str()); }
  bool mkdir(const char *path);
  bool mkdir(const String &path) { return mkdir(path.c_str()); }
  bool rmdir(const char *path);
  bool rmdir(const String &path) { return rmdir(path.c_str()); }
};
}  // namespace fs

using fs::File;
using fs::SeekMode;
using fs::SeekSet;
using fs::SeekCur;
using fs::SeekEnd;
typedef fs::SDFS SDFS;
extern fs::SDFS SD;
