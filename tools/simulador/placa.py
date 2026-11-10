"""
placa.py — compila un sketch para la placa de verdad (ESP32-S3) y deja la imagen lista para cargarla desde el navegador.

    python placa.py ../../firmwares/bajo_8_pasos      compila uno
    python placa.py --todos                           compila todos los que ofrece el simulador

El simulador compila para WebAssembly (compilar.py); esto es lo otro: arduino-cli con los ajustes que el encabezado
del .ino le pide a Arduino IDE. El resultado es la imagen completa (bootloader + particiones + programa, offset 0)
sin el relleno de 0xFF del final, en build/_placa/<id>.bin. El navegador la escribe con esptool-js.
"""
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

import compilar

DESTINO = os.path.join(compilar.BUILD, '_placa')
CLI = shutil.which('arduino-cli') or r'C:\Program Files\Arduino CLI\arduino-cli.exe'
VERSION = '1'
RE_GRANDE = re.compile(r'too big|exceeds available space', re.I)


def ajustes(archivos, principal):
    """Lo que en Arduino IDE se elige en Herramientas, leído del encabezado del .ino."""
    cab = archivos[principal][:8000]
    todo = '\n'.join(v for k, v in archivos.items() if k.endswith(('.ino', '.h', '.cpp')) and len(v) < 2_000_000)
    tinyusb = re.search(r'USB Mode\s*:\s*USB-OTG', cab) or re.search(r'#\s*include\s*[<"](USBMIDI|Adafruit_TinyUSB)\.h', todo)
    return {
        'CDCOnBoot': 'default' if re.search(r'USB CDC On Boot\s*:\s*Disabled', cab) else 'cdc',
        'FlashMode': 'dio',                                 # OPI rompe el I2S en esta placa
        'PSRAM': 'disabled' if re.search(r'PSRAM\s*:\s*Disabled', cab) else 'opi',
        'USBMode': 'default' if tinyusb else 'hwcdc',
        'PartitionScheme': 'huge_app' if re.search(r'Partition Scheme\s*:\s*\**\s*Huge', cab) else 'default',
    }


def fqbn(a):
    return 'esp32:esp32:esp32s3:' + ','.join(f'{k}={v}' for k, v in a.items())


def recortar(datos):
    """Saca el relleno de 0xFF del final, dejando la imagen alineada a un sector de 4 KB."""
    fin = len(datos.rstrip(b'\xff'))
    return datos[:(fin + 0xFFF) & ~0xFFF]


def _arduino(sketch, trabajo, a):
    salida = os.path.join(trabajo, 'salida')
    cmd = [CLI, 'compile', '--fqbn', fqbn(a), '--build-path', os.path.join(trabajo, 'build'), '--output-dir', salida,
           '--jobs', '3', sketch]
    p = subprocess.run(cmd, capture_output=True, text=True, encoding='utf-8', errors='replace')
    return p.returncode, (p.stdout or '') + (p.stderr or ''), salida


def compilar_placa(archivos, principal=None):
    """archivos: dict nombre -> texto. Devuelve dict con ok, id, bytes, fqbn, log, errores."""
    if principal is None:
        inos = sorted(k for k in archivos if k.endswith('.ino'))
        if not inos:
            return {'ok': False, 'log': 'No hay ningún archivo .ino.', 'errores': []}
        principal = inos[0]
    a = ajustes(archivos, principal)
    h = hashlib.sha1((VERSION + fqbn(a)).encode())
    for k in sorted(archivos):
        h.update(k.encode()); h.update(archivos[k].encode('utf-8', 'replace'))
    pid = h.hexdigest()[:16]
    final = os.path.join(DESTINO, pid + '.bin')
    if os.path.exists(final):
        return {'ok': True, 'id': pid, 'bytes': os.path.getsize(final), 'log': '(ya compilado)', 'errores': [], 'principal': principal}

    trabajo = tempfile.mkdtemp(prefix='percusynth_')       # fuera de OneDrive: son miles de archivos pasajeros
    try:
        nombre = os.path.splitext(principal)[0]
        sketch = os.path.join(trabajo, nombre)
        os.makedirs(sketch)
        for k, v in archivos.items():
            if '/' in k or '\\' in k or k.startswith('.'):
                continue
            with open(os.path.join(sketch, k), 'w', encoding='utf-8', newline='\n') as fh:
                fh.write(v)
        rc, log, salida = _arduino(sketch, trabajo, a)
        if rc != 0 and RE_GRANDE.search(log) and a['PartitionScheme'] != 'huge_app':
            a = {**a, 'PartitionScheme': 'huge_app'}       # no cabe en la partición normal: la de 3 MB
            rc, log, salida = _arduino(sketch, trabajo, a)
        log = log.replace(sketch + os.sep, '')
        errores = [dict(archivo=os.path.basename(m.group('archivo')), linea=int(m.group('linea')), col=int(m.group('col')),
                        tipo=m.group('tipo'), msg=m.group('msg')) for m in compilar.RE_ERR.finditer(log)]
        if rc != 0:
            return {'ok': False, 'log': log, 'errores': errores, 'principal': principal}
        with open(os.path.join(salida, principal + '.merged.bin'), 'rb') as fh:
            imagen = recortar(fh.read())
        if imagen[:1] != b'\xe9':
            return {'ok': False, 'log': 'La imagen compilada no parte con un bootloader válido.', 'errores': [], 'principal': principal}
        os.makedirs(DESTINO, exist_ok=True)
        with open(final + '.tmp', 'wb') as fh:
            fh.write(imagen)
        os.replace(final + '.tmp', final)
        return {'ok': True, 'id': pid, 'bytes': len(imagen), 'log': log, 'errores': errores, 'principal': principal, 'fqbn': fqbn(a)}
    finally:
        shutil.rmtree(trabajo, ignore_errors=True)


def _todos():
    import servidor
    pendientes = []
    for n in servidor.lista_firmwares():
        archivos, principal = compilar.leer_carpeta(os.path.join(servidor.FIRMWARES, n))
        pendientes.append((n, archivos, principal))

    def uno(t):
        n, archivos, principal = t
        try:
            r = compilar_placa(archivos, principal)
        except Exception as e:
            r = {'ok': False, 'log': str(e)}
        if r['ok']:
            print(f"ok {n} {r['id']} {r['bytes'] / 1048576:.2f} MB", flush=True)
        else:
            print(f"FALLA {n}\n{r['log'][-1500:]}", flush=True)
        return r['ok']

    # el primero de cada combinación de ajustes va solo: arma el caché del núcleo, que no aguanta dos a la vez
    vistos, primeros, resto = set(), [], []
    for t in pendientes:
        clave = fqbn(ajustes(t[1], t[2]))
        (resto if clave in vistos else primeros).append(t)
        vistos.add(clave)
    ok = [uno(t) for t in primeros]
    with ThreadPoolExecutor(4) as ex:
        ok += list(ex.map(uno, resto))
    print(f'\n{sum(ok)} de {len(ok)} compilados')


if __name__ == '__main__':
    if sys.argv[1] == '--todos':
        _todos()
    else:
        archivos, principal = compilar.leer_carpeta(sys.argv[1])
        r = compilar_placa(archivos, principal)
        print(r['log'][-3000:])
        print('OK' if r['ok'] else 'FALLÓ', r.get('id', ''), r.get('bytes', ''))
