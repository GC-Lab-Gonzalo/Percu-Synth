"""Convierte el esquemático EasyEDA del PercuSynth en esquema_v2.js para la app de armado.

    python tools/armado_placa/extraer_esquema.py [archivo.json]

Sin argumento usa el JSON de esquemático más nuevo que haya en Hardware/ (exportado desde EasyEDA con
Archivo → Exportar → EasyEDA Source) y, si no hay ninguno, el que viene dentro del zip del proyecto.

Escribe el dibujo como SVG listo para insertar, con dos cosas que lo enlazan con la placa:
  · cada símbolo va en un <g data-esq="R6"> con el mismo designador que en la PCB;
  · cada cable, unión, etiqueta y pin lleva data-red="POT1": la red de la PCB a la que pertenece.
La red se saca de la conectividad del propio esquemático (cables, uniones y etiquetas con el mismo nombre)
y se nombra mirando qué pad de la PCB toca: el pin n de R6 en el esquema es el pad n de R6 en la placa.
"""
import json
import re
import sys
import zipfile
from html import escape
from pathlib import Path

RAIZ = Path(__file__).resolve().parents[2]
HW = RAIZ / 'Hardware'
DESTINO = Path(__file__).with_name('esquema_v2.js')
PCB = json.loads(Path(__file__).with_name('pcb_v2.js').read_text(encoding='utf-8').split('=', 1)[1].rstrip().rstrip(';'))


def cargar():
    if len(sys.argv) > 1:
        return json.loads(Path(sys.argv[1]).read_text(encoding='utf-8')), Path(sys.argv[1]).name
    sueltos = [p for p in HW.glob('*.json') if re.search(r'(^sch|schem|esquem)', p.name.lower())]
    if sueltos:
        p = max(sueltos, key=lambda p: p.stat().st_mtime)
        return json.loads(p.read_text(encoding='utf-8')), p.name
    z = next(HW.glob('Percu-synth_*.zip'))
    with zipfile.ZipFile(z) as f:
        nombre = next(n for n in f.namelist() if 'Schematic' in n)
        return json.loads(f.read(nombre)), f'{z.name}/{nombre}'


doc, ORIGEN = cargar()
hoja = doc['schematics'][0]['dataStr'] if 'schematics' in doc else doc
SHAPES = hoja['shape']
BB = hoja.get('BBox') or {'x': 0, 'y': -807, 'width': 1149, 'height': 807.5}

# ---------------------------------------------------------------- conectividad
def k(x, y):
    return (round(float(x), 1), round(float(y), 1))

padre = {}
def raiz(a):
    padre.setdefault(a, a)
    while padre[a] != a:
        padre[a] = padre[padre[a]]
        a = padre[a]
    return a
def unir(a, b):
    padre[raiz(a)] = raiz(b)

def pts(s):
    v = [float(n) for n in s.split()]
    return [k(v[i], v[i + 1]) for i in range(0, len(v) - 1, 2)]

cables = []                                   # [(puntos, color, ancho)]
for s in SHAPES:
    if s.startswith('W~'):
        t = s.split('~')
        p = pts(t[1])
        cables.append((p, t[2], t[3]))
        for a, b in zip(p, p[1:]):
            unir(a, b)

def sobre(pt, a, b):                          # pt cae sobre el tramo a–b (los cables son horizontales o verticales)
    (x, y), (x1, y1), (x2, y2) = pt, a, b
    if abs((x2 - x1) * (y - y1) - (y2 - y1) * (x - x1)) > 0.05:
        return False
    return min(x1, x2) - .05 <= x <= max(x1, x2) + .05 and min(y1, y2) - .05 <= y <= max(y1, y2) + .05

def enganchar(pt):
    raiz(pt)
    for p, _, _ in cables:
        for a, b in zip(p, p[1:]):
            if sobre(pt, a, b):
                unir(pt, a)
                break

for p, _, _ in cables:                        # puntas de cable que caen en el medio de otro (uniones en T)
    enganchar(p[0]); enganchar(p[-1])

