// I2C de la placa virtual (SDA 21 / SCL 38). Los dispositivos del bus se
// registran en wire.cpp: hoy no hay ninguno, así que toda transmisión responde
// NACK (endTransmission() = 2), igual que una placa sin IMU ni OLED enchufados.
#pragma once
#include "Arduino.h"

class SimI2CDevice {
public:
  virtual ~SimI2CDevice() {}
  virtual uint8_t address() const = 0;
  virtual void write(const uint8_t *data, size_t n) = 0;     // una transacción de escritura completa
  virtual size_t read(uint8_t *out, size_t n) = 0;            // requestFrom
};
void sim_i2c_attach(SimI2CDevice *d);
void sim_i2c_detach(uint8_t addr);

class TwoWire : public Stream {
  uint8_t txAddr = 0;
  uint8_t txBuf[256];
  size_t txLen = 0;
  uint8_t rxBuf[256];
  size_t rxLen = 0, rxPos = 0;
  bool transmitting = false;
public:
  TwoWire(uint8_t bus = 0) { (void)bus; }
  bool begin(int sda = -1, int scl = -1, uint32_t frequency = 0) { (void)sda; (void)scl; (void)frequency; return true; }
  bool begin(uint8_t addr, int sda, int scl, uint32_t frequency) { (void)addr; return begin(sda, scl, frequency); }
  bool end() { return true; }
  bool setPins(int, int) { return true; }
  bool setClock(uint32_t) { return true; }
  uint32_t getClock() { return 400000; }
  void setTimeOut(uint16_t) {}
  void setTimeout(uint16_t) {}
  void beginTransmission(uint8_t addr) { txAddr = addr; txLen = 0; transmitting = true; }
  void beginTransmission(int addr) { beginTransmission((uint8_t)addr); }
  uint8_t endTransmission(bool sendStop = true);
  uint8_t endTransmission(uint8_t sendStop) { return endTransmission((bool)sendStop); }
  size_t pedir(uint8_t addr, size_t len, bool sendStop);
  // arduino-esp32 tiene nueve sobrecargas; una plantilla acepta cualquier mezcla de enteros sin ambigüedad
  template <typename A, typename B> uint8_t requestFrom(A addr, B len) { return (uint8_t)pedir((uint8_t)addr, (size_t)len, true); }
  template <typename A, typename B, typename C> uint8_t requestFrom(A addr, B len, C stop) { return (uint8_t)pedir((uint8_t)addr, (size_t)len, (bool)stop); }
  size_t write(uint8_t c) override { if (transmitting && txLen < sizeof txBuf) { txBuf[txLen++] = c; return 1; } return 0; }
  size_t write(const uint8_t *d, size_t n) override { size_t k = 0; while (n--) k += write(*d++); return k; }
  using Print::write;
  int available() override { return (int)(rxLen - rxPos); }
  int read() override { return rxPos < rxLen ? rxBuf[rxPos++] : -1; }
  int peek() override { return rxPos < rxLen ? rxBuf[rxPos] : -1; }
  void flush() override {}
  void onReceive(void (*)(int)) {}
  void onRequest(void (*)(void)) {}
};
extern TwoWire Wire;
extern TwoWire Wire1;
