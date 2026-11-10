# ==============================================================================================================================================
# PREPARAR_SAMPLES — mete los samples DENTRO del firmware matriz_oscura (GC Lab Chile): sin microSD
# ==============================================================================================================================================
# Uso:
#   python preparar_samples.py                 → lee banco/<carpeta>/*.wav|mp3|ogg… y los escribe DENTRO de matriz_oscura.ino
#   python preparar_samples.py --origen OTRA   → otra carpeta con las mismas subcarpetas
#
# Los samples son el NOTICIERO DEL FUTURO (generar_noticias.py los hace con ElevenLabs y les hornea el glitch):
# un banco por carpeta, titulares · presentador · senal (BTN1 · BTN2 · BTN3 del panel Samples). De cada carpeta
# entran hasta 8, en orden (para elegir el orden, numéralos: 1.wav, 2.wav…). Las carpetas impactos · metales ·
# fx ya NO entran como samples: sólo se recortan para la percusión extra de Batería. Cada sample queda:
#   · mono, 44.1 kHz, sin el silencio del principio ni del final (si no, al revés tarda en arrancar)
#   · sin continua, nivelado por RMS a −16 dBFS medido sólo donde suena, con el pico tope en −1 dBFS (con
#     sólo el pico, un retumbo largo sonaba mucho más fuerte que un golpe seco — lección de sinfonia_natural)
#   · cortado al largo máximo de su categoría con un fundido de 150 ms, y fundidos de 3 ms en los bordes
#   · comprimido en IMA ADPCM de 4 bit en bloques de 1024 muestras con su cabecera (la que lee el firmware;
#     se puede empezar a leer en cualquier bloque, que es lo que necesita la reversa). Si el ADPCM le deja
#     menos de SNR_MIN dB (pasa con ondas cuadradas y ruidos: sirenas, glitches) va en μ-law de 8 bit, que
#     ocupa el doble pero da ~38 dB: claridad antes que espacio
# Todo queda en UN SOLO ARCHIVO, matriz_oscura.ino (se comparte ese archivo y nada más): un índice cerca del
# principio, entre «// >>> INDICE SAMPLES» y «// <<< INDICE SAMPLES», y los datos al final, entre
# «// >>> SAMPLES» y «// <<< SAMPLES». Este script reescribe sólo lo que hay entre esas marcas. Con las
# 24 frases son ~1.6 MB: hace falta la partición «Huge APP (3MB No OTA/1MB SPIFFS)».
# Necesita ffmpeg en el PATH y numpy.
# ==============================================================================================================================================
import argparse, os, re, subprocess
import numpy as np

SR = 44100
# Los 3 BANCOS del panel Samples (BTN1 · BTN2 · BTN3): nombre y carpeta
BANCOS = (("TITULARES", "titulares"), ("PRESENTADOR", "presentador"), ("SENAL", "senal"))
# RECORTES: la percusión extra del BTN1 de Batería. Son trozos cortos de los samples (desde el golpe más
# fuerte de cada uno), en tres largos: cortos, medios y largos. De cada fuente sale uno; se toman en este
# orden hasta completar 4 por grupo. Nada de clics sueltos: el más corto dura 40 ms (las ráfagas de
# clics de ruido sonaban a clipeo en industrial_matriz).
RECORTE_MS = ((40, 50, 60, 45), (80, 100, 120, 90), (160, 200, 240, 260))
RECORTE_DE = ("metales", "fx", "impactos")
MAX_POR_CAT = 8
MAX_S = {"titulares": 5.0, "presentador": 5.0, "senal": 5.0, "impactos": 3.0, "metales": 2.5, "fx": 2.5}
UMBRAL_DB, MARGEN_MS, FUNDIDO_MS, FIN_MS = -55.0, 8, 3, 150
PICO_DB, RMS_DB = -1.0, -16.0
BLOQUE = 1024
SNR_MIN = 20.0
AQUI = os.path.dirname(os.path.abspath(__file__))
SALIDA = os.path.join(AQUI, "matriz_oscura.ino")
AUDIO = (".wav", ".mp3", ".ogg", ".oga", ".flac", ".m4a", ".aif", ".aiff")

