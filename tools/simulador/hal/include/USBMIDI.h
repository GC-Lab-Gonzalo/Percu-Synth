// USB-MIDI de arduino-esp32 en la placa virtual. Los mensajes salen por el
// conector USB nativo: si ese cable está desenchufado, se pierden.
#pragma once
#include "Arduino.h"

typedef struct { uint8_t header; uint8_t byte1; uint8_t byte2; uint8_t byte3; } midiEventPacket_t;

class USBMIDI {
  uint8_t run[3]; int runLen = 0, runNeed = 0;
  void send3(uint8_t s, uint8_t d1, uint8_t d2);
public:
  USBMIDI() {}
  USBMIDI(const char *) {}
  void begin() {}
  void end() {}
  void noteOn(uint8_t note, uint8_t velocity = 0, uint8_t channel = 1) { send3(0x90 | ((channel - 1) & 15), note & 127, velocity & 127); }
  void noteOff(uint8_t note, uint8_t velocity = 0, uint8_t channel = 1) { send3(0x80 | ((channel - 1) & 15), note & 127, velocity & 127); }
  void polyPressure(uint8_t note, uint8_t pressure, uint8_t channel = 1) { send3(0xA0 | ((channel - 1) & 15), note & 127, pressure & 127); }
  void controlChange(uint8_t control, uint8_t value = 0, uint8_t channel = 1) { send3(0xB0 | ((channel - 1) & 15), control & 127, value & 127); }
  void programChange(uint8_t program, uint8_t channel = 1) { send3(0xC0 | ((channel - 1) & 15), program & 127, 0); }
  void channelPressure(uint8_t pressure, uint8_t channel = 1) { send3(0xD0 | ((channel - 1) & 15), pressure & 127, 0); }
  void pitchBend(int16_t value, uint8_t channel = 1) { uint16_t v = (uint16_t)(value + 8192) & 0x3FFF; send3(0xE0 | ((channel - 1) & 15), v & 127, v >> 7); }
  void pitchBend(uint16_t value, uint8_t channel = 1) { send3(0xE0 | ((channel - 1) & 15), value & 127, (value >> 7) & 127); }
  void pitchBend(double value, uint8_t channel = 1) { pitchBend((int16_t)(value * 8191), channel); }
  // compatibilidad con la librería MIDI de FortySevenEffects
  void sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel) { noteOn(note, velocity, channel); }
  void sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel) { noteOff(note, velocity, channel); }
  void sendControlChange(uint8_t c, uint8_t v, uint8_t ch) { controlChange(c, v, ch); }
  void sendProgramChange(uint8_t p, uint8_t ch) { programChange(p, ch); }
  void sendRealTime(uint8_t b) { send3(b, 0, 0); }
  void sendClock() { send3(0xF8, 0, 0); }
  void sendStart() { send3(0xFA, 0, 0); }
  void sendContinue() { send3(0xFB, 0, 0); }
  void sendStop() { send3(0xFC, 0, 0); }
  // byte a byte (arma los mensajes con running status)
  size_t write(uint8_t b);
  bool readPacket(midiEventPacket_t *) { return false; }
  bool read() { return false; }
};

// Llamadas directas a TinyUSB que usan algunos sketches para mandar paquetes crudos
extern "C" void sim_midi_usb(uint8_t s, uint8_t d1, uint8_t d2);
static inline bool tud_midi_n_packet_write(uint8_t, const uint8_t packet[4]) { sim_midi_usb(packet[1], packet[2], packet[3]); return true; }
static inline bool tud_midi_packet_write(const uint8_t packet[4]) { return tud_midi_n_packet_write(0, packet); }
static inline bool tud_midi_n_packet_read(uint8_t, uint8_t[4]) { return false; }
static inline bool tud_midi_packet_read(uint8_t[4]) { return false; }
static inline uint32_t tud_midi_n_available(uint8_t, uint8_t) { return 0; }
static inline uint32_t tud_midi_available(void) { return 0; }
static inline bool tud_midi_mounted(void) { return sim_bus()->usb_nativo != 0; }
static inline bool tud_mounted(void) { return sim_bus()->usb_nativo != 0; }
