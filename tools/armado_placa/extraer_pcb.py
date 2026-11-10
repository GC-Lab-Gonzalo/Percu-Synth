"""Convierte el diseño EasyEDA de la PCB V2.0 en pcb_v2.js para la app de armado.

    python tools/armado_placa/extraer_pcb.py

Lee Hardware/PCB_Percu-synth_2026-09-04.json V2.json (formato EasyEDA estándar) y escribe
pcb_v2.js con la geometría que la página dibuja: contorno, agujeros, pistas y vías por cara,
rellenos de GND, serigrafía de las dos caras y, por cada componente, su designador, su posición
y sus pads (con la red a la que va cada pad). Las coordenadas quedan en unidades de EasyEDA
(10 mil = 0,254 mm) con el origen en la esquina de arriba a la izquierda del contorno.
"""
import json
import re
from pathlib import Path

RAIZ = Path(__file__).resolve().parents[2]
ORIGEN = RAIZ / 'Hardware' / 'PCB_Percu-synth_2026-09-04.json V2.json'
DESTINO = Path(__file__).with_name('pcb_v2.js')

CARA = {'1': 'top', '2': 'bot'}
SERIGRAFIA = {'3': 'top', '4': 'bot'}

d = json.loads(ORIGEN.read_text(encoding='utf-8'))
contorno = next(s for s in d['shape'] if s.startswith('TRACK~') and s.split('~')[2] == '10')
xs = [float(v) for v in contorno.split('~')[4].split()[0::2]]
ys = [float(v) for v in contorno.split('~')[4].split()[1::2]]
X0, Y0 = min(xs), min(ys)
ANCHO, ALTO = max(xs) - X0, max(ys) - Y0


def r(v):
    v = round(v, 1)
    return int(v) if v == int(v) else v


def mover(texto):
    """Traslada un path SVG absoluto (M L C Q A Z). En los arcos sólo se mueve el punto final:
    radios, rotación y banderas quedan como están."""
    fichas = re.findall(r'[A-Za-z]|-?\d*\.?\d+(?:e-?\d+)?', texto)
    out, cmd, k = [], 'M', 0
    for f in fichas:
        if f.isalpha():
            cmd, k = f.upper(), 0
            out.append(cmd)
            continue
        v = float(f)
        if cmd == 'A':
            pos = k % 7
            v = v - X0 if pos == 5 else v - Y0 if pos == 6 else v
        else:
            v = v - (X0 if k % 2 == 0 else Y0)
        out.append(str(r(v)))
        k += 1
    return re.sub(r' ?([A-Z]) ?', r'\1', ' '.join(out))


def mover_arco(path):
    return mover(path)


def puntos_a_path(pts):
    v = pts.split()
    return 'M' + ' L'.join(f'{r(float(v[i]) - X0)} {r(float(v[i + 1]) - Y0)}' for i in range(0, len(v) - 1, 2))


def serigrafia(sub, destino):
    """Agrega a destino[cara] lo que esté en las capas de serigrafía: [path, ancho] o [path, 0] si va relleno."""
    t = sub.split('~')
    tipo = t[0]
    if tipo == 'TRACK' and t[2] in SERIGRAFIA:
        destino[SERIGRAFIA[t[2]]].append([puntos_a_path(t[4]), r(float(t[1]))])
    elif tipo == 'CIRCLE' and t[5] in SERIGRAFIA:
        cx, cy, rad = float(t[1]) - X0, float(t[2]) - Y0, float(t[3])
        destino[SERIGRAFIA[t[5]]].append([f'C{r(cx)} {r(cy)} {r(rad)}', r(float(t[4]))])
    elif tipo == 'ARC' and t[2] in SERIGRAFIA:
        destino[SERIGRAFIA[t[2]]].append([mover_arco(t[4]), r(float(t[1]))])
    elif tipo == 'SOLIDREGION' and t[1] in SERIGRAFIA:
        destino[SERIGRAFIA[t[1]]].append([mover(t[3]), 0])
    elif tipo == 'TEXT' and t[7] in SERIGRAFIA and t[12] != 'none' and t[11]:
        destino[SERIGRAFIA[t[7]]].append([mover(t[11]), r(float(t[4]))])
    elif tipo == 'SVGNODE':
        nodo = json.loads(sub[len('SVGNODE~'):])
        capa = nodo.get('layerid')
        if capa in SERIGRAFIA and nodo['attrs'].get('d'):
            destino[SERIGRAFIA[capa]].append([mover(nodo['attrs']['d']), 0])