PASO = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107,
        118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876,
        963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
        5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623,
        27086, 29794, 32767]
INDICE = [-1, -1, -1, -1, 2, 4, 6, 8]


def decodificar(ruta):
    crudo = subprocess.run(["ffmpeg", "-v", "error", "-i", ruta, "-f", "s16le", "-ac", "1", "-ar", str(SR), "-"],
                           check=True, capture_output=True).stdout
    return np.frombuffer(crudo, dtype=np.int16).astype(np.float32) / 32768.0


def preparar(x, max_s):
    sobre = np.nonzero(np.abs(x) > 10 ** (UMBRAL_DB / 20))[0]
    if not len(sobre):
        return None
    m = MARGEN_MS * SR // 1000
    x = x[max(0, sobre[0] - m):min(len(x), sobre[-1] + m)].copy()
    x -= x.mean()                                      # sin continua: con offset, el borde es un escalón
    if len(x) > max_s * SR:
        x = x[:int(max_s * SR)]
        f = FIN_MS * SR // 1000; x[-f:] *= np.linspace(1, 0, f)
    v = int(0.020 * SR)
    env = np.sqrt(np.convolve(x ** 2, np.ones(v) / v, mode="same"))
    rms = np.sqrt(np.mean(env[env > env.max() * 0.1] ** 2))
    x *= min(10 ** (PICO_DB / 20) / max(np.abs(x).max(), 1e-9), 10 ** (RMS_DB / 20) / max(rms, 1e-9))
    f = FUNDIDO_MS * SR // 1000
    x[:f] *= np.linspace(0, 1, f); x[-f:] *= np.linspace(1, 0, f)
    return np.clip(np.round(x * 32767.0), -32768, 32767).astype(np.int32)


def adpcm(x):
    """IMA ADPCM en bloques de BLOQUE muestras: cabecera de 3 bytes (predictor int16 LE + índice) + BLOQUE/2 bytes."""
    n = (len(x) + BLOQUE - 1) // BLOQUE * BLOQUE
    x = np.concatenate([x, np.zeros(n - len(x), np.int32)])
    out, pred, idx = bytearray(), 0, 0
    for b in range(0, n, BLOQUE):
        out += int(pred & 0xFFFF).to_bytes(2, "little") + bytes([idx])
        nib = []
        for s in x[b:b + BLOQUE]:
            paso = PASO[idx]; d = int(s) - pred; c = 0
            if d < 0: c = 8; d = -d
            vp = paso >> 3
            if d >= paso: c |= 4; d -= paso; vp += paso
            if d >= paso >> 1: c |= 2; d -= paso >> 1; vp += paso >> 1
            if d >= paso >> 2: c |= 1; vp += paso >> 2
            pred = max(-32768, min(32767, pred - vp if c & 8 else pred + vp))
            idx = max(0, min(88, idx + INDICE[c & 7]))
            nib.append(c)
        out += bytes(nib[i] | (nib[i + 1] << 4) for i in range(0, BLOQUE, 2))
    return out


def mulaw(x):
    """G.711 μ-law de 8 bit (el firmware lo decodifica con una tabla de 256)."""
    out = bytearray()
    for s in x:
        s = int(s); signo = 0x80 if s < 0 else 0
        m = min(abs(s), 32635) + 132
        e = 7
        while e > 0 and not (m & (0x4000 >> (7 - e))): e -= 1
        out.append(~(signo | (e << 4) | ((m >> (e + 3)) & 0x0F)) & 0xFF)
    return out


def demulaw(b):
    b = ~b & 0xFF; signo = b & 0x80; e = (b >> 4) & 7; m = b & 0x0F
    v = ((m << 3) + 132) << e; v -= 132
    return -v if signo else v


def snr_mulaw(x, datos):
    y = np.array([demulaw(b) for b in datos], dtype=np.float64)
    return 10 * np.log10(np.sum(x.astype(np.float64) ** 2) / max(np.sum((x - y) ** 2), 1.0))


