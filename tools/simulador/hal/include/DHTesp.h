// DHTesp.h — la librería «DHT sensor library for ESPx» (beegee-tokyo) en la placa virtual:
//   DHTesp dht;  dht.setup(3, DHTesp::DHT11);  TempAndHumidity v = dht.getTempAndHumidity();
// Lee el sensor por su protocolo real (sim_dht.h). Si no contesta, devuelve NAN y getStatus() lo dice.
#pragma once
#include "sim_dht.h"

struct TempAndHumidity { float temperature; float humidity; };

class DHTesp {
public:
  typedef enum { AUTO_DETECT, DHT11, DHT22, AM2302, RHT03 } DHT_MODEL_t;
  typedef enum { ERROR_NONE = 0, ERROR_TIMEOUT, ERROR_CHECKSUM } DHT_ERROR_t;
private:
  uint8_t pin = 255;
  DHT_MODEL_t modelo = AUTO_DETECT;
  DHT_ERROR_t error = ERROR_NONE;
  uint32_t ultima = 0;
  bool primera = true;
  TempAndHumidity v = { NAN, NAN };
  int tipo() const { return modelo == DHT11 ? 11 : 22; }
  void leer() {
    if (!primera && millis() - ultima < (uint32_t)getMinimumSamplingPeriod()) return;
    primera = false; ultima = millis();
    uint8_t d[5];
    if (modelo == AUTO_DETECT) {                 // como la librería: prueba DHT22 y si no, DHT11
      modelo = DHT22;
      if (sim_dht_leer(pin, 22, d) != 0) modelo = DHT11;
    }
    int r = sim_dht_leer(pin, tipo(), d);
    error = r == 0 ? ERROR_NONE : r == 1 ? ERROR_TIMEOUT : ERROR_CHECKSUM;
    if (r) { v.temperature = v.humidity = NAN; return; }
    v.temperature = sim_dht_temp(tipo(), d);
    v.humidity = sim_dht_hum(tipo(), d);
  }
public:
  void setup(uint8_t dhtPin, DHT_MODEL_t model = AUTO_DETECT) { pin = dhtPin; modelo = model; primera = true; pinMode(pin, INPUT_PULLUP); }
  void resetTimer() { primera = true; }
  TempAndHumidity getTempAndHumidity() { leer(); return v; }
  float getTemperature() { leer(); return v.temperature; }
  float getHumidity() { leer(); return v.humidity; }
  DHT_ERROR_t getStatus() { return error; }
  const char *getStatusString() { return error == ERROR_NONE ? "OK" : error == ERROR_TIMEOUT ? "TIMEOUT" : "CHECKSUM"; }
  DHT_MODEL_t getModel() { return modelo; }
  int getMinimumSamplingPeriod() { return modelo == DHT11 ? 1000 : 2000; }
  int8_t getNumberOfDecimalsTemperature() { return modelo == DHT11 ? 0 : 1; }
  int8_t getNumberOfDecimalsHumidity() { return modelo == DHT11 ? 0 : 1; }
  int8_t getLowerBoundTemperature() { return modelo == DHT11 ? 0 : -40; }
  int8_t getUpperBoundTemperature() { return modelo == DHT11 ? 50 : 125; }
  int8_t getLowerBoundHumidity() { return modelo == DHT11 ? 20 : 0; }
  int8_t getUpperBoundHumidity() { return modelo == DHT11 ? 90 : 100; }
  static float toFahrenheit(float c) { return c * 1.8f + 32; }
  static float toCelsius(float f) { return (f - 32) * 0.55555f; }
  float computeDewPoint(float t, float h, bool isFahrenheit = false) {
    if (isFahrenheit) t = toCelsius(t);
    double a = 17.271, b = 237.7, x = (a * t) / (b + t) + log(h * 0.01);
    double r = (b * x) / (a - x);
    return isFahrenheit ? toFahrenheit((float)r) : (float)r;
  }
};
