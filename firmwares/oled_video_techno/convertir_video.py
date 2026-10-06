#!/usr/bin/env python3
# ==============================================================================================================================================
# convertir_video.py — pasa un video VERTICAL a cuadros de 1 bit para la OLED SSD1306 girada (64 × 128)
# GC Lab Chile · parte de firmwares/oled_video_techno
# ==============================================================================================================================================
# La SSD1306 es de 128 × 64 apaisada. Girada de lado queda en 64 de ancho × 128 de alto, la misma proporción
# (1:2) que un short recortado un poco de los costados. Cada cuadro en 1 bit son 64·128/8 = 1024 bytes, y se
# guarda YA en el orden de memoria de la pantalla (8 páginas × 128 columnas, cada byte = 8 píxeles en vertical),
# así el firmware sólo copia bytes al I2C, sin girar nada en la placa.
#
# Uso:
#   python convertir_video.py short.mp4                       → video.h + vista_previa.png
#   python convertir_video.py short.mp4 --modo dither         → dithering de Bayer sobre la luminancia
#   python convertir_video.py short.mp4 --tapar 290,0,70,24   → tapa un rectángulo (x,y,ancho,alto en píxeles del
#                                                               video original) antes de convertir: logos, textos
#
# Modos:
#   silueta (por defecto) — para un personaje sobre FONDO CLARO liso: el fondo queda APAGADO (negro), la figura
#                           ENCENDIDA, y las líneas oscuras del dibujo (contornos, pelo, pliegues) se apagan dentro
#                           de la figura. Es lo que mejor se lee en 64 × 128: en una OLED el negro es el fondo.
#   dither                — escala de grises con matriz de Bayer 4×4 (estable entre cuadros: no "hierve")
#   umbral                — blanco y negro duro
#
# Requiere: ffmpeg en el PATH (o --ffmpeg), numpy, Pillow.
# ==============================================================================================================================================

import argparse
import os
import shutil
import subprocess
import sys

import numpy as np
from PIL import Image, ImageFilter

ANCHO, ALTO = 64, 128          # la pantalla girada (vertical)
BYTES_CUADRO = ANCHO * ALTO // 8

BAYER4 = (np.array([[0, 8, 2, 10],
                    [12, 4, 14, 6],
                    [3, 11, 1, 9],
                    [15, 7, 13, 5]], dtype=np.float32) + 0.5) / 16.0


def leer_cuadros(ffmpeg, ruta, fps, tapar):
    """Devuelve una lista de cuadros en escala de grises (ALTO × ANCHO, float 0..1)."""
    filtros = []
    if tapar:
        x, y, w, h = tapar
        filtros.append(f"drawbox=x={x}:y={y}:w={w}:h={h}:color=white:t=fill")
    if fps:
        filtros.append(f"fps={fps}")
    # recorta al centro a proporción 1:2 y escala a 2× (se reduce después con Lanczos en PIL)
    filtros.append("crop='min(iw,ih/2)':'min(ih,iw*2)'")
    filtros.append(f"scale={ANCHO * 2}:{ALTO * 2}:flags=lanczos")
    filtros.append("format=gray")
    cmd = [ffmpeg, "-v", "error", "-i", ruta, "-vf", ",".join(filtros), "-f", "rawvideo", "-"]
    crudo = subprocess.run(cmd, check=True, capture_output=True).stdout
    tam = ANCHO * 2 * ALTO * 2
    n = len(crudo) // tam
    cuadros = []
    for i in range(n):
        img = Image.frombytes("L", (ANCHO * 2, ALTO * 2), crudo[i * tam:(i + 1) * tam])
        cuadros.append(img)
    return cuadros


