"""
exportar_web.py — arma la versión web del simulador, sin servidor, para subirla a la web de GC Lab.

    python exportar_web.py [carpeta_destino]

Por defecto escribe en  GC Lab Chile/web/public_html/simulador/  (la copia local de la web).
En un hosting compartido no hay Emscripten, así que no se puede compilar ahí: este script compila
los mismos firmwares que ofrece el selector (servidor.lista_firmwares(): los que corren, del más
nuevo al más antiguo) y deja

    index.html, motor.html, img/       la página, tal cual
    build/<id>/sim.js + sim.wasm       cada firmware compilado
    placa/<id>.bin                     cada firmware compilado para el ESP32-S3 real («cargar a la placa»)
    esptool/                           esptool-js, que escribe esa imagen por Web Serial
    firmwares.json                     la lista con su build, su imagen, sus periféricos y su guía «cómo se usa»
    .htaccess                          las cabeceras COOP/COEP que la placa necesita (SharedArrayBuffer)

index.html encuentra firmwares.json y entra en modo web: carga los firmwares ya compilados y no
muestra «abrir .ino» (un código propio sólo corre con servidor.py).
"""
import json
import os
import shutil
import sys

import compilar
import placa
import servidor

AQUI = os.path.dirname(os.path.abspath(__file__))
DESTINO = sys.argv[1] if len(sys.argv) > 1 else os.path.normpath(
    os.path.join(AQUI, '..', '..', '..', 'web', 'public_html', 'simulador'))

HTACCESS = """# Simulador del PercuSynth (generado por percusynth/tools/simulador/exportar_web.py)
# La placa corre en varios hilos que comparten memoria (SharedArrayBuffer): el navegador sólo lo
# permite si la página está aislada con estas cabeceras. require-corp (y no credentialless) para Safari.
<IfModule mod_headers.c>
    Header always set Cross-Origin-Opener-Policy "same-origin"
    Header always set Cross-Origin-Embedder-Policy "require-corp"
    Header always set Cross-Origin-Resource-Policy "same-origin"
    <FilesMatch "\\.(html|json)$">
        Header set Cache-Control "no-cache"
    </FilesMatch>
</IfModule>
AddType application/wasm .wasm
"""


def main():
    nombres = servidor.lista_firmwares()
    lista = []
    for n in nombres:
        carpeta = os.path.join(servidor.FIRMWARES, n)
        archivos, principal = compilar.leer_carpeta(carpeta)
        r = compilar.compilar(archivos, principal)
        if not r['ok']:
            print('NO COMPILA, queda fuera:', n)
            continue
        p = placa.compilar_placa(archivos, r.get('principal') or principal)
        if not p['ok']:
            print('NO COMPILA PARA LA PLACA (queda sin «cargar a la placa»):', n)
        lista.append({'nombre': n, 'id': r['id'], 'principal': r.get('principal') or principal,
                      'placa': p['id'] if p['ok'] else None,
                      'perifericos': r.get('perifericos') or {},
                      'guia': servidor.guia_de(archivos, r.get('principal') or principal, carpeta)})
        print('ok', n, r['id'], p.get('id'))

    if os.path.isdir(DESTINO):
        shutil.rmtree(DESTINO)
    os.makedirs(os.path.join(DESTINO, 'build'))
    for f in ('index.html', 'motor.html'):
        shutil.copy2(os.path.join(AQUI, f), DESTINO)
    shutil.copytree(os.path.join(AQUI, 'img'), os.path.join(DESTINO, 'img'))
    shutil.copytree(os.path.join(AQUI, 'esptool'), os.path.join(DESTINO, 'esptool'))
    os.makedirs(os.path.join(DESTINO, 'placa'))
    for f in lista:
        if f['placa']:
            shutil.copy2(os.path.join(placa.DESTINO, f['placa'] + '.bin'), os.path.join(DESTINO, 'placa'))
        origen = os.path.join(AQUI, 'build', f['id'])
        destino = os.path.join(DESTINO, 'build', f['id'])
        os.makedirs(destino)
        for a in ('sim.js', 'sim.wasm'):
            shutil.copy2(os.path.join(origen, a), destino)
    with open(os.path.join(DESTINO, 'firmwares.json'), 'w', encoding='utf-8') as fh:
        json.dump({'firmwares': lista}, fh, ensure_ascii=False, separators=(',', ':'))
    with open(os.path.join(DESTINO, '.htaccess'), 'w', encoding='utf-8', newline='\n') as fh:
        fh.write(HTACCESS)
    total = sum(os.path.getsize(os.path.join(d, a)) for d, _, fs in os.walk(DESTINO) for a in fs)
    print(f'\n{len(lista)} firmwares en {DESTINO} ({total / 1048576:.1f} MB)')


if __name__ == '__main__':
    main()
