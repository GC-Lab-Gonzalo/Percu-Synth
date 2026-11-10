// =============================================================================
// sim_bus.h — la memoria que comparten la placa virtual (WebAssembly) y la
// interfaz (JavaScript). La interfaz escribe botones, perillas y cables; la
// placa escribe LEDs, audio, Serial y MIDI. Todo vive en la memoria compartida
// del módulo, así que no hay mensajes entre hilos: cada lado lee y escribe.
//
// Los anillos (audio, Serial, MIDI) tienen un solo escritor y un solo lector:
// el escritor sólo mueve `w`, el lector sólo mueve `r`, y los dos contadores
// corren libres (se usan módulo el tamaño). Así no hace falta ningún candado.
//
// Las posiciones de cada campo NO se escriben a mano en el JS: sim_layout()
// las publica con offsetof() al arrancar.
// =============================================================================
#pragma once
#include <stdint.h>

#define SIM_MAX_LEDS      1024     // LEDs WS2812 en el GPIO 46 (6 de la placa + tira/matriz)
#define SIM_AUDIO_FRAMES  8192     // frames estéreo int16 en el anillo de audio (potencia de 2)
#define SIM_RING          16384    // bytes de cada anillo de Serial / MIDI (potencia de 2)
#define SIM_MIC_FRAMES    16384    // muestras del anillo del micrófono (potencia de 2)

struct SimRing {
  volatile uint32_t w;             // bytes escritos (corre libre)
  volatile uint32_t r;             // bytes leídos   (corre libre)
  uint8_t buf[SIM_RING];
};

struct SimBus {
  uint32_t magic;                  // 0x50534D31 ("PSM1"): la interfaz verifica que leyó bien

  // --- entradas que pone la interfaz ---------------------------------------
  volatile int32_t pin_in[64];     // nivel de cada GPIO de entrada (1 = alto). Botón suelto = 1
  volatile int32_t adc[64];        // lectura cruda 0..4095 de cada GPIO analógico
  volatile uint32_t piezo_seq[4];  // la interfaz lo sube en cada golpe al piezo 1..4 (GPIO 4..7)
  volatile float    piezo_amp[4];  // fuerza del golpe 0..1
  volatile int32_t usb_nativo;     // cable en el conector USB (nativo: Serial + USB-MIDI)
  volatile int32_t usb_com;        // cable en el conector COM (puente UART: Serial0)
  volatile int32_t audio_live;     // 1 = la interfaz está consumiendo el anillo de audio
  volatile int32_t arranque;       // la interfaz lo pone en 1 cuando ya escribió las entradas: recién ahí corre setup()
  volatile int32_t imu_on;         // MPU6050 puesto en el header (I2C 0x68)
  volatile float   imu_acc[3];     // aceleración en g (x, y, z) según la inclinación que da la interfaz
  volatile float   imu_gyr[3];     // velocidad de giro en °/s
  volatile uint32_t piso_seq;      // la interfaz lo sube en cada golpe al piso (pico en el acelerómetro)
  volatile float    piso_amp;      // fuerza del golpe en g
  volatile int32_t oled_on;        // SSD1306 puesta en el header (I2C 0x3C). Comparte el header con el IMU
  volatile int32_t sd_on;          // tarjeta microSD puesta (SPI 14/15/16/17)
  volatile uint32_t mic_w, mic_r;  // micrófono del PC (INMP441): frames mono escritos / leídos
  volatile int32_t  mic_rate;      // frecuencia a la que la interfaz escribe el anillo del micrófono
  volatile int32_t  mic_live;      // 1 = la interfaz está escribiendo el micrófono
  float mic[SIM_MIC_FRAMES];       // muestras -1..1
  volatile int32_t dht_tipo[64];   // sensor DHT con su DATA en ese GPIO: 0 = nada, 11 = DHT11, 22 = DHT22
  volatile float   dht_temp[64];   // °C que mide
  volatile float   dht_hum[64];    // % de humedad relativa

  // --- salidas que pone la placa -------------------------------------------
  volatile int32_t pin_mode[64];   // lo que pidió pinMode()
  volatile int32_t pin_out[64];    // lo que escribió digitalWrite()
  volatile uint32_t led_seq;       // sube en cada FastLED.show()
  volatile int32_t  led_count;     // LEDs declarados en el GPIO 46
  uint8_t led_rgb[SIM_MAX_LEDS * 3];   // colores ya escalados por el brillo
  volatile uint32_t rgb48_seq;     // LED RGB del módulo ESP32-S3 (GPIO 48)
  uint8_t rgb48[3];
  uint8_t _pad48;

  volatile int32_t  audio_rate;    // frecuencia que configuró el sketch en el I2S
  volatile uint32_t a_w, a_r;      // frames escritos / leídos (corren libres)
  int16_t audio[SIM_AUDIO_FRAMES * 2];
  volatile uint32_t audio_huecos;  // veces que el audio se quedó sin datos (el DMA vacío del ESP32)

  SimRing tx[3];                   // 0 = Serial (USB nativo), 1 = Serial0 (COM), 2 = Serial1 (DIN-5)
  SimRing rx[2];                   // del monitor serie hacia Serial / Serial0
  SimRing usbmidi;                 // mensajes USB-MIDI de 3 bytes (status, d1, d2)

  volatile int32_t  serial1_baud;  // 31250 si el sketch abrió el DIN-5 como MIDI
  volatile int32_t  reinicio;      // ESP.restart() lo pone en 1: la interfaz recarga la placa
  volatile int32_t  hilos;         // tareas de FreeRTOS creadas (informativo)

  // pantalla OLED SSD1306: la RAM del chip (8 páginas × 128 columnas) y su configuración
  volatile uint32_t oled_seq;      // sube en cada escritura a la pantalla
  uint8_t oled_ram[1024];
  volatile int32_t oled_cfg[8];    // encendida, invertida, contraste, A1 (columnas espejadas), C8 (filas espejadas),
                                   // línea de inicio, desplazamiento, todo encendido (A5)
  volatile uint32_t sd_seq;        // sube cada vez que cambia un archivo de la microSD
  volatile int32_t  i2s_rx;        // el sketch abrió un canal I2S de entrada (micrófono)
};

#ifdef __cplusplus
extern "C" {
#endif
SimBus *sim_bus();
#ifdef __cplusplus
}
#endif
