// ==============================================================================================================================================
// PERCU-SYNTH — Secuenciador Melodico (16 steps, Scales, Pitch)
// ==============================================================================================================================================
#include <Arduino.h>
#include <FastLED.h>
#include <ESP32Synth.h>

// -----------------------------------------------------------------------------
// DEFINICIONES DE HARDWARE
// -----------------------------------------------------------------------------
#define I2S_LCK 39
#define I2S_DIN 40
#define I2S_BCK 41

const uint8_t BTN_PINS[5] = { 44, 42, 0, 45, 47 };
const uint8_t POT_PINS[4] = { 1, 2, 8, 10 };

#define LED_PIN      46
#define START_LED    0
#define NUM_LEDS     6 // Usamos los 6 LEDs internos como solicitó el usuario

CRGB leds[NUM_LEDS];
ESP32Synth synth;

// -----------------------------------------------------------------------------
// ESTADO DEL SECUENCIADOR Y HARDWARE
// -----------------------------------------------------------------------------
#define NUM_STEPS 16
int8_t sequence[NUM_STEPS] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }; 

const int SCALES[7][8] = {
    {0, 2, 4, 5, 7, 9, 11, 12}, // Ionian (Mayor)
    {0, 2, 3, 5, 7, 9, 10, 12}, // Dorian
    {0, 1, 3, 5, 7, 8, 10, 12}, // Phrygian
    {0, 2, 4, 6, 7, 9, 11, 12}, // Lydian
    {0, 2, 4, 5, 7, 9, 10, 12}, // Mixolydian
    {0, 2, 3, 5, 7, 8, 10, 12}, // Aeolian (Menor)
    {0, 1, 3, 5, 6, 8, 10, 12}  // Locrian
};

int currentScaleIdx = 0;
int currentPitch = 0; // -12 a +12 semitonos
int baseMidi = 60; // C4

bool isPlaying = false;
bool isReverse = false;

int currentStep = 0;
uint32_t lastStepTime = 0;
int bpm = 120;
int lastBPM = -1;

// Variables Botones
bool btnPrev[5] = {true, true, true, true, true};
uint32_t btnLastMs[5] = {0,0,0,0,0};
#define DEBOUNCE_MS 35

// Variables Pots
int lastPotVol = -1;
int lastPotPitch = -999;
int lastPotSustain = -1;

String serialBuffer = "";

// -----------------------------------------------------------------------------
// FUNCIONES AUXILIARES
// -----------------------------------------------------------------------------
void ledsSetup() {
    FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(40); // Brillo moderado para no encandilar con los internos
    FastLED.clear(); 
    FastLED.show();
}

void updateLEDs() {
    FastLED.clear(); 
    
    // Como tenemos 16 pasos y solo 6 LEDs físicos, 
    // el "cursor" dará vueltas cíclicamente (currentStep % 6)
    int ledIdx = currentStep % NUM_LEDS;
    
    if (isPlaying) {
        leds[ledIdx] = CRGB::White;
    }
    
    // Si la nota actual tiene sonido, parpadear un color extra sobre ella
    if (sequence[currentStep] != -1) {
        leds[ledIdx] = CHSV(sequence[currentStep] * 32, 255, 255);
    }
    
    FastLED.show();
}

int leerPot(uint8_t pin) {
    uint32_t s = 0;
    for (int i = 0; i < 8; i++) s += analogRead(pin);
    return s >> 3; // 0..4095
}

bool botonFlanco(uint8_t i, uint32_t now) {
    bool pressed = (digitalRead(BTN_PINS[i]) == LOW);
    bool edge = false;
    if (pressed != btnPrev[i] && (now - btnLastMs[i]) > DEBOUNCE_MS) {
        btnLastMs[i] = now;
        btnPrev[i] = pressed;
        if (pressed) edge = true;
    }
    return edge;
}

uint32_t getFreqCentiHz(int noteIndex) {
    int midiNote = baseMidi + currentPitch + SCALES[currentScaleIdx][noteIndex];
    float freqHz = 440.0f * powf(2.0f, (midiNote - 69) / 12.0f);
    return (uint32_t)(freqHz * 100.0f);
}

// -----------------------------------------------------------------------------
// COMUNICACIÓN SERIAL
// -----------------------------------------------------------------------------
void sendSyncToWeb() {
    Serial.print("SYNC ");
    for(int i=0; i<NUM_STEPS; i++) {
        Serial.print(sequence[i]);
        if(i < NUM_STEPS - 1) Serial.print(" ");
    }
    Serial.println();
}

void parseSerial() {
    while (Serial.available() > 0) {
        char c = Serial.read();
        if (c == '\n') {
            serialBuffer.trim();
            if (serialBuffer.startsWith("SEQ ")) {
                int index = 4;
                for (int i = 0; i < NUM_STEPS; i++) {
                    int space = serialBuffer.indexOf(' ', index);
                    if (space == -1) space = serialBuffer.length();
                    if (index < serialBuffer.length()) {
                        sequence[i] = serialBuffer.substring(index, space).toInt();
                    }
                    index = space + 1;
                }
                updateLEDs();
            }
            serialBuffer = "";
        } else if (c != '\r') {
            serialBuffer += c;
        }
    }
}

