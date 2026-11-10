// USB nativo y USB-MIDI de la placa virtual.
#include "USB.h"
#include "USBMIDI.h"
#include "SPI.h"

ESPUSB USB;
SPIClass SPI;
extern "C" void sim_midi_usb(uint8_t s, uint8_t d1, uint8_t d2);

ESPUSB::operator bool() const { return sim_bus()->usb_nativo != 0; }

void USBMIDI::send3(uint8_t s, uint8_t d1, uint8_t d2) { sim_midi_usb(s, d1, d2); }

// arma mensajes completos a partir de bytes sueltos (con running status)
size_t USBMIDI::write(uint8_t b) {
  if (b >= 0xF8) { send3(b, 0, 0); return 1; }               // tiempo real: va solo
  if (b & 0x80) {
    run[0] = b; runLen = 1;
    uint8_t hi = b & 0xF0;
    runNeed = (hi == 0xC0 || hi == 0xD0) ? 2 : (b >= 0xF0 ? 1 : 3);
    if (runNeed == 1) { send3(b, 0, 0); runLen = 0; }
    return 1;
  }
  if (runLen == 0) return 1;                                   // dato sin status: se ignora
  run[runLen++] = b;
  if (runLen >= runNeed) { send3(run[0], run[1], runNeed > 2 ? run[2] : 0); runLen = 1; }
  return 1;
}
