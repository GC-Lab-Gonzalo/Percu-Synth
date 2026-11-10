// Puertos serie de la placa virtual.
//
//   Serial   = USB nativo del ESP32-S3 (CDC). Sólo existe si el cable está en el conector USB.
//   Serial0  = UART0 por el puente USB del conector COM. Sólo llega si el cable está en COM.
//   Serial1  = UART1. Con TX en el GPIO 43 sale por el DIN-5 (así lo usan drum_poder y los *_clock).
//
// Igual que arduino-esp32 con "USB CDC On Boot: Enabled": Serial es el CDC.
#pragma once
#include "Stream.h"

#define SERIAL_5N1 0x8000010
#define SERIAL_6N1 0x8000014
#define SERIAL_7N1 0x8000018
#define SERIAL_8N1 0x800001c
#define SERIAL_8N2 0x800003c
#define SERIAL_8E1 0x800001e
#define SERIAL_8O1 0x800001f

class SimSerialBase : public Stream {
protected:
  int tx_idx;              // anillo de salida en el SimBus (-1 = a ninguna parte)
  int rx_idx;              // anillo de entrada (-1 = nunca llega nada)
  uint32_t tx_timeout_ms = 100;
  bool started = false;
  virtual bool conectado() const;
public:
  SimSerialBase(int tx, int rx) : tx_idx(tx), rx_idx(rx) {}
  size_t write(uint8_t c) override;
  size_t write(const uint8_t *buf, size_t n) override;
  using Print::write;
  int available() override;
  int read() override;
  int peek() override;
  int availableForWrite() override;
  void flush() override {}
  void setTxTimeoutMs(uint32_t ms) { tx_timeout_ms = ms; }
  void setDebugOutput(bool) {}
  size_t setRxBufferSize(size_t n) { return n; }
  size_t setTxBufferSize(size_t n) { return n; }
  explicit operator bool() const { return conectado(); }
};

class HWCDC : public SimSerialBase {
protected:
  bool conectado() const override;
public:
  HWCDC() : SimSerialBase(0, 0) {}
  void begin(unsigned long baud = 115200) { (void)baud; started = true; }
  void end() { started = false; }
  bool isConnected() const { return conectado(); }
  bool isPlugged() const { return conectado(); }
  void enableReboot(bool) {}
};
typedef HWCDC USBCDC;

class HardwareSerial : public SimSerialBase {
  int num;
public:
  HardwareSerial(int uart_nr);
  void begin(unsigned long baud, uint32_t config = SERIAL_8N1, int8_t rxPin = -1, int8_t txPin = -1,
             bool invert = false, unsigned long timeout_ms = 20000UL, uint8_t rxfifo_full_thrhd = 112);
  void end() { started = false; }
  void updateBaudRate(unsigned long baud);
  bool setPins(int8_t rxPin, int8_t txPin, int8_t ctsPin = -1, int8_t rtsPin = -1);
  unsigned long baudRate() const;
};

extern HWCDC Serial;
extern HardwareSerial Serial0;
extern HardwareSerial Serial1;
extern HardwareSerial Serial2;
