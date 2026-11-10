# ==============================================================================================================================================
# PREPARAR_MUESTRAS — mete los sonidos grabados DENTRO de sinfonia_natural.ino (en la flash del ESP32) — GC Lab Chile
# ==============================================================================================================================================
# Uso:
#   python preparar_muestras.py sonidos                        (todos los audios de una carpeta)
#   python preparar_muestras.py buho.mp3 zorzal.wav ...        (archivos sueltos)
#   python preparar_muestras.py sonidos --max-seg 3            (cada animal dura a lo más 3 s)
#   python preparar_muestras.py --borrar                       (saca los sonidos: BTN5 = gotas afinadas, POT4 no suena)
#
# Los sonidos quedan ESCRITOS EN EL MISMO .ino, así el firmware es un solo archivo que se puede pasar a cualquiera:
#   - arriba, entre «>>> SONIDOS: ÍNDICE» y «<<< SONIDOS: ÍNDICE», cuántos hay y cómo se llaman;
#   - al final del archivo, entre «>>> SONIDOS: DATOS» y «<<< SONIDOS: DATOS», los datos.
# Van comprimidos en IMA ADPCM de 4 bits (4 veces menos que 16 bit), en bloques de 1024 muestras con su propia cabecera:
# así el firmware puede empezar a leer en cualquier bloque (la vertiente continua lo necesita). Medido sobre estos
# sonidos: 20–43 dB de señal/ruido, con el ruido siguiendo a la señal. Todo cabe en la partición por defecto.
#
# Sirve cualquier audio que lea ffmpeg (mp3 de Freesound, wav de ElevenLabs, ogg de Wikimedia…). Necesita ffmpeg en el
# PATH y numpy. Cada sonido queda mono a 44.1 kHz, sin silencio en los bordes, recortado en su punto más quieto, con el
# nivel parejo entre sonidos (RMS a −20 dBFS sin pasar de −1 dBFS de pico) y fundidos. El nombre de cada sonido es el de
# su archivo: el Monitor Serial lo dice cada vez que BTN5 lo dispara.
#
# Un archivo cuyo nombre empieza con «continua_» NO va al BTN5: es la cama continua que suena con el POT4 (la vertiente).
# Va entera (hasta 8 s), sin recortar silencios ni fundidos, porque el firmware la empalma consigo misma sin parar.
#
# La flash manda: 44.1 kHz en ADPCM = 22 KB por segundo. Con la partición por defecto caben ~38 s en total.
# ==============================================================================================================================================
import argparse, os, re, subprocess
import numpy as np

SR = 44100
UMBRAL_DB = -45.0          # bajo esto (respecto del pico) es silencio en los bordes
RMS_DB = -20.0             # nivel parejo entre sonidos: ninguno pasa de este RMS (ni de −1 dBFS de pico)
CABE_MB = 0.82             # lo que cabe junto al programa (~0.45 MB) en la partición por defecto (1.31 MB)
BLOQUE = 1024              # muestras por bloque ADPCM (igual que ADPCM_BLOQUE en el .ino)
EXTENSIONES = (".wav", ".mp3", ".ogg", ".flac", ".m4a", ".aif", ".aiff", ".opus", ".webm")
AQUI = os.path.dirname(os.path.abspath(__file__))
INO = os.path.join(AQUI, "sinfonia_natural.ino")
MARCAS = {"indice": ("// >>> SONIDOS: ÍNDICE", "// <<< SONIDOS: ÍNDICE"),
          "datos":  ("// >>> SONIDOS: DATOS",  "// <<< SONIDOS: DATOS")}

# IMA ADPCM (el decodificador del .ino usa exactamente estas tablas)
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


def a_int16(x):
    return np.clip(np.round(x * 32767), -32768, 32767).astype(np.int16)


def nivelar(x):
    # el que llegue primero de los dos techos — pico a −1 dBFS o RMS a RMS_DB. Normalizar sólo por pico deja una
    # lluvia (ruido parejo) mucho más fuerte que un búho (dos golpes y silencio).
    g_pico = 10 ** (-1 / 20) / max(np.abs(x).max(), 1e-9)
    g_rms = 10 ** (RMS_DB / 20) / max(np.sqrt(np.mean(x ** 2)), 1e-9)
    return x * min(g_pico, g_rms)


