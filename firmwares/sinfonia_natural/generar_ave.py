# ==============================================================================================================================================
# GENERAR_AVE — dibuja el ave de la pantalla OLED y la escribe DENTRO de sinfonia_natural.ino — GC Lab Chile
# ==============================================================================================================================================
# Uso:  python generar_ave.py
#
# Un zorzal posado en una rama, en 128×64 a 1 bit, para la pantalla SSD1306 del conector OLED. Cuadros:
#   0 REPOSO · 1 PARPADEO (ojo cerrado; también es el ave dormida) · 2 CANTA (pico abierto, cabeza arriba)
# La silueta se dibuja a 4× y se umbraliza (bordes suaves); los detalles oscuros (ojo, ala, plumas, motas del pecho)
# se dibujan después a 1×, píxel por píxel, para que queden de 1 px exacto.
# Escribe los cuadros en el .ino entre «// >>> AVE» y «// <<< AVE», en el orden de memoria de la SSD1306
# (8 páginas × 128 columnas, bit 0 = fila de arriba de la página), y deja ave_vista_previa.png para revisarlos.
# ==============================================================================================================================================
import math, os
import numpy as np
from PIL import Image, ImageDraw

AQUI = os.path.dirname(os.path.abspath(__file__))
INO = os.path.join(AQUI, "sinfonia_natural.ino")
MARCAS = ("// >>> AVE", "// <<< AVE")
W, H, S = 128, 64, 4


def elipse(cx, cy, rx, ry, ang=0.0, n=72):
    a = math.radians(ang)
    return [(cx + rx * math.cos(t) * math.cos(a) - ry * math.sin(t) * math.sin(a),
             cy + rx * math.cos(t) * math.sin(a) + ry * math.sin(t) * math.cos(a))
            for t in (2 * math.pi * i / n for i in range(n))]


def esc(pts):
    return [(x * S, y * S) for x, y in pts]


def cuadro(canta=False, ojo_cerrado=False):
    g = Image.new("L", (W * S, H * S), 0)
    d = ImageDraw.Draw(g)
    dy = -1.5 if canta else 0.0                       # al cantar estira el cuello
    hx, hy = 50.0, 21.0 + dy                           # centro de la cabeza
    # --- rama (de la derecha hacia la izquierda, un poco curva) con una ramita y hojas ---
    rama = [(128, 49.0), (110, 50.0), (90, 51.5), (70, 52.5), (52, 53.0), (36, 54.5), (22, 57.0)]
    d.line(esc(rama), fill=255, width=int(3.2 * S), joint="curve")
    d.line(esc([(36, 54.5), (27, 50.0), (20, 49.0)]), fill=255, width=int(1.8 * S), joint="curve")
    for (cx, cy, rx, ry, an) in [(15.5, 47.0, 5.5, 2.4, -20), (24.0, 60.5, 5.0, 2.2, 25), (116, 56.0, 6.0, 2.4, 30)]:
        d.polygon(esc(elipse(cx, cy, rx, ry, an)), fill=255)
    d.line(esc([(118, 49.5), (117, 53.0)]), fill=255, width=int(1.4 * S))
    # --- patas ---
    for x0 in (65.0, 72.0):
        d.line(esc([(x0 + 1.5, 44.0), (x0, 52.0)]), fill=255, width=int(1.5 * S))
    # --- cola, cuerpo, cuello, cabeza ---
    d.polygon(esc([(80, 38), (106, 50), (108, 54.5), (101, 54), (77, 45)]), fill=255)
    d.polygon(esc(elipse(70, 35, 17.5, 11.5, 24)), fill=255)
    d.polygon(esc(elipse(58.5, 28.0 + dy * 0.5, 8.5, 9.5, -30)), fill=255)
    d.polygon(esc(elipse(hx, hy, 8.6, 8.0)), fill=255)
    # --- pico ---
    if canta:
        d.polygon(esc([(44.5, hy - 3.4), (33.0, hy - 6.8), (44.0, hy + 0.2)]), fill=255)
        d.polygon(esc([(44.0, hy + 1.2), (35.0, hy + 4.8), (44.5, hy + 4.0)]), fill=255)
    else:
        d.polygon(esc([(43.5, hy - 2.2), (33.0, hy + 0.4), (43.5, hy + 2.4)]), fill=255)

    a = np.asarray(g, dtype=np.float32).reshape(H, S, W, S).mean(axis=(1, 3)) / 255.0
    im = Image.fromarray(((a >= 0.5) * 255).astype(np.uint8))
    p = ImageDraw.Draw(im)
    ex, ey = int(round(hx - 2)), int(round(hy - 1))
    # --- detalles oscuros a 1 px ---
    if ojo_cerrado:
        p.line([(ex - 2, ey + 1), (ex + 1, ey + 1)], fill=0)
        p.point([(ex - 2, ey), (ex + 1, ey)], fill=0)
    else:
        p.ellipse([ex - 2, ey - 2, ex + 1, ey + 1], fill=0)
        p.point((ex - 1, ey - 1), fill=255)            # brillo del ojo
    if not canta:
        p.line([(43, int(round(hy))), (44, int(round(hy)))], fill=0)   # comisura del pico
    # ala: contorno + tres plumas largas hacia la cola
    ala = [(62, 31), (67, 28), (75, 28), (82, 31), (88, 36), (94, 42), (86, 41), (77, 39), (68, 37), (63, 34)]
    p.line(ala + [ala[0]], fill=0)
    for (x0, y0, x1, y1) in [(71, 33, 86, 39)]:
        p.line([(x0, y0), (x1, y1)], fill=0)
    # motas del pecho (el zorzal)
    for (x, y) in [(55, 31), (58, 34), (54, 35), (57, 38), (61, 38), (60, 41), (64, 41), (63, 44), (67, 44)]:
        p.point([(x, y), (x + 1, y)], fill=0)
    # separación entre el cuerpo y la cola
    p.line([(79, 42), (84, 46)], fill=0)
    return (np.asarray(im) > 127)


