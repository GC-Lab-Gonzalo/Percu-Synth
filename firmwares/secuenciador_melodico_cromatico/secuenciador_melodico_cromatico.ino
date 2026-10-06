// ==============================================================================================================================================
// PERCU-SYNTH — Secuenciador Melodico Cromatico (16 steps, 12 notes, Polifonico)
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
#define NUM_LEDS     6

CRGB leds[NUM_LEDS];
ESP32Synth synth;

// -----------------------------------------------------------------------------
// ESTADO DEL SECUENCIADOR
// -----------------------------------------------------------------------------
#define NUM_STEPS   16
#define NUM_NOTES   12  // Octava cromática: 0=C, 1=C#, 2=D, 3=D#, 4=E, 5=F, 6=F#, 7=G, 8=G#, 9=A, 10=A#, 11=B
#define POLY_VOICES 12  // Voces dedicadas 0..11

// Cada step es un bitmask de 12 bits: bit N = nota N activa
uint16_t sequence[NUM_STEPS] = {
    (1 << 0),          // Step 0: C4
    0,                 // Step 1
    (1 << 7),          // Step 2: G4
    0,                 // Step 3
    (1 << 3) | (1<<10),// Step 4: D#4 + A#4 (acorde)
    0,                 // Step 5
    (1 << 7),          // Step 6: G4
    0,                 // Step 7
    (1 << 0) | (1<<12),// Step 8: C4
    0,                 // Step 9
    (1 << 5),          // Step 10: F4
    0,                 // Step 11
    (1 << 7) | (1<<3), // Step 12: G4 + D#4
    0,                 // Step 13
    (1 << 10),         // Step 14: A#4
    0                  // Step 15
};

int currentPitch = 0; // -12 a +12 semitonos (transposición)
int baseMidi = 60;    // C4

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
int lastPotEnv = -1;

// Waveform cycling con enum WaveType válido de ESP32Synth
uint8_t currentWaveIdx = 0;
const WaveType WAVE_TYPES[] = { WAVE_PULSE, WAVE_SAW, WAVE_TRIANGLE, WAVE_SINE };
const char* WAVE_NAMES[] = { "PULSE", "SAW", "TRI", "SINE" };
#define NUM_WAVE_TYPES 4

String serialBuffer = "";

// -----------------------------------------------------------------------------
// FUNCIONES AUXILIARES
// -----------------------------------------------------------------------------
void ledsSetup() {
    FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(40);
    FastLED.clear(); 
    FastLED.show();
}

void updateLEDs() {
    FastLED.clear(); 
    int ledIdx = currentStep % NUM_LEDS;
    
    if (isPlaying) {
        uint16_t mask = sequence[currentStep];
        if (mask != 0) {
            int lowestNote = 0;
            for (int n = 0; n < NUM_NOTES; n++) {
                if (mask & (1 << n)) { lowestNote = n; break; }
            }
            leds[ledIdx] = CHSV(lowestNote * 21, 255, 255);
        } else {
            leds[ledIdx] = CRGB::White;
        }
    }
    
    FastLED.show();
}

int leerPot(uint8_t pin) {
    uint32_t s = 0;
    for (int i = 0; i < 8; i++) s += analogRead(pin);
    return s >> 3;
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

uint32_t getFreqCentiHz(int semitone) {
    int midiNote = baseMidi + currentPitch + semitone;
    float freqHz = 440.0f * powf(2.0f, (midiNote - 69) / 12.0f);
    return (uint32_t)(freqHz * 100.0f);
}

void setupVoices() {
    for (int v = 0; v < POLY_VOICES; v++) {
        synth.setWave(v, WAVE_TYPES[currentWaveIdx]);
        if (WAVE_TYPES[currentWaveIdx] == WAVE_PULSE) {
            synth.setPulseWidth(v, 127);
        }
        synth.setEnv(v, 10, 150, 180, 200);
    }
}

void allNotesOff() {
    for (int v = 0; v < POLY_VOICES; v++) {
        synth.noteOff(v);
    }
}

void playStep(int step) {
    allNotesOff();
    uint16_t mask = sequence[step];
    for (int n = 0; n < NUM_NOTES; n++) {
        if (mask & (1 << n)) {
            synth.noteOn(n, getFreqCentiHz(n), 240);
        }
    }
}

// -----------------------------------------------------------------------------
// COMUNICACIÓN SERIAL
// -----------------------------------------------------------------------------
void sendSyncToWeb() {
    Serial.print("SYNC");
    for (int i = 0; i < NUM_STEPS; i++) {
        Serial.print(" ");
        Serial.print(sequence[i]);
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
                        sequence[i] = (uint16_t)serialBuffer.substring(index, space).toInt();
                    }
                    index = space + 1;
                }
                updateLEDs();
            } else if (serialBuffer.equalsIgnoreCase("PLAY")) {
                isPlaying = true;
                currentStep = 0;
                lastStepTime = millis();
                Serial.println("PLAYING 1");
                updateLEDs();
            } else if (serialBuffer.equalsIgnoreCase("STOP")) {
                isPlaying = false;
                allNotesOff();
                Serial.println("PLAYING 0");
                updateLEDs();
            } else if (serialBuffer.equalsIgnoreCase("TOGGLE")) {
                isPlaying = !isPlaying;
                if (isPlaying) {
                    currentStep = 0;
                    lastStepTime = millis();
                } else {
                    allNotesOff();
                }
                Serial.print("PLAYING "); Serial.println(isPlaying ? 1 : 0);
                updateLEDs();
            } else if (serialBuffer.equalsIgnoreCase("GET_SYNC")) {
                sendSyncToWeb();
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
        Serial.println("ERROR: ESP32Synth I2S init failed");
    }
    synth.setMasterVolume(220); // Volumen predeterminado alto y audible
    setupVoices();
    
    delay(500);
    Serial.println("PERCUSYNTH READY");
    Serial.print("PLAYING "); Serial.println(isPlaying ? 1 : 0);
    Serial.print("DIR "); Serial.println(isReverse ? 1 : 0);
    Serial.print("PITCH "); Serial.println(currentPitch);
    Serial.print("WAVE "); Serial.println(WAVE_NAMES[currentWaveIdx]);
    Serial.print("BPM "); Serial.println(bpm);
    sendSyncToWeb();
}

