const NUM_STEPS = 16;
let sequence = Array(NUM_STEPS).fill(-1);

let serialPort = null;
let serialWriter = null;
let serialReader = null;
let keepReading = true;

// UI Elements
const gridEl = document.getElementById('sequencer-grid');
const valVol = document.getElementById('val-vol');
const valBpm = document.getElementById('val-bpm');
const valPitch = document.getElementById('val-pitch');
const valEnv = document.getElementById('val-env');
const valScale = document.getElementById('val-scale');
const valMode = document.getElementById('val-mode');
const midiStatusEl = document.getElementById('midi-status');

const SCALES = [
    { name: "Ionian (Mayor)", intervals: [0, 2, 4, 5, 7, 9, 11, 12] },
    { name: "Dorian",         intervals: [0, 2, 3, 5, 7, 9, 10, 12] },
    { name: "Phrygian",       intervals: [0, 1, 3, 5, 7, 8, 10, 12] },
    { name: "Lydian",         intervals: [0, 2, 4, 6, 7, 9, 11, 12] },
    { name: "Mixolydian",     intervals: [0, 2, 4, 5, 7, 9, 10, 12] },
    { name: "Aeolian (Menor)",intervals: [0, 2, 3, 5, 7, 8, 10, 12] },
    { name: "Locrian",        intervals: [0, 1, 3, 5, 6, 8, 10, 12] }
];

const NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];

let currentScaleIdx = 0;
let currentPitch = 0; // -12 a +12

function getNoteName(baseMidi, interval) {
    let midi = baseMidi + interval;
    let octave = Math.floor(midi / 12) - 1;
    let note = NOTE_NAMES[midi % 12];
    return `${note}${octave}`;
}

function initUI() {
    gridEl.innerHTML = '';
    
    // Generar 8 filas para las 8 notas de la escala (de Octava a Tónica, invertido visualmente)
    for (let row = 7; row >= 0; row--) {
        const rowEl = document.createElement('div');
        rowEl.className = 'track';

        const label = document.createElement('div');
        label.className = 'track-label';
        label.id = `label-row-${row}`;
        rowEl.appendChild(label);

        const stepsEl = document.createElement('div');
        stepsEl.className = 'steps';

        for (let i = 0; i < NUM_STEPS; i++) {
            const stepEl = document.createElement('div');
            stepEl.className = 'step';
            stepEl.dataset.step = i;
            stepEl.dataset.note = row; // 0 = Tónica, 7 = Octava
            
            stepEl.addEventListener('click', () => {
                if (sequence[i] === row) sequence[i] = -1;
                else sequence[i] = row;
                updateGridVisuals();
                sendSequenceToHardware();
            });

            stepsEl.appendChild(stepEl);
        }

        rowEl.appendChild(stepsEl);
        gridEl.appendChild(rowEl);
    }
    
    updateLabels();
    updateGridVisuals();
}

function updateLabels() {
    let baseMidi = 60 + currentPitch; // C4 = 60
    let scale = SCALES[currentScaleIdx];
    
    for (let row = 0; row < 8; row++) {
        let el = document.getElementById(`label-row-${row}`);
        if (el) {
            el.textContent = getNoteName(baseMidi, scale.intervals[row]);
        }
    }
    valScale.textContent = scale.name;
}

function updateGridVisuals() {
    document.querySelectorAll('.step').forEach(el => {
        el.classList.remove('active');
        el.style.background = 'var(--step-off)';
        el.style.boxShadow = 'none';
    });
    
    for (let i = 0; i < NUM_STEPS; i++) {
        const noteIndex = sequence[i];
        if (noteIndex !== -1) {
            const el = document.querySelector(`.step[data-step="${i}"][data-note="${noteIndex}"]`);
            if (el) {
                el.classList.add('active');
                el.style.background = `hsl(${noteIndex * 40}, 100%, 50%)`;
                el.style.boxShadow = `0 0 10px hsla(${noteIndex * 40}, 100%, 50%, 0.5)`;
            }
        }
    }
}

async function initSerial() {
    if (!navigator.serial) {
        alert("Web Serial API no soportada.");
        return;
    }
    try {
        serialPort = await navigator.serial.requestPort();
        await serialPort.open({ baudRate: 115200 });
        midiStatusEl.textContent = 'Serial: Conectado';
        midiStatusEl.classList.add('connected');
        
        const encoder = new TextEncoderStream();
        encoder.readable.pipeTo(serialPort.writable);
        serialWriter = encoder.writable.getWriter();
        
        keepReading = true;
        readSerialLoop();
        sendSequenceToHardware();

        navigator.serial.addEventListener("disconnect", (e) => {
            if (e.target === serialPort) {
                midiStatusEl.textContent = 'Conectar Serial';
                midiStatusEl.classList.remove('connected');
                serialPort = null;
                serialWriter = null;
                keepReading = false;
            }
        });
    } catch (err) {
        console.error(err);
        midiStatusEl.textContent = 'Conectar Serial (Error)';
        midiStatusEl.classList.remove('connected');
    }
}

async function readSerialLoop() {
    const decoder = new TextDecoderStream();
    serialPort.readable.pipeTo(decoder.writable);
    const reader = decoder.readable.getReader();
    serialReader = reader;
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
    else if (cmd.startsWith("BPM ")) {
        valBpm.textContent = cmd.substring(4);
    }
    else if (cmd.startsWith("VOL ")) {
        valVol.textContent = cmd.substring(4) + " %";
    }
    else if (cmd.startsWith("PITCH ")) {
        currentPitch = parseInt(cmd.substring(6));
        valPitch.textContent = (currentPitch > 0 ? "+" : "") + currentPitch + " st";
        updateLabels();
    }
    else if (cmd.startsWith("ENV ")) {
        let parts = cmd.split(' ');
        if (parts.length >= 3) {
            valEnv.textContent = `A:${parts[1]} R:${parts[2]}`;
        }
    }
    else if (cmd.startsWith("SCALE ")) {
        currentScaleIdx = parseInt(cmd.substring(6));
        updateLabels();
    }
    else if (cmd.startsWith("DIR ")) {
        valMode.textContent = parseInt(cmd.substring(4)) === 1 ? "Reverse" : "Forward";
    }
    else if (cmd.startsWith("SYNC ")) {
        let parts = cmd.split(' ');
        if (parts.length === NUM_STEPS + 1) {
            for(let i=0; i<NUM_STEPS; i++) sequence[i] = parseInt(parts[i+1]);
            updateGridVisuals();
        }
    }
}

async function sendSerialCommand(command) {
    if (!serialWriter) return;
    try { await serialWriter.write(command + "\n"); } catch (e) {}
}

function sendSequenceToHardware() {
    let str = "SEQ ";
    for (let i = 0; i < NUM_STEPS; i++) str += sequence[i] + " ";
    sendSerialCommand(str.trim());
}

midiStatusEl.addEventListener('click', () => { if(!serialPort) initSerial(); });

window.addEventListener('DOMContentLoaded', () => {
    initUI();
});