def snr(x, datos):
    """Relación señal/ruido del codec (decodificando igual que el firmware)."""
    y, k = [], 0
    while k < len(datos):
        pred = int.from_bytes(datos[k:k + 2], "little", signed=True); idx = datos[k + 2]; k += 3
        for j in range(BLOQUE):
            c = (datos[k + j // 2] >> 4) if j & 1 else (datos[k + j // 2] & 15)
            paso = PASO[idx]; dif = paso >> 3
            if c & 4: dif += paso
            if c & 2: dif += paso >> 1
            if c & 1: dif += paso >> 2
            pred = max(-32768, min(32767, pred - dif if c & 8 else pred + dif))
            idx = max(0, min(88, idx + INDICE[c & 7])); y.append(pred)
        k += BLOQUE // 2
    y = np.array(y[:len(x)], dtype=np.float64)
    return 10 * np.log10(np.sum(x.astype(np.float64) ** 2) / max(np.sum((x - y) ** 2), 1.0))


def main():
    ap = argparse.ArgumentParser(description="Mete los samples dentro de matriz_oscura.ino")
    ap.add_argument("--origen", default=os.path.join(AQUI, "banco"), help="carpeta con titulares/ presentador/ senal/ (y impactos/ metales/ fx/ para los recortes)")
    a = ap.parse_args()
    lineas, nombres, largos, formatos, inicio, total = [], [], [], [], [0], 0
    fuentes = []                                    # (categoría, archivo, x) para los recortes
    def archivos(cat):
        carpeta = os.path.join(a.origen, cat)
        return sorted((f for f in os.listdir(carpeta) if f.lower().endswith(AUDIO)) if os.path.isdir(carpeta) else [],
                      key=lambda f: (int(re.match(r"\d+", f)[0]) if re.match(r"\d+", f) else 999, f))[:MAX_POR_CAT]
    for cat in RECORTE_DE:                          # fuentes de los recortes: no van como samples
        for f in archivos(cat):
            x = preparar(decodificar(os.path.join(a.origen, cat, f)), MAX_S[cat])
            if x is not None: fuentes.append((cat, f, x))
    for _, cat in BANCOS:
        for f in archivos(cat):
            x = preparar(decodificar(os.path.join(a.origen, cat, f)), MAX_S[cat])
            if x is None:
                print(f"vacío: {cat}/{f}"); continue
            datos = adpcm(x); fmt = 0; q = snr(x, datos)
            if q < SNR_MIN:
                datos = mulaw(x); fmt = 1; q = snr_mulaw(x, datos)
            k = len(nombres)
            nombre = f"SMP_{k:02d}_{cat.upper()}"
            filas = [",".join(str(v) for v in datos[i:i + 32]) for i in range(0, len(datos), 32)]
            lineas.append(f"// {cat}/{f} · {len(x) / SR:.2f} s\nconst uint8_t {nombre}[{len(datos)}] = {{\n" + ",\n".join(filas) + "\n};")
            nombres.append(nombre); largos.append(len(x)); formatos.append(fmt); total += len(datos)
            print(f"{cat}/{f:10s} {len(x) / SR:4.2f} s  {len(datos) / 1e3:5.1f} KB  {'μ-law' if fmt else 'ADPCM'} {q:4.1f} dB")
        inicio.append(len(nombres))                 # fin de cada banco
    # ── recortes para la percusión extra ──
    por = [[t for t in fuentes if t[0] == c] for c in RECORTE_DE]  # se alternan: metal, fx, impacto, metal…
    fuentes = [l[i] for i in range(max(len(l) for l in por)) for l in por if i < len(l)]
    rec, recLar, k = [], [], 0
    for g, largos_ms in enumerate(RECORTE_MS):
        for ms in largos_ms:
            cat, f, x = fuentes[k % len(fuentes)]; k += 1
            v = int(0.003 * SR); env = np.sqrt(np.convolve((x / 32768.0) ** 2, np.ones(v) / v, mode="same"))
            on = max(0, int(np.argmax(env)) - int(0.004 * SR))   # 4 ms antes del golpe más fuerte
            n = int(ms * SR / 1000); y = x[on:on + n].astype(np.float64)
            if len(y) < n: y = np.concatenate([y, np.zeros(n - len(y))])
            t = np.arange(n)
            y *= np.minimum(1.0, t / (0.001 * SR)) * np.exp(-3.0 * t / n) * np.minimum(1.0, (n - t) / (0.004 * SR))
            y *= 10 ** (-3 / 20) * 32767 / max(np.abs(y).max(), 1.0)
            datos = mulaw(np.round(y).astype(np.int32))
            nombre = f"REC_{len(rec):02d}"
            filas = [",".join(str(b) for b in datos[i:i + 32]) for i in range(0, len(datos), 32)]
            lineas.append(f"// recorte {len(rec)} · {ms} ms de {cat}/{f}" + chr(10) + f"const uint8_t {nombre}[{len(datos)}] = {{" + chr(10) + ("," + chr(10)).join(filas) + chr(10) + "};")
            rec.append(nombre); recLar.append(n); total += len(datos)
            print(f"recorte {len(rec) - 1:2d}  {ms:3d} ms  de {cat}/{f}")
    indice = ["// >>> INDICE SAMPLES — lo escribe preparar_samples.py; no se edita a mano. Los datos van al final del archivo.",
              f"#define SMP_TOTAL {len(nombres)}", f"#define NUM_RECORTES {len(rec)}",
              "const uint8_t SMP_INICIO[4] = { " + ", ".join(str(v) for v in inicio) + " };   // dónde empieza cada banco: "
              + " · ".join(b[0] for b in BANCOS),
              "const uint32_t SMP_LARGO[SMP_TOTAL] = { " + ", ".join(str(v) for v in largos) + " };   // en muestras",
              "const uint8_t SMP_FORMATO[SMP_TOTAL] = { " + ", ".join(str(v) for v in formatos) + " };   // 0 IMA ADPCM · 1 μ-law",
              "const uint16_t REC_LARGO[NUM_RECORTES] = { " + ", ".join(str(v) for v in recLar) + " };   // recortes: 0–3 cortos · 4–7 medios · 8–11 largos",
              "extern const uint8_t *const SMP_DATOS[SMP_TOTAL];",
              "extern const uint8_t *const REC_DATOS[NUM_RECORTES];",
              "// <<< INDICE SAMPLES"]
    datos = ["// >>> SAMPLES",
             "// ==============================================================================================================================================",
             "// SAMPLES — 44.1 kHz mono, en IMA ADPCM 4 bit (bloques de 1024) o μ-law 8 bit. Los escribe preparar_samples.py desde banco/;",
             "// no se editan a mano. Noticias: ElevenLabs (generar_noticias.py). Recortes: CC0 de Freesound (fuentes_samples.txt).",
             "// =============================================================================================================================================="] + lineas + [
             "const uint8_t *const SMP_DATOS[SMP_TOTAL] = { " + ", ".join(nombres) + " };",
             "// recortes de la percusión extra, μ-law, ya con su envolvente",
             "const uint8_t *const REC_DATOS[NUM_RECORTES] = { " + ", ".join(rec) + " };",
             "// <<< SAMPLES"]
    ino = open(SALIDA, encoding="utf-8").read()
    def poner(texto, ini, fin, bloque, si_no):
        if ini in texto:                                  # reemplaza lo que hay entre las marcas
            a = texto.index(ini); b = texto.index(fin, a) + len(fin)
            return texto[:a] + bloque + texto[b:]
        return si_no(texto, bloque)
    ino = poner(ino, "// >>> INDICE SAMPLES", "// <<< INDICE SAMPLES", "\n".join(indice),
                lambda t, bl: t.replace('#include "samples.h"', bl, 1))
    ino = poner(ino, "// >>> SAMPLES\n", "// <<< SAMPLES", "\n".join(datos),
                lambda t, bl: t.rstrip("\n") + "\n\n" + bl)
    if not ino.endswith("\n"): ino += "\n"
    open(SALIDA, "w", encoding="utf-8", newline="\n").write(ino)
    print(f"\n{len(nombres)} samples · {sum(largos) / SR:.1f} s · {total / 1e6:.2f} MB de flash → dentro de {os.path.basename(SALIDA)}")


if __name__ == "__main__":
    main()
