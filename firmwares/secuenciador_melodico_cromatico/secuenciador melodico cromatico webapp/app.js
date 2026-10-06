const NUM_STEPS = 16;
const NUM_NOTES = 12;

// Cada step es un bitmask de 12 bits (bit N = nota N activa)
let sequence = new Uint16Array([
    (1 << 0),           // Step 0: C
    0,                  // Step 1
    (1 << 7),           // Step 2: G
    0,                  // Step 3
    (1 << 3) | (1 << 10),// Step 4: D# + A# (acorde)
    0,                  // Step 5
    (1 << 7),           // Step 6: G
    0,                  // Step 7
    (1 << 0) | (1 << 12),// Step 8: C
    0,                  // Step 9
    (1 << 5),           // Step 10: F
    0,                  // Step 11
    (1 << 7) | (1 << 3), // Step 12: G + D#
    0,                  // Step 13
    (1 << 10),          // Step 14: A#
    0                   // Step 15
]);

let isPlaying = false;
let serialPort = null;
let serialWriter = null;
let keepReading = true;

// UI Elements
const gridEl = document.getElementById('sequencer-grid');
const valVol = document.getElementById('val-vol');
const barVol = document.getElementById('bar-vol');
const valBpm = document.getElementById('val-bpm');
const valPitch = document.getElementById('val-pitch');
const valEnv = document.getElementById('val-env');
const valWave = document.getElementById('val-wave');
const valMode = document.getElementById('val-mode');
const midiStatusEl = document.getElementById('midi-status');
const btnPlay = document.getElementById('btn-play');
const btnRand = document.getElementById('btn-rand');
const btnClear = document.getElementById('btn-clear');

const NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
const BLACK_KEYS = [1, 3, 6, 8, 10]; // C#, D#, F#, G#, A#

let currentPitch = 0;

function getNoteName(semitone) {
    let midi = 60 + currentPitch + semitone;
    let octave = Math.floor(midi / 12) - 1;
    let note = NOTE_NAMES[midi % 12];
    return `${note}${octave}`;
}

function initUI() {
    gridEl.innerHTML = '';
    
    // 12 filas: de B (nota 11, arriba) a C (nota 0, abajo)
    for (let row = NUM_NOTES - 1; row >= 0; row--) {
        const rowEl = document.createElement('div');
        rowEl.className = 'track';
        if (BLACK_KEYS.includes(row)) rowEl.classList.add('black-key');

        const label = document.createElement('div');
        label.className = 'track-label';
        label.id = `label-row-${row}`;
        label.textContent = getNoteName(row);
        rowEl.appendChild(label);

        const stepsEl = document.createElement('div');
        stepsEl.className = 'steps';

        for (let i = 0; i < NUM_STEPS; i++) {
            const stepEl = document.createElement('div');
            stepEl.className = 'step';
            stepEl.dataset.step = i;
            stepEl.dataset.note = row;
            
            stepEl.addEventListener('click', () => {
                sequence[i] ^= (1 << row);
                updateGridVisuals();
                sendSequenceToHardware();
            });

            stepsEl.appendChild(stepEl);
        }

        rowEl.appendChild(stepsEl);
        gridEl.appendChild(rowEl);
    }
    
    updateGridVisuals();
}

function updateLabels() {
    for (let row = 0; row < NUM_NOTES; row++) {
        let el = document.getElementById(`label-row-${row}`);
        if (el) el.textContent = getNoteName(row);
    }
}

function updateGridVisuals() {
    document.querySelectorAll('.step').forEach(el => {
        el.classList.remove('active');
    });
    
    for (let i = 0; i < NUM_STEPS; i++) {
        for (let n = 0; n < NUM_NOTES; n++) {
            if (sequence[i] & (1 << n)) {
                const el = document.querySelector(`.step[data-step="${i}"][data-note="${n}"]`);
                if (el) el.classList.add('active');
            }
        }
    }
}

function setPlayingState(playing) {
    isPlaying = playing;
    if (isPlaying) {
        btnPlay.innerHTML = '&#9632; STOP';
        btnPlay.classList.add('playing');
    } else {
        btnPlay.innerHTML = '&#9654; START';
        btnPlay.classList.remove('playing');
        document.querySelectorAll('.step').forEach(el => el.classList.remove('current'));
    }
}

