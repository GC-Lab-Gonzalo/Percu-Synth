"""
Extrae las pistas MIDI de un proyecto de Ableton Live (.als) y genera cancion.h.

    python extraer_cancion.py "ruta/al/proyecto.als"

Lee las 3 primeras pistas MIDI del ARREGLO (no las de la vista de sesión) en orden:
melodía, bajo, arpegio. Respeta la posición de cada clip, su ventana (CurrentStart/End),
el loop y el offset interno (LoopStart + StartRelative), y corta las notas que pasan del
final del clip, igual que Ableton al reproducir. También lee el tempo del proyecto.

Tiempos en ticks de 480 por negra: con eso cualquier figura del archivo queda exacta.
"""
import gzip, sys, xml.etree.ElementTree as ET

PPQ = 480
NOMBRES = ["MELODIA", "BAJO", "ARPEGIO"]

def pistas_de(als):
    r = ET.fromstring(gzip.open(als).read())
    tempo = float(r.find(".//MasterTrack//Tempo/Manual").get("Value"))
    pistas, fin = [], 0.0
    for tr in r.iter("MidiTrack"):
        notas = []
        for c in tr.iter("MidiClip"):
            t0 = float(c.get("Time"))
            largo = float(c.find("CurrentEnd").get("Value")) - float(c.find("CurrentStart").get("Value"))
            off = float(c.find("Loop/LoopStart").get("Value")) + float(c.find("Loop/StartRelative").get("Value"))
            fin = max(fin, t0 + largo)
            for kt in c.iter("KeyTrack"):
                k = int(kt.find("MidiKey").get("Value"))
                for e in kt.iter("MidiNoteEvent"):
                    if e.get("IsEnabled") != "true":
                        continue
                    t, d = float(e.get("Time")), float(e.get("Duration"))
                    if t < off - 1e-6 or t >= off + largo - 1e-6:
                        continue
                    a = t0 + (t - off)
                    b = min(a + d, t0 + largo)
                    notas.append((round(a * PPQ), round(b * PPQ), k))
        notas.sort()
        pistas.append(notas)
    return tempo, fin, pistas[:3]

def main():
    als = sys.argv[1]
    tempo, fin, pistas = pistas_de(als)
    out = [
        "// Generado por extraer_cancion.py desde el proyecto de Ableton (%d pistas, %g BPM)." % (len(pistas), tempo),
        "// Tiempos en ticks de %d por negra. {inicio, fin, nota MIDI}. No editar a mano: regenerar." % PPQ,
        "#pragma once",
        "// (el tipo NotaMidi está definido en la_partida.ino, antes de incluir este archivo)",
        "",
        "#define PPQ %d" % PPQ,
        "#define BPM_ORIGINAL %.1ff" % tempo,
        "#define FIN_CANCION %d   // largo del arreglo: %g negras" % (round(fin * PPQ), fin),
        "",
    ]
    for n, p in zip(NOMBRES, pistas):
        out.append("const NotaMidi PISTA_%s[] = {" % n)
        for i in range(0, len(p), 4):
            out.append("  " + " ".join("{%6d, %6d, %2d}," % x for x in p[i:i + 4]))
        out += ["};", "#define N_%s %d" % (n, len(p)), ""]
        print("%-8s %3d notas  rango %d..%d" % (n, len(p), min(x[2] for x in p), max(x[2] for x in p)))
    open("cancion.h", "w", encoding="utf-8").write("\n".join(out))
    print("cancion.h escrito — %g BPM, %g negras" % (tempo, fin))

if __name__ == "__main__":
    main()
