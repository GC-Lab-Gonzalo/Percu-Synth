# Prompt para crear firmwares con IA

Copia el texto de abajo y pégalo en el chat de tu asistente de IA favorito (Claude, ChatGPT, Gemini, etc.). **Adjunta también la imagen del pinout** que está en `Imagenes/percu-synth pinout.jpeg` — así la IA conoce el hardware y puede escribir código directamente compatible.

---

## PROMPT — copiar desde aquí

```
Estoy trabajando con una placa de experimentación llamada PercuSynth, desarrollada por GC Lab Chile. Es un laboratorio portátil basado en ESP32 para explorar síntesis de audio, percusión electrónica, MIDI y control gestual, con enfoque educativo y metodología STEAM.

Te adjunto la imagen del pinout del hardware para que conozcas la disposición exacta de los pines.

### Hardware disponible

- **Microcontrolador:** ESP32 (o ESP32-S3)
- **DAC de audio:** PCM5102 conectado por I2S
  - I2S LRCK → pin 39
  - I2S DATA → pin 40
  - I2S BCLK → pin 41
  - Sample rate: 44100 Hz, 16-bit estéreo
- **Botones (5):** pines 44, 42, 0, 45, 47 — configurados como INPUT_PULLUP
- **Potenciómetros (4):** pines ADC 1, 2, 8, 10
- **Sensores piezoeléctricos (4):** pines ADC 4, 5, 6, 7
- **IMU MPU6050:** conectado por I2C (acelerómetro + giroscopio)

### Estilo del código existente

- Firmwares en Arduino (.ino) para ESP32
- Audio generado en el loop principal llenando buffers de 128 muestras con i2s_channel_write()
- Sin uso de librerías externas de audio (síntesis propia)
- Las lecturas de potenciómetros usan 8x oversampling
- Los botones se leen con debounce de ~25 ms

### Lo que necesito

[DESCRIBE AQUÍ LO QUE QUIERES HACER]

Ejemplos de lo que puedes pedir:
- "Quiero un oscilador simple donde cada botón toque una nota diferente y el potenciómetro controle el volumen"
- "Quiero un secuenciador de 8 pasos donde el tempo se controle con un potenciómetro"
- "Quiero que los piezos detecten golpes y envíen notas MIDI USB"
- "Quiero un efecto de eco/delay sobre la salida de audio"
- "Quiero usar el IMU para controlar el pitch de un oscilador con el movimiento"

Por favor escribe el código completo listo para cargar en Arduino IDE.
```

---

## Consejos para mejores resultados

- **Sé específico:** describe el comportamiento que quieres, no solo el nombre del efecto
- **Itera:** si el código no compila o no suena como esperabas, muéstraselo a la IA y pídele que lo corrija
- **Pide explicaciones:** puedes pedirle que explique cada parte del código mientras lo escribe
- **Empieza simple:** una función básica primero, luego agrega complejidad
- **Guarda lo que funciona:** antes de hacer cambios grandes, guarda una copia del firmware que ya suena bien
