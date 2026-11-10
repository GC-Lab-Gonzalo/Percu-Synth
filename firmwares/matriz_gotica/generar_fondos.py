# Dibuja los 6 LUGARES de la pantalla OLED (128×64, 1 bit) de matriz_gotica y los escribe DENTRO del .ino,
# entre los marcadores «// >>> FONDOS» y «// <<< FONDOS» (el firmware sigue siendo un solo archivo).
#   python generar_fondos.py            → reescribe el bloque en matriz_gotica.ino y deja fondos.png para mirar
# Se dibuja a 4× y se umbraliza. Cada lugar declara su FOCO: el punto de donde sale la luz que late con el bombo.
# La franja de abajo (y > 44) se deja simple: ahí baila la gente.
from PIL import Image, ImageDraw
import math, os, re

K = 4; W, H = 128 * K, 64 * K
AQUI = os.path.dirname(os.path.abspath(__file__))

def lienzo():
    im = Image.new('L', (W, H), 0); return im, ImageDraw.Draw(im)
def fin(im):
    return im.resize((128, 64), Image.LANCZOS).point(lambda v: 255 if v > 110 else 0)
def L(d, pts, w=1.0, f=255):
    d.line([(x * K, y * K) for x, y in pts], fill=f, width=max(1, int(round(w * K))), joint='curve')
def disco(d, x, y, r, f=255, borde=None, w=1.0):
    d.ellipse(((x - r) * K, (y - r) * K, (x + r) * K, (y + r) * K), fill=f, outline=borde, width=max(1, int(round(w * K))))
def anillo(d, x, y, rx, ry, w=1.0):
    d.ellipse(((x - rx) * K, (y - ry) * K, (x + rx) * K, (y + ry) * K), outline=255, width=max(1, int(round(w * K))))
def ojival(d, x0, x1, ybase, ytop, w=1.0, relleno=None):
    a = x1 - x0; izq = []; der = []
    alto = a * math.sin(math.radians(60))
    for i in range(41):
        t = i / 40 * math.radians(60)
        izq.append((x1 - a * math.cos(t), ytop + alto - a * math.sin(t)))
        der.append((x0 + a * math.cos(t), ytop + alto - a * math.sin(t)))
    forma = [(x0, ybase), (x0, ytop + alto)] + izq + der[::-1] + [(x1, ytop + alto), (x1, ybase)]
    if relleno is not None: d.polygon([(x * K, y * K) for x, y in forma], fill=relleno)
    L(d, forma, w)
def medio_punto(d, x0, x1, ybase, yarr, w=1.0):
    r = (x1 - x0) / 2; cx = (x0 + x1) / 2
    pts = [(x0, ybase)] + [(cx - r * math.cos(math.pi * i / 40), yarr + r * 0 - (0) + (yarr_c(yarr, r) - r * math.sin(math.pi * i / 40))) for i in range(41)] + [(x1, ybase)]
    L(d, pts, w)
def yarr_c(yarr, r): return yarr + r          # centro del semicírculo cuya clave queda en yarr
def cruz(d, x, y, a=3, w=1.0):
    L(d, [(x, y - a), (x, y + a * 1.4)], w); L(d, [(x - a * 0.7, y - a * 0.2), (x + a * 0.7, y - a * 0.2)], w)

def cripta():
    im, d = lienzo()
    xs = [3, 44, 84, 125]
    for a, b in zip(xs, xs[1:]): medio_punto(d, a, b, 34, 6, 1.4)
    for x in xs:
        d.rectangle(((x - 2.5) * K, 30 * K, (x + 2.5) * K, 52 * K), fill=255)
        L(d, [(x - 4.5, 30), (x + 4.5, 30)], 1.6)
    L(d, [(64, 0), (64, 13)], 1); d.polygon([(64 * K, 13 * K), (67 * K, 17 * K), (64 * K, 21 * K), (61 * K, 17 * K)], fill=255)
    for cx in (24, 104):                               # nichos con cruz
        ojival(d, cx - 6, cx + 6, 46, 28, 1); cruz(d, cx, 38, 2.6)
    L(d, [(52, 46), (52, 40), (55, 37), (73, 37), (76, 40), (76, 46)], 1.2); L(d, [(52, 40), (76, 40)], 1)   # el sarcófago
    L(d, [(0, 52), (127, 52)], 1)
    return fin(im), (64, 17)