def a_bits(img2x, modo, umbral, fondo):
    """img2x: imagen L al doble de resolución. Devuelve array bool ALTO × ANCHO (True = píxel encendido)."""
    g = np.asarray(img2x.resize((ANCHO, ALTO), Image.LANCZOS), dtype=np.float32) / 255.0
    if modo == "umbral":
        return g > umbral
    if modo == "dither":
        return g > np.tile(BAYER4, (ALTO // 4, ANCHO // 4))
    # silueta: la máscara se calcula al doble de resolución (bordes más limpios) y se reduce
    g2 = np.asarray(img2x, dtype=np.float32) / 255.0
    figura2 = g2 < fondo
    m = Image.fromarray((figura2 * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(3)).filter(ImageFilter.MinFilter(3))
    figura = np.asarray(m.resize((ANCHO, ALTO), Image.BILINEAR), dtype=np.float32) / 255.0 > 0.5
    # dentro de la figura: encendido salvo las líneas oscuras (contorno, pelo, sombras duras)
    oscuro = g < umbral
    return figura & ~oscuro


def empaquetar(bits):
    """bits: ALTO(128) × ANCHO(64) en vertical → 1024 bytes en el orden de la SSD1306 apaisada.

    La pantalla física es 128 columnas × 64 filas. La imagen vertical se gira 90° a la izquierda:
    la fila y del video (0 = arriba) pasa a la columna física y, y la columna x del video pasa a la fila
    física 63 − x. Con la placa girada hacia el otro lado, el firmware da vuelta la pantalla por hardware
    (GIRO_180) sin tocar los datos.
    """
    fis = np.flipud(bits.T)                # fis[63 − x, y] = bits[y, x]  →  64 filas × 128 columnas
    out = bytearray(BYTES_CUADRO)
    for pag in range(8):
        bloque = fis[pag * 8:(pag + 1) * 8, :]          # 8 filas × 128 columnas
        pesos = (1 << np.arange(8)).reshape(8, 1)
        bytes_col = (bloque.astype(np.uint16) * pesos).sum(axis=0)
        out[pag * 128:(pag + 1) * 128] = bytes(bytes_col.astype(np.uint8))
    return out


def vista_previa(lista_bits, ruta, columnas=12, escala=3, maximo=48):
    paso = max(1, len(lista_bits) // maximo)
    sel = lista_bits[::paso][:maximo]
    filas = (len(sel) + columnas - 1) // columnas
    m = 4
    lienzo = Image.new("RGB", (columnas * (ANCHO * escala + m) + m, filas * (ALTO * escala + m) + m), (40, 40, 40))
    for i, b in enumerate(sel):
        img = Image.fromarray(np.where(b, 235, 0).astype(np.uint8)).resize((ANCHO * escala, ALTO * escala), Image.NEAREST)
        cx, cy = i % columnas, i // columnas
        lienzo.paste(img.convert("RGB"), (m + cx * (ANCHO * escala + m), m + cy * (ALTO * escala + m)))
    lienzo.save(ruta)


def escribir_h(ruta, cuadros, pulsos, fps_fuente, origen):
    n = len(cuadros)
    with open(ruta, "w", encoding="utf-8") as f:
        f.write("// Generado por convertir_video.py — no editar a mano (vuelve a correr el script).\n")
        f.write(f"// Origen: {os.path.basename(origen)} · {n} cuadros de 64×128 (1 bit) · {fps_fuente:g} fps en la fuente\n")
        f.write("// Orden de memoria: el de la SSD1306 apaisada (8 páginas × 128 columnas), girado para verse vertical.\n")
        f.write("#pragma once\n#include <stdint.h>\n\n")
        f.write(f"#define VIDEO_CUADROS     {n}\n")
        f.write(f"#define VIDEO_PULSOS      {pulsos}      // el loop del video dura esta cantidad de negras\n")
        f.write(f"#define VIDEO_FPS_FUENTE  {fps_fuente:g}f\n\n")
        f.write(f"const uint8_t VIDEO[VIDEO_CUADROS][{BYTES_CUADRO}] PROGMEM = {{\n")
        for c in cuadros:
            f.write("  {")
            for i in range(0, len(c), 32):
                f.write("\n    " + ",".join(f"0x{b:02X}" for b in c[i:i + 32]) + ",")
            f.write("\n  },\n")
        f.write("};\n")


def main():
    ap = argparse.ArgumentParser(description="Video vertical → cuadros 1 bit para la OLED SSD1306 girada (64×128)")
    ap.add_argument("video")
    ap.add_argument("--salida", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "video.h"))
    ap.add_argument("--vista", default=None, help="PNG de vista previa (por defecto junto a la salida)")
    ap.add_argument("--modo", choices=["silueta", "dither", "umbral"], default="silueta")
    ap.add_argument("--umbral", type=float, default=0.22, help="luminancia bajo la cual una línea se apaga (0..1)")
    ap.add_argument("--fondo", type=float, default=0.86, help="silueta: más claro que esto es fondo (0..1)")
    ap.add_argument("--fps", type=float, default=None, help="re-muestrear a estos fps (por defecto los del video)")
    ap.add_argument("--max-cuadros", type=int, default=1400, help="tope (cada cuadro = 1 KB de flash)")
    ap.add_argument("--tapar", default=None, help="x,y,ancho,alto a tapar con blanco en el video original")
    ap.add_argument("--bpm", type=float, default=126.0, help="tempo de referencia (el BPM_INICIAL del firmware)")
    ap.add_argument("--pulsos", type=int, default=None, help="forzar el largo del loop en negras")
    ap.add_argument("--ffmpeg", default=shutil.which("ffmpeg") or "ffmpeg")
    a = ap.parse_args()

    tapar = tuple(int(v) for v in a.tapar.split(",")) if a.tapar else None

    # fps de la fuente (para calcular cuántas negras dura el loop)
    ffprobe = os.path.join(os.path.dirname(a.ffmpeg), "ffprobe" + (".exe" if os.name == "nt" else ""))
    fps_fuente = a.fps
    if not fps_fuente:
        r = subprocess.run([ffprobe, "-v", "error", "-select_streams", "v:0", "-show_entries", "stream=r_frame_rate",
                            "-of", "default=nw=1:nk=1", a.video], capture_output=True, text=True).stdout.strip()
        num, _, den = r.partition("/")
        fps_fuente = float(num) / float(den or 1)

    imgs = leer_cuadros(a.ffmpeg, a.video, a.fps, tapar)
    if len(imgs) > a.max_cuadros:
        print(f"aviso: {len(imgs)} cuadros, se recortan a {a.max_cuadros}", file=sys.stderr)
        imgs = imgs[:a.max_cuadros]
    bits = [a_bits(im, a.modo, a.umbral, a.fondo) for im in imgs]
    cuadros = [empaquetar(b) for b in bits]

    dur = len(cuadros) / fps_fuente
    if a.pulsos:
        pulsos = a.pulsos
    else:  # múltiplo de 4 negras (compases enteros) más cercano a la duración real al tempo de referencia
        pulsos = max(4, int(round(dur * a.bpm / 60.0 / 4.0)) * 4)

    escribir_h(a.salida, cuadros, pulsos, fps_fuente, a.video)
    vista = a.vista or os.path.splitext(a.salida)[0] + "_vista_previa.png"
    vista_previa(bits, vista)
    vel = (pulsos * 60.0 / a.bpm) and dur / (pulsos * 60.0 / a.bpm)
    print(f"{len(cuadros)} cuadros · {len(cuadros) * BYTES_CUADRO / 1024:.0f} KB · {dur:.2f} s a {fps_fuente:g} fps")
    print(f"loop = {pulsos} negras ({pulsos // 4} compases): a {a.bpm:g} BPM el video corre a {vel:.2f}× su velocidad")
    print(f"→ {a.salida}\n→ {vista}")


if __name__ == "__main__":
    main()
