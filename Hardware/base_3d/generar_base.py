"""
Base imprimible en 3D para la placa PercuSynth V2.0
====================================================

Reemplaza el marco de madera: un marco de pared ancha y cantos redondeados,
abierto por debajo, con seis postes donde se atornilla la placa (los mismos
seis agujeros M2 de la PCB).

Por qué es así:
- ABIERTA POR DEBAJO y SIN TRAVESAÑOS: un piso macizo de 20x12 cm es lo que
  más tiempo y plástico cuesta, y una cruz al centro estorba. La rigidez la dan
  la pared ancha y una ceja interior al ras de la mesa; una vez atornillada la
  placa, la propia PCB cierra el marco.
- LA PARED LLEGA AL RAS DE LA CARA SUPERIOR DE LA PCB, no más arriba: los
  jacks, el USB-C, el MIDI, la salida del DAC y las borneras enchufan desde el
  borde de la placa, y una pared más alta los tapa. Por eso "más alta" se
  consigue con más aire BAJO la placa, no con más borde sobre ella.
- AIRE BAJO LA PCB: los 6 LEDs WS2812 van montados POR DEBAJO de la placa
  (capa bottom) y las patas de los componentes asoman ~3 mm.

Las medidas salen de los archivos de fabricación, no de una regla:
contorno = Gerber_BoardOutlineLayer.GKO, agujeros = Drill_NPTH_Through.DRL
(V2.0, 2026-09-04, PCB de 1.6 mm). Si la placa cambia, se corrigen PCB_* y
AGUJEROS.

Uso:   pip install manifold3d trimesh numpy
       python generar_base.py
Salida: base_percusynth_v2.stl y base_percusynth_v2.3mf en esta carpeta.
Unidades: milímetros.
"""

import math
import os
import numpy as np
import trimesh
from manifold3d import CrossSection, JoinType

# ── Placa (de los gerbers V2.0) ──────────────────────────────────────────────
PCB_ANCHO = 201.00       # X
PCB_ALTO = 124.42        # Y
PCB_GROSOR = 1.6
# Agujeros de montaje Ø2.032 (tornillo M2), medidos desde la esquina inferior
# izquierda del contorno. Cuatro esquinas + dos al medio de los lados cortos.
AGUJEROS = [(x, y) for x in (5.433, 195.933) for y in (5.054, 62.210, 119.354)]

# ── Parámetros de la base (lo que se puede tocar) ────────────────────────────
HOLGURA = 0.4            # juego entre el canto de la PCB y la pared, por lado
PARED = 7.0              # grosor de pared
AIRE = 16.0              # de la mesa a la cara inferior de la PCB
BORDE = 0.0              # cuánto sube la pared por encima de la cara superior
                         # de la PCB. >1 mm empieza a estorbar a los plugs.
RADIO_ESQUINA = 10.0     # redondeo de las esquinas vistas desde arriba
RADIO_CANTO = 4.0        # redondeo del canto superior exterior (< PARED)
RADIO_PIE = 1.0          # chaflán redondeado abajo (evita la "pata de elefante")

POSTE_D = 7.0            # diámetro de los postes
PILOTO_D = 1.7           # agujero guía para tornillo autorroscante M2
PILOTO_PROF = 6.0        # profundidad del agujero (tornillo M2 x 6 u 8 mm)
                         # el agujero es ciego: la punta no sale por abajo

PESTANA = 6.0            # ceja interior al ras de la mesa (adherencia + rigidez)
PISO = 2.0               # grosor de la ceja

SEG = 64                 # resolución de los círculos
PASOS_CANTO = 16         # rebanadas del redondeo (0.25 mm c/u ≈ una capa)

# ── Geometría ────────────────────────────────────────────────────────────────
int_x = PCB_ANCHO + 2 * HOLGURA
int_y = PCB_ALTO + 2 * HOLGURA
ext_x = int_x + 2 * PARED
ext_y = int_y + 2 * PARED
alto = AIRE + PCB_GROSOR + BORDE

# Origen de la PCB dentro de la base (esquina inferior izquierda del contorno)
ox = PARED + HOLGURA
oy = PARED + HOLGURA


