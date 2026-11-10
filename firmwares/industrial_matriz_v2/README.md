# industrial_matriz_v2

Segunda versión de `industrial_matriz`. El motor de sonido es el mismo, ordenado en cuatro
instrumentos que se tocan por separado (batería · bajo · melodías · oscilador), con 5 estilos
(INDUSTRIAL · TRANCE · TECHNO · SYNTHWAVE · FUTURISMO) y la matriz 32×8 en GPIO 46.

## Controles

En **todos** los paneles: **BTN4 = siguiente panel**, **BTN5 = break / vuelta del break**. Mantenido más de 2 s arranca la
**SUBIDA** del v1 (16 compases que terminan en un golpe y esperan en silencio) y el siguiente
toque del BTN5 es el **DROP**.
Sin combos. El LED 5 muestra el panel (verde General · rojo Bajo · ámbar Batería · cian Melodías ·
azul Oscilador). Al cambiar de panel, cada pot toma el control recién cuando lo mueves un 4 %.

| Panel | BTN1 | BTN2 | BTN3 | POT1 | POT2 | POT3 | POT4 |
|---|---|---|---|---|---|---|---|
| General | play/stop | estilo | fill al final del compás | volumen | tempo 70–170 | swing | pasa-bajos general |
| Bajo | suena/calla | patrón | tipo (SIERRAS · SUB · ÁCIDO · REESE) | volumen | filtro | ataque | caída |
| Batería | suena/calla | patrón | kit (INDUSTRIAL · NEXO · 909 · 808 · GLITCH) | volumen | filtro | largo de los golpes | densidad |
| Melodías | suena/calla | patrón | instrumento (KEYS · PLUCK · LEAD · VIDRIO) | volumen | filtro | ataque | caída |
| Oscilador | on/off | timbre | octava (normal · +1 · −1) | **nota** | filtro | LFO | envío al delay |

Las secuencias corren siempre: el BTN1 de cada instrumento sólo lo calla o lo deja sonar, así
que al volver entra en tiempo.

El POT1 del oscilador elige la nota entre 15 grados de la escala del estilo que suena (de una
octava abajo a una arriba de la tónica, que queda al centro), con histéresis por zona. El
oscilador no tiene volumen: un compresor propio lo mantiene parejo en la mezcla aunque cambien
el filtro, el LFO, el timbre o la altura.

El tempo se redondea a BPM enteros con histéresis para que el
ruido del ADC no lo haga bailar.

## Cargar

ESP32S3 Dev Module · **Flash Mode DIO** (con OPI/QIO el I2S suena desfasado y con chasquidos) ·
USB CDC On Boot Enabled · PSRAM OPI. Requiere FastLED.

```
arduino-cli compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashMode=dio,PSRAM=opi" --upload -p COM43 .
```

## Medición en la placa (2026-10-16)

Build con `-DMEDIR=1` (arranca tocando, densidad al máximo, oscilador encendido, imprime por
Serial0 = conector UART cada 2 s): CPU del audio **59 % promedio / 83 % máximo**, limitador sin
actuar con el volumen general bajo. **Con el volumen general alto la mezcla llegaba a 1.6 antes del
limitador** (bajaba hasta −4.7 dB, con muestras en el techo): ése era el clipeo. `MASTER` bajó de
×1.36 a 0.70 y ese mismo momento queda en 0.70–0.92, sin muestras en el techo. Con el compresor
del oscilador: 61 % / 87 %.

## Pendiente

- La matriz: misma prueba de orientación y límite de 450 mA que el v1.