def tunel():
    im, d = lienzo(); cx, cy = 64, 29
    radios = [(70, 46), (46, 30), (29, 19), (18, 12), (10, 7), (5, 3.5)]
    for i, (rx, ry) in enumerate(radios): anillo(d, cx, cy, rx, ry, 1.8 - 0.25 * i)
    for k in range(10):
        a = math.pi * (k + 0.5) / 5
        L(d, [(cx + 5 * math.cos(a), cy + 3.5 * math.sin(a)), (cx + 70 * math.cos(a), cy + 46 * math.sin(a))], 0.8)
    disco(d, cx, cy, 2.2)
    return fin(im), (cx, cy)

def baile():
    # la pista: una araña de velas colgando al centro y el piso de damero en fuga
    im, d = lienzo(); cx = 64
    L(d, [(cx, 0), (cx, 9)], 1)
    L(d, [(cx - 22, 16), (cx - 12, 22), (cx, 24), (cx + 12, 22), (cx + 22, 16)], 1.4)      # el aro de la araña
    L(d, [(cx - 12, 22), (cx, 9), (cx + 12, 22)], 1); L(d, [(cx - 22, 16), (cx, 9), (cx + 22, 16)], 1)
    for x, y in ((cx - 22, 16), (cx - 12, 22), (cx, 24), (cx + 12, 22), (cx + 22, 16)):    # las velas con su llama
        L(d, [(x, y), (x, y - 5)], 1.6); disco(d, x, y - 7, 1.1)
    vy = 34                                                                               # horizonte del piso
    L(d, [(0, vy), (127, vy)], 1)
    for k in range(-8, 9): L(d, [(cx + k * 5, vy), (cx + k * 26, 64)], 0.8)
    for y in (38, 44, 53): L(d, [(0, y), (127, y)], 0.8)
    for x in (8, 119):                                                                    # dos candelabros de pie
        L(d, [(x, vy), (x, 12)], 1.4); L(d, [(x - 4, 14), (x + 4, 14)], 1.2)
        for dx in (-4, 0, 4): L(d, [(x + dx, 14), (x + dx, 10)], 1); disco(d, x + dx, 8, 1.0)
    return fin(im), (cx, 15)

def salon():
    # el salón: tres ventanales ojivales con tracería, la luna en el del medio y cortinas a los lados
    im, d = lienzo()
    for cx, an, top in ((28, 11, 12), (64, 15, 4), (100, 11, 12)):
        ojival(d, cx - an, cx + an, 52, top, 1.4)
        L(d, [(cx, top + an * 0.9), (cx, 52)], 1); L(d, [(cx - an, 34), (cx + an, 34)], 1)
    disco(d, 64, 22, 5)
    for sx, x0 in ((1, 0), (-1, 127)):                                                    # cortinas recogidas
        L(d, [(x0, 0), (x0 + sx * 12, 0), (x0 + sx * 9, 20), (x0 + sx * 3, 34), (x0 + sx * 8, 52)], 1.3)
        L(d, [(x0 + sx * 5, 0), (x0 + sx * 4, 20), (x0 + sx * 1, 34)], 0.8); L(d, [(x0, 34), (x0 + sx * 6, 34)], 1.6)
    L(d, [(0, 52), (127, 52)], 1)
    return fin(im), (64, 22)

def procesion():
    im, d = lienzo()
    disco(d, 102, 13, 7)
    for x0 in (14, 46):                                # dos torres con aguja y cruz
        L(d, [(x0, 54), (x0, 22), (x0 + 16, 22), (x0 + 16, 54)], 1.3)
        L(d, [(x0 - 1, 22), (x0 + 8, 5), (x0 + 17, 22)], 1.3); cruz(d, x0 + 8, 2.5, 1.8)
        ojival(d, x0 + 5, x0 + 11, 40, 27, 1)
    L(d, [(30, 30), (38, 18), (46, 30)], 1.3)          # el hastial, con su rosetón
    disco(d, 38, 31, 4.5, f=0, borde=255, w=1); cruz(d, 38, 31.5, 2.2, 0.8)
    ojival(d, 34, 42, 54, 40, 1)
    L(d, [(0, 50), (14, 50)], 1); L(d, [(62, 50), (127, 50)], 1)
    for x, a in ((76, 3), (90, 2.4), (104, 3), (118, 2.4)): cruz(d, x, 44, a, 1.2)   # el camposanto
    return fin(im), (102, 13)

