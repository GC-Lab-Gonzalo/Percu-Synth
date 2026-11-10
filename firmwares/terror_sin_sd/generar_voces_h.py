# ==============================================================================================================================================
# GENERAR_VOCES_H — mete las voces de terror_sin_sd dentro del firmware (en la flash del ESP32) — GC Lab Chile
# ==============================================================================================================================================
# Uso:
#   python generar_voces_h.py --risas a.wav b.wav --lamentos c.wav d.wav --susurros e.wav f.wav --ninos g.wav h.wav --bebes i.wav j.wav
#
# Escribe voces.h en esta carpeta: cada voz como un arreglo const de int16 (va a la flash, no a la RAM), mono a
# 44.1 kHz, sin silencio en los bordes, recortada a MAX_S en su punto más quieto, con el pico a −1 dBFS y fundidos.
# Acepta cualquier audio que lea ffmpeg (necesita ffmpeg en el PATH y numpy).
#
# La flash manda: con la partición «Huge APP (3MB No OTA)» quedan ~2.4 MB para voces, unos 27 s en total.
# El script avisa si se pasa.
# ==============================================================================================================================================
import argparse, os, subprocess
import numpy as np

SR = 44100
MAX_S = 3.0            # cada voz dura a lo más esto
UMBRAL_DB = -45.0      # bajo esto (respecto del pico) es silencio en los bordes
PRESUPUESTO_MB = 2.4   # lo que cabe junto al programa en la partición Huge APP
CATEGORIAS = ("risas", "lamentos", "susurros", "ninos", "bebes")


def decodificar(ruta):
    crudo = subprocess.run(["ffmpeg", "-v", "error", "-i", ruta, "-f", "s16le", "-ac", "1", "-ar", str(SR), "-"],
                           check=True, capture_output=True).stdout
    return np.frombuffer(crudo, dtype=np.int16).astype(np.float32) / 32768.0


def preparar(x):
    sobre = np.nonzero(np.abs(x) > np.abs(x).max() * 10 ** (UMBRAL_DB / 20))[0]
    x = x[max(0, sobre[0] - 200):sobre[-1] + 1500]
    if len(x) > MAX_S * SR:                          # larga: se corta en el punto más quieto de su último tramo
        a, b = int((MAX_S - 0.8) * SR), int(MAX_S * SR)
        e = np.convolve(np.abs(x[a:b]), np.ones(441) / 441, mode="same")
        x = x[:a + int(np.argmin(e))]
    x = x * (10 ** (-1 / 20) / max(np.abs(x).max(), 1e-9))
    f, g = int(0.003 * SR), int(0.060 * SR)
    x[:f] *= np.linspace(0, 1, f); x[-g:] *= np.linspace(1, 0, g) ** 2
    return np.clip(np.round(x * 32767), -32768, 32767).astype(np.int16)


def main():
    ap = argparse.ArgumentParser(description="Genera voces.h para terror_sin_sd")
    for c in CATEGORIAS:
        ap.add_argument("--" + c, nargs="*", default=[], help=f"audios de {c}")
    a = ap.parse_args()
    lineas = ["// Generado por generar_voces_h.py — no se edita a mano. Mono, 16 bit, 44.1 kHz, en la flash.", "#pragma once", ""]
    tabla, total = [], 0
    for c in CATEGORIAS:
        if len(getattr(a, c)) > 4: raise SystemExit(f"{c}: a lo más 4 voces por categoría")
        nombres = []
        for i, ruta in enumerate(getattr(a, c)):
            x = preparar(decodificar(ruta))
            n = f"VOZ_{c.upper()}_{i + 1}"
            nombres.append((n, len(x)))
            total += len(x)
            cuerpo = ",".join(str(int(v)) for v in x)
            lineas.append(f"// {os.path.basename(ruta)} · {len(x) / SR:.2f} s")
            lineas.append(f"const int16_t {n}[{len(x)}] = {{{cuerpo}}};")
            print(f"{c}/{i + 1}: {len(x) / SR:.2f} s  <- {os.path.basename(ruta)}")
        tabla.append(nombres)
    lineas.append("")
    lineas.append("#define VOCES_FLASH_MAX 4")
    lineas.append("const int16_t *const VOZ_DATOS[5][VOCES_FLASH_MAX] = {")
    for nombres in tabla:
        lineas.append("  { " + ", ".join([n for n, _ in nombres] + ["nullptr"] * (4 - len(nombres))) + " },")
    lineas.append("};")
    lineas.append("const uint32_t VOZ_LARGO[5][VOCES_FLASH_MAX] = {")
    for nombres in tabla:
        lineas.append("  { " + ", ".join([str(l) for _, l in nombres] + ["0"] * (4 - len(nombres))) + " },")
    lineas.append("};")
    lineas.append("const int VOZ_CUANTAS[5] = { " + ", ".join(str(len(n)) for n in tabla) + " };")
    destino = os.path.join(os.path.dirname(os.path.abspath(__file__)), "voces.h")
    open(destino, "w", encoding="utf-8").write("\n".join(lineas) + "\n")
    mb = total * 2 / 1e6
    print(f"voces.h: {total / SR:.1f} s · {mb:.2f} MB de flash" + ("" if mb <= PRESUPUESTO_MB else f"  ¡OJO: se pasa de {PRESUPUESTO_MB} MB, no va a caber!"))


if __name__ == "__main__":
    main()