def pad(sub):
    t = sub.split('~')
    forma, x, y, w, h, capa, red, num, hueco = t[1], float(t[2]), float(t[3]), float(t[4]), float(t[5]), t[6], t[7], t[8], float(t[9])
    p = {'x': r(x - X0), 'y': r(y - Y0), 'w': r(w), 'h': r(h), 'f': forma[0], 'n': num, 'red': red,
         'cara': 'tht' if capa == '11' else CARA.get(capa, capa)}
    if hueco:
        p['hueco'] = r(hueco)
        largo = float(t[13]) if len(t) > 13 and t[13] else 0
        if largo:
            p['ranura'] = r(largo)
    if forma in ('RECT', 'POLYGON') and t[10]:
        p['pts'] = puntos_a_path(t[10]) + 'Z'
    if t[11] and float(t[11] or 0):
        p['rot'] = r(float(t[11]))
    return p


pistas = {'top': [], 'bot': []}
vias, agujeros, rellenos = [], [], {'top': [], 'bot': []}
silk = {'top': [], 'bot': []}
comps = []

for s in d['shape']:
    tipo = s.split('~')[0]
    if tipo == 'TRACK':
        t = s.split('~')
        if t[2] in CARA:
            pistas[CARA[t[2]]].append([t[3], r(float(t[1])), puntos_a_path(t[4])])
        else:
            serigrafia(s, silk)
    elif tipo == 'VIA':
        t = s.split('~')
        vias.append([r(float(t[1]) - X0), r(float(t[2]) - Y0), r(float(t[3])), t[4], r(float(t[5]))])
    elif tipo == 'HOLE':
        t = s.split('~')
        agujeros.append([r(float(t[1]) - X0), r(float(t[2]) - Y0), r(float(t[3]))])
    elif tipo == 'COPPERAREA':
        # el relleno de GND es una trama de 45° que bajo la máscara negra no se ve: sólo el contorno
        t = s.split('~')
        if t[2] in CARA:
            rellenos[CARA[t[2]]].append(mover(t[4]))
    elif tipo == 'LIB':
        partes = s.split('#@$')
        cab = partes[0].split('~')
        kv = cab[3].split('`')
        para = dict(zip(kv[0::2], kv[1::2]))
        comp = {'x': r(float(cab[1]) - X0), 'y': r(float(cab[2]) - Y0), 'rot': r(float(cab[4] or 0)),
                'pkg': para.get('package', ''), 'pads': [], 'silk': {'top': [], 'bot': []}, 'agujeros': []}
        for sub in partes[1:]:
            t = sub.split('~')
            if sub.startswith('PAD~'):
                comp['pads'].append(pad(sub))
            elif sub.startswith('HOLE~'):
                comp['agujeros'].append([r(float(t[1]) - X0), r(float(t[2]) - Y0), r(float(t[3]))])
            elif sub.startswith('TEXT~') and t[1] == 'P':
                comp['id'] = t[10]
                serigrafia(sub, comp['silk'])
            elif sub.startswith('SOLIDREGION~') and t[1] == '11':
                comp.setdefault('ventanas', []).append(mover(t[3]))
            else:
                serigrafia(sub, comp['silk'])
        comps.append(comp)
    else:
        serigrafia(s, silk)

datos = {'ancho': r(ANCHO), 'alto': r(ALTO), 'agujeros': agujeros, 'vias': vias, 'pistas': pistas,
         'rellenos': rellenos, 'silk': silk, 'comps': comps}
texto = json.dumps(datos, ensure_ascii=False, separators=(',', ':'))
DESTINO.write_text('// Generado por extraer_pcb.py desde el diseño EasyEDA de la PCB V2.0. No editar a mano.\n'
                   f'const PCB = {texto};\n', encoding='utf-8')
print(f'{DESTINO.name}: {len(texto) / 1024:.0f} KB · {len(comps)} componentes · '
      f'{sum(len(v) for v in pistas.values())} pistas · {len(vias)} vías · placa {ANCHO * 0.254:.1f} × {ALTO * 0.254:.1f} mm')
for k, v in rellenos.items():
    print(f'  relleno {k}: {sum(len(c) for c in v) / 1024:.0f} KB')
print(f'  serigrafía suelta: {sum(len(p[0]) for v in silk.values() for p in v) / 1024:.0f} KB')
