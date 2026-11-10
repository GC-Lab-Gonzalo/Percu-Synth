// SPI de la placa virtual: existe para que compilen los sketches que lo incluyen,
// pero no hay ningún dispositivo SPI conectado (la microSD todavía no se simula).
#pragma once
#include "Arduino.h"
#define SPI_MODE0 0
#define SPI_MODE1 1
#define SPI_MODE2 2
#define SPI_MODE3 3
#define MSBFIRST 1
#define LSBFIRST 0
class SPISettings { public: SPISettings() {} SPISettings(uint32_t, uint8_t, uint8_t) {} };
class SPIClass {
public:
  SPIClass(uint8_t bus = 0) { (void)bus; }
  void begin(int8_t sck = -1, int8_t miso = -1, int8_t mosi = -1, int8_t ss = -1) { (void)sck; (void)miso; (void)mosi; (void)ss; }
  void end() {}
  void beginTransaction(SPISettings) {}
  void endTransaction() {}
  uint8_t transfer(uint8_t) { return 0xFF; }
  uint16_t transfer16(uint16_t) { return 0xFFFF; }
  void transfer(void *buf, size_t n) { memset(buf, 0xFF, n); }
  void setFrequency(uint32_t) {}
  void setDataMode(uint8_t) {}
  void setBitOrder(uint8_t) {}
};
extern SPIClass SPI;
#define FSPI 0
#define HSPI 1
