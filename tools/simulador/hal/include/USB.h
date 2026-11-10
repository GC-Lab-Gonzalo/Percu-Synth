// USB nativo del ESP32-S3 (TinyUSB). En la placa virtual sólo guarda los nombres.
#pragma once
#include "Arduino.h"
class ESPUSB {
public:
  bool begin() { return true; }
  ESPUSB &productName(const char *) { return *this; }
  ESPUSB &manufacturerName(const char *) { return *this; }
  ESPUSB &serialNumber(const char *) { return *this; }
  ESPUSB &VID(uint16_t) { return *this; }
  ESPUSB &PID(uint16_t) { return *this; }
  ESPUSB &firmwareVersion(uint16_t) { return *this; }
  ESPUSB &usbVersion(uint16_t) { return *this; }
  ESPUSB &usbPower(uint16_t) { return *this; }
  ESPUSB &usbClass(uint8_t) { return *this; }
  ESPUSB &webUSB(bool) { return *this; }
  operator bool() const;
};
extern ESPUSB USB;
