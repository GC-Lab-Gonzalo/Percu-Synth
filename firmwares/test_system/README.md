# test_system — Monitor de sistema del PercuSynth

Firmware de **diagnóstico** que vuelca por el Monitor Serie el estado en vivo de **todos**
los componentes del PercuSynth en una sola pantalla, y permite lanzar auto-tests de
audio y luces escribiendo comandos. Úsalo para verificar una placa recién armada o
depurar una conexión sospechosa antes de cargar un firmware musical.

## Qué monitorea (cada ~250 ms)

| Bloque | Qué muestra |
|--------|-------------|
| `IMU`  | WHO_AM_I (0x68 = OK) + aceleración (g), giro (°/s) y temperatura (°C) |
| `BTN`  | Estado de los 5 botones: `PUL` (pulsado) / `---` (suelto) + su GPIO |
| `POT`  | Lectura cruda `0..4095` + porcentaje de los 4 potenciómetros |
| `PZ`   | Lectura cruda de los 4 piezos (golpéalos para ver el pico) |
| `EXT`  | Lectura cruda de los 2 sensores externos (A/B) |

## Comandos (escribe la letra + Enter en el Monitor Serie)

| Tecla | Acción |
|-------|--------|
| `a` | **Test de audio**: 3 beeps (440/660/880 Hz) por el DAC PCM5102 |
| `l` | **Test de LEDs**: barrido R/G/B por la tira WS2812 *(requiere `ENABLE_LED_TEST = 1`)* |
| `p` | Pausa / reanuda el volcado continuo (para leer con calma) |
| `h` | Ayuda |

## Flags de compilación (arriba del `.ino`)

```c
#define ENABLE_AUDIO_TEST 1   // test de tono I2S (solo core, sin libs externas)
#define ENABLE_LED_TEST   0   // test de tira WS2812 (REQUIERE instalar FastLED)
```

- El test de audio usa solo el core de ESP32 (`driver/i2s_std.h`), así que compila tal cual.
- El test de LEDs está **desactivado por defecto** para no exigir FastLED. Si lo activas,
  ajusta `NUM_LEDS` al largo real de tu tira.

## Arduino IDE — settings críticos

- **Board**: ESP32S3 Dev Module
- **USB CDC On Boot**: Enabled
- **Flash Mode**: **DIO** (necesario para que el audio I2S funcione en este hardware)
- **Monitor Serie**: 115200 baudios

## Pinout usado

| Componente | Pines |
|------------|-------|
| IMU MPU6050 (I2C) | SDA 21, SCL 38 (dir. 0x68) |
| Botones | BTN1→44, BTN2→42, BTN3→0, BTN4→45, BTN5→47 |
| Potenciómetros | POT1→GPIO1, POT2→GPIO2, POT3→GPIO8, POT4→GPIO10 |
| Piezos | PZ1→GPIO4, PZ2→GPIO5, PZ3→GPIO6, PZ4→GPIO7 |
| Sensores externos | EXT_A→GPIO3, EXT_B→GPIO9 |
| DAC PCM5102 (I2S) | LCK 39, DIN 40, BCK 41 |
| Tira LED WS2812 | DATA 46 |