def preparar(x, max_s):
    if len(x) == 0 or np.abs(x).max() < 1e-6:
        return None
    x = x - x.mean()
    sobre = np.nonzero(np.abs(x) > np.abs(x).max() * 10 ** (UMBRAL_DB / 20))[0]
    x = x[max(0, sobre[0] - 220):sobre[-1] + 2200]
    if len(x) > max_s * SR:                          # largo: se corta en el punto más quieto de su último tramo
        a, b = int(max(0.5, max_s - 0.8) * SR), int(max_s * SR)
        e = np.convolve(np.abs(x[a:b]), np.ones(441) / 441, mode="same")
        x = x[:a + int(np.argmin(e))]
    x = nivelar(x)
    f, g = int(0.005 * SR), min(int(0.120 * SR), len(x) // 3)
    x[:f] *= np.linspace(0, 1, f)
    x[-g:] *= np.linspace(1, 0, g) ** 2              # el firmware no hace fundido: el final llega en silencio
    return a_int16(x)


def preparar_continua(x):
    x = x - x.mean()
    return a_int16(nivelar(x[:int(8.0 * SR)]))


def adpcm(x):
    """IMA ADPCM en bloques de BLOQUE muestras: cabecera de 3 bytes (predictor int16 LE + índice) + BLOQUE/2 bytes."""
    n = (len(x) + BLOQUE - 1) // BLOQUE * BLOQUE
    x = np.concatenate([x.astype(np.int32), np.zeros(n - len(x), np.int32)])
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


def arreglo(nombre, datos):
    # 32 valores por línea: el generador de prototipos del IDE (ctags) falla con líneas muy largas («line too long»)
    filas = [",".join(str(v) for v in datos[i:i + 32]) for i in range(0, len(datos), 32)]
    return f"const uint8_t {nombre}[{len(datos)}] = {{\n" + ",\n".join(filas) + "\n};"


def nombre_c(ruta, i):
    base = os.path.splitext(os.path.basename(ruta))[0]
    limpio = re.sub(r"[^A-Za-z0-9]+", "_", base).strip("_").upper() or "SONIDO"
    return f"MUESTRA_{i + 1}_{limpio[:24]}", base.replace('"', "'")[:40]


def escribir_en_ino(indice, datos):
    s = open(INO, encoding="utf-8").read()
    for clave, cuerpo in (("indice", indice), ("datos", datos)):
        a, b = MARCAS[clave]
        i, j = s.find(a), s.find(b)
        if i < 0 or j < 0:
            raise SystemExit(f"No encuentro las marcas «{a}» / «{b}» en {os.path.basename(INO)}")
        s = s[:s.index("\n", i) + 1] + cuerpo + ("\n" if cuerpo else "") + s[j:]
    open(INO, "w", encoding="utf-8", newline="\n").write(s)


def main():
    ap = argparse.ArgumentParser(description="Mete los sonidos grabados dentro de sinfonia_natural.ino")
    ap.add_argument("entradas", nargs="*", help="archivos de audio o carpetas")
    ap.add_argument("--max-seg", type=float, default=4.0, help="duración máxima de cada animal (s)")
    ap.add_argument("--borrar", action="store_true", help="saca los sonidos del .ino")
    a = ap.parse_args()
    if a.borrar:
        escribir_en_ino("#define NUM_MUESTRAS 0", "")
        print("Sonidos sacados del .ino: BTN5 hace gotas afinadas y POT4 no suena"); return
    rutas = []
    for e in a.entradas:
        if os.path.isdir(e):
            rutas += sorted(os.path.join(e, f) for f in os.listdir(e) if f.lower().endswith(EXTENSIONES))
        else:
            rutas.append(e)
    if not rutas:
        raise SystemExit("No hay audios. Ejemplo: python preparar_muestras.py sonidos")
    if len(rutas) > 24:
        raise SystemExit("A lo más 24 sonidos")
    datos, tabla, total, continua = [], [], 0, None
    for ruta in rutas:
        base = os.path.basename(ruta)
        if base.lower().startswith("continua_"):
            if continua is not None:
                raise SystemExit("Sólo puede haber un archivo continua_")
            x = preparar_continua(decodificar(ruta)); b = adpcm(x); continua = len(x); total += len(b)
            datos += [f"// {base} · {len(x) / SR:.2f} s · cama continua del POT4", arreglo("CONTINUA_DATOS", b)]
            print(f"    {len(x) / SR:5.2f} s  {base} (continua, POT4)")
            continue
        x = preparar(decodificar(ruta), a.max_seg)
        if x is None:
            print(f"  (salto {base}: está en silencio)"); continue
        n, legible = nombre_c(ruta, len(tabla))
        b = adpcm(x); tabla.append((n, len(x), legible)); total += len(b)
        datos += [f"// {base} · {len(x) / SR:.2f} s", arreglo(n, b)]
        print(f"{len(tabla):2d}. {len(x) / SR:5.2f} s  {legible}")
    if not tabla and continua is None:
        raise SystemExit("Ningún audio sirvió")
    indice = [f"#define NUM_MUESTRAS {len(tabla)}"]
    if tabla:
        indice += ["extern const uint8_t *const MUESTRA_DATOS[NUM_MUESTRAS];",
                   "extern const uint32_t MUESTRA_LARGO[NUM_MUESTRAS];",
                   "extern const char *const MUESTRA_NOMBRE[NUM_MUESTRAS];"]
        datos += ["const uint8_t *const MUESTRA_DATOS[NUM_MUESTRAS] = { " + ", ".join(n for n, _, _ in tabla) + " };",
                  "const uint32_t MUESTRA_LARGO[NUM_MUESTRAS] = { " + ", ".join(str(l) for _, l, _ in tabla) + " };",
                  "const char *const MUESTRA_NOMBRE[NUM_MUESTRAS] = { " + ", ".join(f'"{s}"' for _, _, s in tabla) + " };"]
    if continua is not None:
        indice += [f"#define CONTINUA_LARGO {continua}", "extern const uint8_t CONTINUA_DATOS[];"]
    escribir_en_ino("\n".join(indice), "\n".join(datos))
    mb = total / 1e6
    print(f"\nEscritos en {os.path.basename(INO)}: {len(tabla)} sonidos" + (" + vertiente continua" if continua else "")
          + f" · {mb:.2f} MB de flash")
    print("Cabe con la partición por defecto." if mb <= CABE_MB else
          f"¡OJO! Pasa de {CABE_MB} MB: usa «Huge APP (3MB No OTA/1MB SPIFFS)» o menos sonidos.")


if __name__ == "__main__":
    main()