pines = []                                    # (punto, designador, número)
banderas = []                                 # (punto, nombre)
libs = []
for s in SHAPES:
    if s.startswith('LIB~'):
        partes = s.split('#@$')
        des = ''
        for sub in partes[1:]:
            t = sub.split('~')
            if sub.startswith('T~P~'):
                des = t[12]
        libs.append((des, partes))
        for sub in partes[1:]:
            if sub.startswith('P~'):
                seg = sub.split('^^')
                num = seg[0].split('~')[3]
                x, y = seg[1].split('~')[:2]
                pt = k(x, y)
                pines.append((pt, des, num))
                enganchar(pt)
    elif s.startswith('F~'):
        seg = s.split('^^')
        x, y = seg[1].split('~')[:2]
        nombre = seg[2].split('~')[0]
        pt = k(x, y)
        banderas.append((pt, nombre))
        enganchar(pt)

# las etiquetas con el mismo nombre son la misma red
por_nombre = {}
for pt, n in banderas:
    if n in por_nombre:
        unir(pt, por_nombre[n])
    else:
        por_nombre[n] = pt

PADS = {c['id']: {p['n']: p['red'] for p in c['pads']} for c in PCB['comps']}
NORMAL = {'gnd': 'GND', '3.3v': '3.3V', '3v3': '3.3V', '5v': '5V', 'vcc': '5V'}
red_de = {}
for pt, des, num in pines:
    r = PADS.get(des, {}).get(num)
    if r:
        red_de.setdefault(raiz(pt), r)
for pt, n in banderas:
    red_de.setdefault(raiz(pt), NORMAL.get(n.lower(), n.upper()))
def red(pt):
    return red_de.get(raiz(pt), '')

# ---------------------------------------------------------------- dibujo
def num(v):
    return f'{float(v):g}'

def attrs(**a):
    return ' '.join(f'{k.replace("_", "-")}="{escape(str(v))}"' for k, v in a.items() if v not in (None, ''))

def trazo(color, ancho, relleno='none'):
    return attrs(stroke=color or '#979FAD', stroke_width=ancho or 1, fill=relleno if relleno and relleno != 'transparent' else 'none')

def texto(x, y, rot, s, color, ancla, tam='7pt', peso=None):
    t = float(re.sub(r'[^\d.]', '', tam or '') or 7)
    a = {'start': 'start', 'end': 'end', 'middle': 'middle'}.get(ancla or 'start', 'start')
    tr = f' transform="rotate({num(rot)} {num(x)} {num(y)})"' if rot and float(rot) else ''
    return (f'<text x="{num(x)}" y="{num(y)}" font-size="{t:g}" fill="{color or "#979FAD"}" text-anchor="{a}"'
            f'{" font-weight=" + chr(34) + peso + chr(34) if peso else ""}{tr}>{escape(s)}</text>')

def figura(sub, extra=''):
    t = sub.split('~')
    tipo = t[0]
    if tipo == 'PL':
        return f'<polyline points="{t[1]}" {trazo(t[2], t[3], t[5])}{extra}/>'
    if tipo == 'PG':
        return f'<polygon points="{t[1]}" {trazo(t[2], t[3], t[5])}{extra}/>'
    if tipo == 'PT':
        return f'<path d="{t[1]}" {trazo(t[2], t[3], t[5])}{extra}/>'
    if tipo == 'A':
        return f'<path d="{t[1]}" {trazo(t[3], t[4], t[6])}{extra}/>'
    if tipo == 'R':
        return (f'<rect x="{t[1]}" y="{t[2]}" rx="{t[3] or 0}" ry="{t[4] or 0}" width="{t[5]}" height="{t[6]}" '
                f'{trazo(t[7], t[8], t[10])}{extra}/>')
    if tipo == 'E':
        return f'<ellipse cx="{t[1]}" cy="{t[2]}" rx="{t[3]}" ry="{t[4]}" {trazo(t[5], t[6], t[8])}{extra}/>'
    if tipo == 'C':
        return f'<circle cx="{t[1]}" cy="{t[2]}" r="{t[3]}" {trazo(t[4], t[5], t[7])}{extra}/>'
    if tipo == 'T':
        if t[13] == '0':
            return ''
        return texto(t[2], t[3], t[4], t[12], t[5], t[14], t[7] or '7pt')
    return ''