// -----------------------------------------------------------------------------
// LOOP
// -----------------------------------------------------------------------------
void loop() {
    uint32_t now = millis();
    
    // 1. POTENCIÓMETROS
    // POT1: Volumen
    int potVol = leerPot(POT_PINS[0]);
    if (abs(potVol - lastPotVol) > 40) {
        lastPotVol = potVol;
        uint8_t masterVol = map(potVol, 0, 4095, 0, 255);
        synth.setMasterVolume(masterVol);
        int volPercent = map(potVol, 0, 4095, 0, 100);
        Serial.print("VOL "); Serial.println(volPercent);
    }
    
    // POT2: BPM
    int potBPM = leerPot(POT_PINS[1]);
    int newBPM = map(potBPM, 0, 4095, 60, 240);
    if (newBPM != lastBPM) {
        lastBPM = newBPM;
        bpm = newBPM;
        Serial.print("BPM "); Serial.println(bpm);
    }
    
    // POT3: Pitch (-12..+12 semitonos)
    int potPitch = leerPot(POT_PINS[2]);
    int newPitch = map(potPitch, 0, 4095, -12, 12);
    if (newPitch != lastPotPitch) {
        lastPotPitch = newPitch;
        currentPitch = newPitch;
        Serial.print("PITCH "); Serial.println(currentPitch);
    }
    
    // POT4: Attack/Release desde el centro
    int potEnv = leerPot(POT_PINS[3]);
    if (abs(potEnv - lastPotEnv) > 40) {
        lastPotEnv = potEnv;
        uint16_t att = 10;
        uint16_t rel = 100;
        if (potEnv < 2048) {
            att = map(potEnv, 2047, 0, 10, 800);
            rel = 100;
        } else {
            att = 10;
            rel = map(potEnv, 2048, 4095, 100, 1200);
        }
        for (int v = 0; v < POLY_VOICES; v++) {
            synth.setEnv(v, att, 150, 180, rel);
        }
        Serial.print("ENV "); Serial.print(att); Serial.print(" "); Serial.println(rel);
    }

    // 2. BOTONES FÍSICOS
    // BTN1: Play/Stop
    if (botonFlanco(0, now)) {
        isPlaying = !isPlaying;
        if (isPlaying) {
            currentStep = 0;
            lastStepTime = now;
        } else {
            allNotesOff();
        }
        Serial.print("PLAYING "); Serial.println(isPlaying ? 1 : 0);
        updateLEDs();
    }
    
    // BTN2: Waveform (Pulse -> Saw -> Tri -> Sine)
    if (botonFlanco(1, now)) {
        currentWaveIdx = (currentWaveIdx + 1) % NUM_WAVE_TYPES;
        for (int v = 0; v < POLY_VOICES; v++) {
            synth.setWave(v, WAVE_TYPES[currentWaveIdx]);
            if (WAVE_TYPES[currentWaveIdx] == WAVE_PULSE) {
                synth.setPulseWidth(v, 127);
            }
        }
        Serial.print("WAVE "); Serial.println(WAVE_NAMES[currentWaveIdx]);
    }
    
    // BTN3: Reversa
    if (botonFlanco(2, now)) {
        isReverse = !isReverse;
        Serial.print("DIR "); Serial.println(isReverse ? 1 : 0);
    }
    
    // BTN4: Randomize
    if (botonFlanco(3, now)) {
        for (int i = 0; i < NUM_STEPS; i++) {
            sequence[i] = 0;
            for (int n = 0; n < NUM_NOTES; n++) {
                if (random(100) < 18) sequence[i] |= (1 << n);
            }
        }
        sendSyncToWeb();
        updateLEDs();
    }
    
    // BTN5: Clear
    if (botonFlanco(4, now)) {
        for (int i = 0; i < NUM_STEPS; i++) sequence[i] = 0;
        sendSyncToWeb();
        updateLEDs();
    }
    
    // 3. RELOJ DEL SECUENCIADOR
    if (isPlaying) {
        uint32_t stepDurationMs = (60000 / bpm) / 4; // Semicorcheas (1/16th)
        
        if (now - lastStepTime >= stepDurationMs) {
            lastStepTime = now;
            
            if (isReverse) {
                currentStep = (currentStep - 1 + NUM_STEPS) % NUM_STEPS;
            } else {
                currentStep = (currentStep + 1) % NUM_STEPS;
            }
            
            Serial.print("STEP "); Serial.println(currentStep);
            playStep(currentStep);
            updateLEDs();
        }
    }
    
    // 4. PARSER SERIAL WEB
    parseSerial();
    
    delay(2);
}
