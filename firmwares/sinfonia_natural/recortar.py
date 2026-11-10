# ==============================================================================================================================================
# RECORTAR — elige el mejor tramo de una grabación larga para sonidos/ (sinfonia_natural) — GC Lab Chile
# ==============================================================================================================================================
# Uso:
#   python recortar.py grabacion.mp3 sonidos/vertiente.wav a 3.2              (a = ambiente: el tramo más parejo)
#   python recortar.py grabacion.mp3 sonidos/zorzal.wav e 3.2                 (e = evento: una frase que parte fuerte y termina en silencio)
#   python recortar.py grabacion.mp3 sonidos/buho.wav e 2.8 250 1000          (busca el evento sólo en esa banda: el búho vive en
#                                                                              250–1000 Hz y, sin esto, el siseo de la noche ganaba)
# Después: python preparar_muestras.py sonidos
# Necesita ffmpeg en el PATH y numpy.
# ==============================================================================================================================================
# Elige la mejor ventana de cada grabación y la deja como WAV mono 44.1 kHz.
#   ambiente: el tramo más parejo (RMS cerca de la mediana, poca variación, sin picos raros)
#   evento:   la frase con más energía que empieza en un ataque y termina en silencio
import subprocess, sys, os, numpy as np
SR = 44100
def leer(r):
    b = subprocess.run(["ffmpeg", "-v", "error", "-i", r, "-f", "s16le", "-ac", "1", "-ar", str(SR), "-"], check=True, capture_output=True).stdout
    return np.frombuffer(b, np.int16).astype(np.float32) / 32768
def env(x, ms=10):
    h = int(SR * ms / 1000); n = len(x) // h
    return np.sqrt(np.mean(x[:n * h].reshape(n, h) ** 2, axis=1) + 1e-12), h
def ambiente(x, L):
    e, h = env(x, 50); w = int(L * SR / h); med = np.median(e)
    mejor, ib = 1e9, 0
    for i in range(0, len(e) - w, 2):
        s = e[i:i + w]
        score = abs(np.log(s.mean() / med)) + np.std(np.log(s)) + 0.5 * np.log(s.max() / s.mean())
        if score < mejor: mejor, ib = score, i
    return x[ib * h: ib * h + int(L * SR)]
def banda(x, lo, hi):
    X = np.fft.rfft(x); f = np.fft.rfftfreq(len(x), 1 / SR); X[(f < lo) | (f > hi)] = 0
    return np.fft.irfft(X, len(x)).astype(np.float32)
def evento(x, L, lo=0, hi=0):
    xb = banda(x, lo, hi) if hi else x
    e, h = env(xb, 10); w = int(L * SR / h); cola = int(0.3 * SR / h)
    piso = np.percentile(e, 20); mejor, ib = -1e9, 0
    for i in range(3, len(e) - w):
        if e[i] < 3 * piso or e[i - 3:i].max() > 2 * piso: continue      # empezar justo en un ataque
        s = e[i:i + w]
        q = max(1, w // 4)
        score = np.log(np.mean(s ** 2) + 1e-12) + np.log(np.mean(s[:q] ** 2) + 1e-12) - 3.0 * np.log(np.mean(s[-cola:] ** 2) / np.mean(s ** 2) + 1e-3)
        if score > mejor: mejor, ib = score, i
    s = e[ib:ib + w]; k = int(np.argmax(s > s.max() * 10 ** (-10 / 20)))   # arranca donde la frase ya suena
    a = max(0, (ib + k) * h - int(0.03 * SR))
    return x[a: a + int(L * SR)]
def guardar(y, ruta):
    p = subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "f32le", "-ar", str(SR), "-ac", "1", "-i", "-", ruta], input=y.astype(np.float32).tobytes())
entrada, salida, tipo, L = sys.argv[1], sys.argv[2], sys.argv[3], float(sys.argv[4])
x = leer(entrada)
lo, hi = (float(sys.argv[5]), float(sys.argv[6])) if len(sys.argv) > 6 else (0, 0)
y = ambiente(x, L) if tipo == "a" else evento(x, L, lo, hi)
guardar(y, salida); print(f"{os.path.basename(salida)}: {len(y)/SR:.2f} s (de {len(x)/SR:.1f} s)")
