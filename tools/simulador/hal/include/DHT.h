// DHT.h — la librería «DHT sensor library» de Adafruit en la placa virtual, con la misma API:
//   DHT dht(3, DHT11);  dht.begin();  dht.readTemperature();  dht.readHumidity();
// Lee el sensor por su protocolo real (sim_dht.h). Como la original, no vuelve a leer antes de
// 2 s (devuelve la lectura anterior) y si el sensor no contesta devuelve NAN.
#pragma once
#include "sim_dht.h"

#define DHT11 11
#define DHT12 12
#define DHT21 21
#define DHT22 22
#define AM2301 21

class DHT {
  uint8_t _pin, _tipo;
  uint8_t data[5];
  uint32_t _ultima = 0;
  bool _resultado = false, _primera = true;
public:
  DHT(uint8_t pin, uint8_t type, uint8_t count = 6) : _pin(pin), _tipo(type) { (void)count; memset(data, 0, 5); }
  void begin(uint8_t usec = 55) { (void)usec; pinMode(_pin, INPUT_PULLUP); _primera = true; }
  bool read(bool force = false) {
    uint32_t ahora = millis();
    if (!force && !_primera && (ahora - _ultima) < 2000) return _resultado;
    _primera = false; _ultima = ahora;
    _resultado = sim_dht_leer(_pin, _tipo, data) == 0;
    return _resultado;
  }
  float readTemperature(bool S = false, bool force = false) {
    if (!read(force)) return NAN;
    float f = sim_dht_temp(_tipo, data);
    return S ? convertCtoF(f) : f;
  }
  float readHumidity(bool force = false) {
    if (!read(force)) return NAN;
    return sim_dht_hum(_tipo, data);
  }
  float convertCtoF(float c) { return c * 1.8f + 32; }
  float convertFtoC(float f) { return (f - 32) * 0.55555f; }
  // índice de calor (fórmula de la NOAA, la misma de la librería)
  float computeHeatIndex(float temperature, float percentHumidity, bool isFahrenheit = true) {
    if (!isFahrenheit) temperature = convertCtoF(temperature);
    float hi = 0.5f * (temperature + 61.0f + ((temperature - 68.0f) * 1.2f) + (percentHumidity * 0.094f));
    if (hi > 79) {
      hi = -42.379f + 2.04901523f * temperature + 10.14333127f * percentHumidity +
           -0.22475541f * temperature * percentHumidity + -0.00683783f * temperature * temperature +
           -0.05481717f * percentHumidity * percentHumidity + 0.00122874f * temperature * temperature * percentHumidity +
           0.00085282f * temperature * percentHumidity * percentHumidity + -0.00000199f * temperature * temperature * percentHumidity * percentHumidity;
      if (percentHumidity < 13 && temperature >= 80 && temperature <= 112)
        hi -= ((13.0f - percentHumidity) * 0.25f) * sqrtf((17.0f - fabsf(temperature - 95.0f)) * 0.05882f);
      else if (percentHumidity > 85 && temperature >= 80 && temperature <= 87)
        hi += ((percentHumidity - 85.0f) * 0.1f) * ((87.0f - temperature) * 0.2f);
    }
    return isFahrenheit ? hi : convertFtoC(hi);
  }
};