// -----------------------------------------------------------------------------
// SETUP
// -----------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    
    for (int i = 0; i < 5; i++) pinMode(BTN_PINS[i], INPUT_PULLUP);
    analogReadResolution(12); 
    analogSetAttenuation(ADC_11db);
    
    ledsSetup();

    if (!synth.begin(I2S_BCK, I2S_LCK, I2S_DIN)) {
        Serial.println("Error ESP32Synth.");
    }
    synth.setMasterVolume(100);
    
    synth.setWave(0, WAVE_PULSE);
    synth.setPulseWidth(0, 127);
    synth.setEnv(0, 10, 150, 60, 300);
    
    // Forzar el envío del estado inicial a la interfaz web si estuviera conectada
    delay(1000);
    Serial.print("SCALE "); Serial.println(currentScaleIdx);
    Serial.print("DIR "); Serial.println(isReverse ? 1 : 0);
    Serial.print("PITCH "); Serial.println(currentPitch);
}

// -----------------------------------------------------------------------------
// LOOP
// -----------------------------------------------------------------------------
void loop() {
    uint32_t now = millis();
    
    // 1. LEER POTENCIÓMETROS
    int potVol = leerPot(POT_PINS[0]);
    if (abs(potVol - lastPotVol) > 40) { // Tolerancia para evitar ruido
        lastPotVol = potVol;
        uint8_t masterVol = map(potVol, 0, 4095, 0, 255);
        synth.setMasterVolume(masterVol);
        int volPercent = map(potVol, 0, 4095, 0, 100);
        Serial.print("VOL "); Serial.println(volPercent);
    }
    
    int potBPM = leerPot(POT_PINS[1]);
    int newBPM = map(potBPM, 0, 4095, 60, 240);
    if (newBPM != lastBPM) {
        lastBPM = newBPM;
        bpm = newBPM;
        Serial.print("BPM "); Serial.println(bpm);
    }
    
    int potPitch = leerPot(POT_PINS[2]);
    // Mapear de 0-4095 a -12..12 discretos
    int newPitch = map(potPitch, 0, 4095, -12, 12);
    if (newPitch != lastPotPitch) {
        lastPotPitch = newPitch;
        currentPitch = newPitch;
        Serial.print("PITCH "); Serial.println(currentPitch);
    }
    
    int potSustain = leerPot(POT_PINS[3]);
    if (abs(potSustain - lastPotSustain) > 40) {
        lastPotSustain = potSustain;
        
        uint16_t att = 10;
        uint16_t rel = 50;
        
        if (potSustain < 2048) {
            // Mitad izquierda: Aumenta Attack, Release corto
            att = map(potSustain, 2047, 0, 10, 1000);
            rel = 50;
        } else {
            // Mitad derecha: Attack corto, aumenta Release
            att = 10;
            rel = map(potSustain, 2048, 4095, 50, 1500);
        }
        
        synth.setEnv(0, att, 150, 60, rel);
        Serial.print("ENV ");
        Serial.print(att);
        Serial.print(" ");
        Serial.println(rel);
    }

    // 2. LEER BOTONES
    if (botonFlanco(0, now)) { // BTN 1: Play/Stop
        isPlaying = !isPlaying;
        if(isPlaying) {
            currentStep = 0;
            lastStepTime = now;
        } else {
            synth.noteOff(0);
        }
        updateLEDs();
    }
    
    if (botonFlanco(1, now)) { // BTN 2: Cambiar Escala (Modos Griegos)
        currentScaleIdx = (currentScaleIdx + 1) % 7;
        Serial.print("SCALE "); Serial.println(currentScaleIdx);
    }
    
    if (botonFlanco(2, now)) { // BTN 3: Reversa
        isReverse = !isReverse;
        Serial.print("DIR "); Serial.println(isReverse ? 1 : 0);
    }
    
    if (botonFlanco(3, now)) { // BTN 4: Randomize
        for(int i=0; i<NUM_STEPS; i++) {
            sequence[i] = (random(100) < 40) ? -1 : random(0, 8);
        }
        sendSyncToWeb();
        updateLEDs();
    }
    
    if (botonFlanco(4, now)) { // BTN 5: Clear
        for(int i=0; i<NUM_STEPS; i++) sequence[i] = -1;
        sendSyncToWeb();
        updateLEDs();
    }
    
    // 3. SECUENCIADOR Y RELOJ
    if (isPlaying) {
        uint32_t stepDurationMs = (60000 / bpm) / 4; // Semicorcheas (1/4 beat) para los 16 steps
        
        if (now - lastStepTime >= stepDurationMs) {
            lastStepTime = now;
            
            if (isReverse) {
                currentStep = (currentStep - 1 + NUM_STEPS) % NUM_STEPS;
            } else {
                currentStep = (currentStep + 1) % NUM_STEPS;
            }
            
            Serial.print("STEP "); Serial.println(currentStep);
            
            int noteIdx = sequence[currentStep];
            if (noteIdx != -1) {
                synth.noteOff(0); 
                synth.noteOn(0, getFreqCentiHz(noteIdx), 255);
            }
            
            updateLEDs();
        }
    }
    
    // 4. PARSER SERIAL WEB
    parseSerial();
    
    delay(2);
}