svg = []
cajas = {}
for des, partes in libs:
    h = partes[0].split('~')
    cuerpo = []
    xs, ys = [], []
    for sub in partes[1:]:
        if sub.startswith('P~'):
            seg = sub.split('^^')
            cab = seg[0].split('~')
            if cab[1] == 'none':
                continue
            x, y = seg[1].split('~')[:2]
            r = red(k(x, y))
            d, color = seg[2].split('~')[:2]
            g = [f'<path d="{d}" stroke="{color}" fill="none" class="pin"/>']
            for nt in (seg[3], seg[4]):                          # nombre y número del pin
                v = nt.split('~')
                if v[0] == '1':
                    g.append(texto(v[1], v[2], v[3], v[4], v[8] or '#979FAD', v[5], v[7] or '7pt'))
            if len(seg) > 5 and seg[5].startswith('1'):
                v = seg[5].split('~')
                g.append(f'<circle cx="{v[1]}" cy="{v[2]}" r="2" stroke="{color}" fill="none"/>')
            cuerpo.append(f'<g data-red="{escape(r)}" data-pin="{escape(cab[3])}">{"".join(g)}</g>')
            xs.append(float(x)); ys.append(float(y))
        else:
            f = figura(sub)
            if f:
                cuerpo.append(f)
            for n in re.findall(r'-?\d+(?:\.\d+)?', ' '.join(sub.split('~')[1:2])):
                pass
            t = sub.split('~')
            if t[0] in ('PL', 'PG'):
                v = [float(n) for n in t[1].split()]
                xs += v[0::2]; ys += v[1::2]
            elif t[0] == 'R':
                xs += [float(t[1]), float(t[1]) + float(t[5])]; ys += [float(t[2]), float(t[2]) + float(t[6])]
            elif t[0] in ('E', 'C'):
                rx = float(t[3]); ry = float(t[4]) if t[0] == 'E' else rx
                xs += [float(t[1]) - rx, float(t[1]) + rx]; ys += [float(t[2]) - ry, float(t[2]) + ry]
    if des and des != 'A' and xs:
        cajas[des] = [round(min(xs), 1), round(min(ys), 1), round(max(xs), 1), round(max(ys), 1)]
    marco = h[3].startswith('package`NONE') or not des
    svg.append(f'<g {"data-marco" if marco else "data-esq=" + chr(34) + escape(des) + chr(34)}>{"".join(cuerpo)}</g>')

for p, color, ancho in cables:
    r = red(p[0])
    svg.append(f'<polyline points="{" ".join(f"{x:g},{y:g}" for x, y in p)}" stroke="{color}" stroke-width="{ancho}" '
               f'fill="none" data-red="{escape(r)}" class="cable"/>')

for s in SHAPES:
    if s.startswith('J~'):
        t = s.split('~')
        svg.append(f'<circle cx="{t[1]}" cy="{t[2]}" r="{t[3]}" fill="{t[4]}" data-red="{escape(red(k(t[1], t[2])))}" class="union"/>')
    elif s.startswith('F~'):
        seg = s.split('^^')
        x, y = seg[1].split('~')[:2]
        n = seg[2].split('~')
        g = [figura(sub) for sub in seg[3:]]
        if n[6] != '0':
            g.append(texto(n[2], n[3], n[4], n[0], n[1], n[5], n[8], '600'))
        svg.append(f'<g data-red="{escape(red(k(x, y)))}" class="etiqueta">{"".join(g)}</g>')
    elif s.startswith('I~'):
        t = s.split('~')
        svg.append(f'<image x="{t[1]}" y="{t[2]}" width="{t[3]}" height="{t[4]}" href="{t[6]}"/>')

datos = {'origen': ORIGEN, 'caja': [BB['x'], BB['y'], BB['width'], BB['height']], 'cajas': cajas, 'svg': ''.join(svg)}
DESTINO.write_text('// Generado por extraer_esquema.py desde el esquemático EasyEDA. No editar a mano.\n'
                   f'const ESQUEMA = {json.dumps(datos, ensure_ascii=False, separators=(",", ":"))};\n', encoding='utf-8')
sin_red = sum(1 for p, _, _ in cables if not red(p[0]))
print(f'{DESTINO.name} desde {ORIGEN}: {len(cajas)} símbolos · {len(cables)} cables ({sin_red} sin red) · '
      f'{len(set(red_de.values()))} redes · {DESTINO.stat().st_size / 1024:.0f} KB')
faltan = sorted(set(PADS) - set(cajas))
if faltan:
    print('  en la PCB pero no en el esquema:', ', '.join(faltan))