def rect_redondeado(w, h, r, x0=0.0, y0=0.0):
    """Rectángulo con esquinas redondeadas, esquina inferior izquierda en x0,y0."""
    if r <= 0:
        return CrossSection.square((w, h)).translate((x0, y0))
    base = CrossSection.square((w - 2 * r, h - 2 * r)).translate((x0 + r, y0 + r))
    return base.offset(r, JoinType.Round, circular_segments=SEG)


def contorno_exterior(entrada):
    """Contorno exterior encogido `entrada` mm hacia adentro (mismo centro)."""
    return rect_redondeado(ext_x - 2 * entrada, ext_y - 2 * entrada,
                           max(RADIO_ESQUINA - entrada, 0.1), entrada, entrada)


def prisma(seccion, z0, z1):
    return seccion.extrude(z1 - z0).translate((0, 0, z0))


hueco = CrossSection.square((int_x, int_y)).translate((PARED, PARED))


def anillo_redondeado(z0, z1, radio, arriba):
    """Tramo de pared cuyo borde exterior sigue un cuarto de círculo, en rebanadas."""
    piezas = None
    for i in range(PASOS_CANTO):
        za = z0 + (z1 - z0) * i / PASOS_CANTO
        zb = z0 + (z1 - z0) * (i + 1) / PASOS_CANTO
        # distancia al extremo, medida en el centro de la rebanada
        d = (z1 - (za + zb) / 2) if arriba else ((za + zb) / 2 - z0)
        entrada = radio - math.sqrt(max(radio * radio - (radio - d) ** 2, 0.0))
        rebanada = prisma(contorno_exterior(entrada) - hueco, za, zb)
        piezas = rebanada if piezas is None else piezas + rebanada
    return piezas


# Marco: pie redondeado + tramo recto + canto superior redondeado
z_pie = RADIO_PIE
z_canto = alto - RADIO_CANTO
base = anillo_redondeado(0, z_pie, RADIO_PIE, arriba=False)
base += prisma(contorno_exterior(0) - hueco, z_pie, z_canto)
base += anillo_redondeado(z_canto, alto, RADIO_CANTO, arriba=True)

# Ceja interior al ras de la mesa
ceja = CrossSection.square((int_x - 2 * PESTANA, int_y - 2 * PESTANA)).translate(
    (PARED + PESTANA, PARED + PESTANA))
base += prisma(hueco - ceja, 0, PISO)

# Postes: del piso a la cara inferior de la PCB, unidos a la pared lateral
for (hx, hy) in AGUJEROS:
    px, py = ox + hx, oy + hy
    poste = CrossSection.circle(POSTE_D / 2, SEG).translate((px, py))
    if hx < PCB_ANCHO / 2:
        puente = CrossSection.square((px - PARED + 0.5, POSTE_D)).translate(
            (PARED - 0.5, py - POSTE_D / 2))
    else:
        puente = CrossSection.square((ext_x - PARED + 0.5 - px, POSTE_D)).translate(
            (px, py - POSTE_D / 2))
    base += prisma(poste + puente, 0, AIRE)

# Agujeros guía (ciegos, desde arriba)
for (hx, hy) in AGUJEROS:
    px, py = ox + hx, oy + hy
    base -= prisma(CrossSection.circle(PILOTO_D / 2, SEG).translate((px, py)),
                   AIRE - PILOTO_PROF, AIRE + 1)

# ── Exportar ─────────────────────────────────────────────────────────────────
malla = base.to_mesh()
tm = trimesh.Trimesh(vertices=np.asarray(malla.vert_properties)[:, :3],
                     faces=np.asarray(malla.tri_verts), process=True)
assert tm.is_watertight, "la malla no quedó cerrada"

aqui = os.path.dirname(os.path.abspath(__file__))
tm.export(os.path.join(aqui, "base_percusynth_v2.stl"))
tm.export(os.path.join(aqui, "base_percusynth_v2.3mf"))

vol_cm3 = tm.volume / 1000
print(f"Exterior: {ext_x:.1f} x {ext_y:.1f} x {alto:.1f} mm")
print(f"Volumen: {vol_cm3:.1f} cm3  (~{vol_cm3 * 1.24:.0f} g de PLA al 100 %)")