def forja():
    im, d = lienzo()
    medio_punto(d, 36, 92, 54, 12, 2.0); medio_punto(d, 41, 87, 54, 17, 1)
    L(d, [(44, 12), (52, 0)], 1.2); L(d, [(84, 12), (76, 0)], 1.2)      # la campana de la chimenea
    for cx, alto, an in ((64, 22, 9), (53, 13, 6), (75, 14, 6)):        # las llamas
        d.polygon([((cx - an) * K, 50 * K), ((cx - an * 0.5) * K, (50 - alto * 0.5) * K), (cx * K, (50 - alto) * K),
                   ((cx + an * 0.5) * K, (50 - alto * 0.5) * K), ((cx + an) * K, 50 * K)], fill=255)
    d.polygon([(60 * K, 50 * K), (64 * K, 38 * K), (68 * K, 50 * K)], fill=0)
    for x in (14, 114):                                                 # cadenas con gancho
        for y in range(0, 26, 4): anillo(d, x, y + 2, 1.5, 2.2, 0.9)
        L(d, [(x, 27), (x, 31), (x + 3, 33), (x + 3, 30)], 1)
    for x, y in ((6, 14), (24, 22), (100, 16), (122, 24), (8, 34), (120, 40)): L(d, [(x - 3, y), (x + 3, y)], 0.8)   # ladrillos
    L(d, [(0, 54), (127, 54)], 1)
    return fin(im), (64, 34)

LUGARES = [('CRIPTA', cripta), ('TUNEL', tunel), ('BAILE', baile), ('PROCESION', procesion), ('FORJA', forja), ('SALON', salon)]   # el orden de ESCENARIOS[]

def paginas(im):                                       # al orden de memoria del SSD1306: 8 páginas × 128 columnas, bit 0 arriba
    px = im.load(); out = bytearray(1024)
    for y in range(64):
        for x in range(128):
            if px[x, y]: out[(y >> 3) * 128 + x] |= 1 << (y & 7)
    return out

def main():
    ims = []; focos = []; txt = []
    for nombre, f in LUGARES:
        im, foco = f(); ims.append(im); focos.append(foco)
        b = paginas(im)
        filas = ['    ' + ', '.join('0x%02X' % v for v in b[i:i + 32]) for i in range(0, 1024, 32)]
        txt.append('  { // %s\n%s }' % (nombre, ',\n'.join(filas)))
    bloque = ('// >>> FONDOS — los escribe generar_fondos.py; no se editan a mano\n'
              'const uint8_t FONDO[%d][1024] PROGMEM = {\n%s\n};\n'
              'const uint8_t FOCO[%d][2] = { %s };   // de dónde sale la luz en cada lugar (x, y)\n'
              '// <<< FONDOS') % (len(ims), ',\n'.join(txt), len(ims), ', '.join('{ %d, %d }' % (round(x), round(y)) for x, y in focos))
    open(os.path.join(AQUI, 'fondos.inc'), 'w', encoding='utf-8').write(bloque + '\n')
    ino = os.path.join(AQUI, 'matriz_gotica.ino')
    crudo = open(ino, 'rb').read().decode('utf-8'); crlf = '\r\n' in crudo[:4000]
    if crlf: crudo = crudo.replace('\r\n', '\n')
    if '// >>> FONDOS' in crudo:
        a = crudo.index('// >>> FONDOS'); b = crudo.index('// <<< FONDOS') + len('// <<< FONDOS')
        crudo = crudo[:a] + bloque + crudo[b:]
        if crlf: crudo = crudo.replace('\n', '\r\n')
        open(ino, 'wb').write(crudo.encode('utf-8')); print('bloque FONDOS reescrito en matriz_gotica.ino')
    else: print('el .ino todavía no tiene los marcadores FONDOS: quedó fondos.inc')
    Z = 3; hoja = Image.new('L', (2 * (128 * Z + 12) + 12, 3 * (64 * Z + 12) + 12), 60)
    for i, im in enumerate(ims): hoja.paste(im.resize((128 * Z, 64 * Z), Image.NEAREST), (12 + (i % 2) * (128 * Z + 12), 12 + (i // 2) * (64 * Z + 12)))
    hoja.save(os.path.join(AQUI, 'fondos.png'))

if __name__ == '__main__': main()