// ---- SERIAL ----
async function initSerial() {
    if (!navigator.serial) {
        alert("Web Serial API no soportada en este navegador (usa Google Chrome o Edge).");
        return;
    }
    try {
        serialPort = await navigator.serial.requestPort();
        await serialPort.open({ baudRate: 115200 });
        midiStatusEl.textContent = 'CONNECTED';
        midiStatusEl.classList.add('connected');
        
        const encoder = new TextEncoderStream();
        encoder.readable.pipeTo(serialPort.writable);
        serialWriter = encoder.writable.getWriter();
        
        keepReading = true;
        readSerialLoop();
        
        // Solicitar estado actual al ESP32
        sendSerialCommand("GET_SYNC");

        navigator.serial.addEventListener("disconnect", (e) => {
            if (e.target === serialPort) {
                midiStatusEl.textContent = 'CONNECT SERIAL';
                midiStatusEl.classList.remove('connected');
                serialPort = null;
                serialWriter = null;
                keepReading = false;
                setPlayingState(false);
            }
        });
    } catch (err) {
        console.error(err);
    }
}

async function readSerialLoop() {
    const decoder = new TextDecoderStream();
    serialPort.readable.pipeTo(decoder.writable);
    const reader = decoder.readable.getReader();
    let buffer = "";
    
    try {
        while (keepReading) {
            const { value, done } = await reader.read();
            if (done) break;
            if (value) {
                buffer += value;
                let lines = buffer.split('\n');
                buffer = lines.pop(); 
                for (let line of lines) processHardwareCommand(line.trim());
            }
        }
    } catch (error) {
        console.error(error);
    } finally {
        reader.releaseLock();
    }
}

function processHardwareCommand(cmd) {
    if (!cmd) return;
    
    if (cmd.startsWith("STEP ")) {
        let step = parseInt(cmd.substring(5));
        document.querySelectorAll('.step').forEach(el => el.classList.remove('current'));
        document.querySelectorAll(`.step[data-step="${step}"]`).forEach(el => el.classList.add('current'));
    }
    else if (cmd.startsWith("PLAYING ")) {
        let state = parseInt(cmd.substring(8)) === 1;
        setPlayingState(state);
    }
    else if (cmd.startsWith("BPM ")) {
        valBpm.textContent = cmd.substring(4);
    }
    else if (cmd.startsWith("VOL ")) {
        let vol = parseInt(cmd.substring(4));
        valVol.textContent = vol + "%";
        barVol.style.width = vol + "%";
    }
    else if (cmd.startsWith("PITCH ")) {
        currentPitch = parseInt(cmd.substring(6));
        let sign = currentPitch > 0 ? "+" : "";
        valPitch.textContent = sign + currentPitch + "st";
        updateLabels();
    }
    else if (cmd.startsWith("ENV ")) {
        let parts = cmd.split(' ');
        if (parts.length >= 3) {
            valEnv.textContent = `A:${parts[1]} R:${parts[2]}`;
        }
    }
    else if (cmd.startsWith("WAVE ")) {
        valWave.textContent = cmd.substring(5);
    }
    else if (cmd.startsWith("DIR ")) {
        valMode.textContent = parseInt(cmd.substring(4)) === 1 ? "\u25C0 REV" : "FWD \u25B6";
    }
    else if (cmd.startsWith("SYNC ")) {
        let parts = cmd.split(' ');
        if (parts.length >= NUM_STEPS + 1) {
            for (let i = 0; i < NUM_STEPS; i++) {
                sequence[i] = parseInt(parts[i + 1]);
            }
            updateGridVisuals();
        }
    }
}

async function sendSerialCommand(command) {
    if (!serialWriter) return;
    try { await serialWriter.write(command + "\n"); } catch (e) {}
}

function sendSequenceToHardware() {
    let str = "SEQ";
    for (let i = 0; i < NUM_STEPS; i++) str += " " + sequence[i];
    sendSerialCommand(str);
}

// Event Listeners
btnPlay.addEventListener('click', () => {
    sendSerialCommand("TOGGLE");
});

btnRand.addEventListener('click', () => {
    for (let i = 0; i < NUM_STEPS; i++) {
        sequence[i] = 0;
        for (let n = 0; n < NUM_NOTES; n++) {
            if (Math.random() < 0.18) sequence[i] |= (1 << n);
        }
    }
    updateGridVisuals();
    sendSequenceToHardware();
});

btnClear.addEventListener('click', () => {
    for (let i = 0; i < NUM_STEPS; i++) sequence[i] = 0;
    updateGridVisuals();
    sendSequenceToHardware();
});

midiStatusEl.addEventListener('click', () => { if (!serialPort) initSerial(); });

window.addEventListener('keydown', (e) => {
    if (e.code === 'Space') {
        e.preventDefault();
        sendSerialCommand("TOGGLE");
    }
});

window.addEventListener('DOMContentLoaded', () => {
    initUI();
});
