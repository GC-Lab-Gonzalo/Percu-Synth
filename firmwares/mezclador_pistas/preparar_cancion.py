"""
Prepara una canción separada en pistas para el firmware mezclador_pistas.

Uso:
    python preparar_cancion.py <carpeta_con_las_pistas> <carpeta_de_salida> [titulo] [artista] [--nombre cancion2]

Busca en la carpeta archivos cuyo nombre contenga «drums», «bass», «vocals» y «other» (como los
entrega Moises / Demucs), y si hay uno con «metronome» lo usa para medir el BPM y el primer pulso.
Escribe en la carpeta de salida:
    cancion.wav   4 canales mono intercalados (batería, bajo, voz, otros), 16 bit, 44.1 kHz
    cancion.txt   bpm, primer_pulso_ms, título y artista
Copia esos dos archivos a la raíz de la microSD.

Necesita ffmpeg en el PATH y numpy.
"""
import os, subprocess, sys, tempfile
import numpy as np

ORDEN = [('bateria', 'drums'), ('bajo', 'bass'), ('voz', 'vocals'), ('otros', 'other')]


def buscar(carpeta, clave):
    for f in sorted(os.listdir(carpeta)):
        if clave in f.lower():
            return os.path.join(carpeta, f)
    return None


def medir_metronomo(ruta):
    raw = os.path.join(tempfile.gettempdir(), 'metro.raw')
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', ruta, '-ac', '1', '-ar', '44100', '-f', 's16le', raw], check=True)
    m = np.fromfile(raw, dtype=np.int16).astype(np.float32) / 32768
    idx = np.where(np.abs(m) > 0.3 * np.abs(m).max())[0]
    clicks = [idx[0]]
    for i in idx[1:]:
        if i - clicks[-1] > 0.2 * 44100:
            clicks.append(i)
    clicks = np.array(clicks) / 44100
    # Ajuste lineal sobre TODOS los clicks, no la mediana de los intervalos: algunos metrónomos
    # redondean cada click a su cuadro y alternan intervalos (en «Cosmic Consciousness» 480 y
    # 460 ms): la mediana daba 125 BPM y el tempo real es 126.
    k = np.arange(len(clicks))
    periodo, inicio = np.polyfit(k, clicks, 1)
    return 60 / periodo, inicio * 1000


def main():
    nombre = 'cancion'
    if '--nombre' in sys.argv:                       # p. ej. --nombre cancion2 para una segunda canción en la tarjeta
        i = sys.argv.index('--nombre'); nombre = sys.argv[i + 1]; del sys.argv[i:i + 2]
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    origen, destino = sys.argv[1], sys.argv[2]
    titulo = sys.argv[3] if len(sys.argv) > 3 else ''
    artista = sys.argv[4] if len(sys.argv) > 4 else ''
    os.makedirs(destino, exist_ok=True)

    entradas = []
    for pista, clave in ORDEN:
        r = buscar(origen, clave)
        if not r:
            sys.exit(f'No encontré la pista «{clave}» en {origen}')
        entradas += ['-i', r]
        print(f'{pista:8s} <- {os.path.basename(r)}')

    filtro = ';'.join(f'[{i}:a]aresample=44100,pan=mono|c0=0.5*c0+0.5*c1[a{i}]' for i in range(4))
    # El orden de los canales va FIJADO a mano. Sin el «map», join toma cada pista mono como canal
    # central (FC) y deja la primera ahí: el archivo salía bajo, voz, batería, otros, y cada perilla
    # movía otro instrumento (pasó con las dos primeras canciones).
    filtro += ';[a0][a1][a2][a3]join=inputs=4:channel_layout=4.0:map=0.0-FL|1.0-FR|2.0-FC|3.0-BC[out]'
    wav = os.path.join(destino, nombre + '.wav')
    subprocess.run(['ffmpeg', '-v', 'error', '-y', *entradas, '-filter_complex', filtro, '-map', '[out]',
                    '-c:a', 'pcm_s16le', '-ar', '44100', wav], check=True)

    bpm, primero = 120.0, 0.0
    metro = buscar(origen, 'metronome')
    if metro:
        bpm, primero = medir_metronomo(metro)
        print(f'metrónomo: {bpm:.2f} BPM, primer pulso a {primero:.0f} ms')
    else:
        print('sin metrónomo: deja bpm y primer_pulso_ms a mano en cancion.txt')

    with open(os.path.join(destino, nombre + '.txt'), 'w', encoding='utf-8') as f:
        f.write('# Resonancia · mezclador_pistas — datos de la canción\n')
        f.write(f'titulo={titulo}\nartista={artista}\nbpm={bpm:.2f}\nprimer_pulso_ms={primero:.0f}\n')
        for i, (pista, _) in enumerate(ORDEN):
            f.write(f'pista{i + 1}={pista}\n')
    print(f'Listo: {wav} ({os.path.getsize(wav) / 1e6:.0f} MB) y {nombre}.txt')


if __name__ == '__main__':
    main()