def a_bytes(b):
    out = bytearray(1024)
    for pg in range(8):
        for x in range(W):
            v = 0
            for k in range(8):
                if b[pg * 8 + k, x]:
                    v |= 1 << k
            out[pg * 128 + x] = v
    return out


def main():
    cuadros = [("REPOSO", cuadro()), ("PARPADEO", cuadro(ojo_cerrado=True)), ("CANTA", cuadro(canta=True))]
    # vista previa: los tres cuadros, ampliados 4×, con un borde entre ellos
    ancho = W * 4
    prev = Image.new("RGB", (ancho * len(cuadros) + 8 * (len(cuadros) - 1), H * 4), (60, 60, 60))
    for i, (_, b) in enumerate(cuadros):
        img = Image.fromarray(np.where(b, 235, 8).astype(np.uint8)).resize((ancho, H * 4), Image.NEAREST)
        prev.paste(img.convert("RGB"), (i * (ancho + 8), 0))
    prev.save(os.path.join(AQUI, "ave_vista_previa.png"))

    lineas = [MARCAS[0] + " — los escribe generar_ave.py, no se editan a mano",
              f"#define AVE_CUADROS {len(cuadros)}",
              "enum { AVE_REPOSO, AVE_PARPADEO, AVE_CANTA };",
              "const uint8_t AVE[AVE_CUADROS][1024] = {"]
    for nombre, b in cuadros:
        datos = a_bytes(b)
        lineas.append(f"  {{ // {nombre}")
        for i in range(0, 1024, 32):
            lineas.append("    " + ",".join(f"0x{v:02X}" for v in datos[i:i + 32]) + ",")
        lineas.append("  },")
    lineas += ["};", MARCAS[1]]
    bloque = "\n".join(lineas)

    with open(INO, encoding="utf-8") as f:
        texto = f.read()
    i0, i1 = texto.find(MARCAS[0]), texto.find(MARCAS[1])
    if i0 < 0 or i1 < 0:
        raise SystemExit(f"No encuentro las marcas {MARCAS} en {INO}")
    i1 += len(MARCAS[1])
    texto = texto[:i0] + bloque + texto[i1:]
    with open(INO, "w", encoding="utf-8", newline="\n") as f:
        f.write(texto)
    print(f"{len(cuadros)} cuadros escritos en {os.path.basename(INO)} · vista previa en ave_vista_previa.png")


if __name__ == "__main__":
    main()
