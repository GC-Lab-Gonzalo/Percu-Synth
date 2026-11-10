"""
servidor.py — servidor local del simulador del PercuSynth.

    python servidor.py            (abre http://localhost:8794)

Sirve la interfaz y compila los sketches con Emscripten (ver compilar.py).
Manda las cabeceras COOP/COEP que el navegador exige para usar memoria
compartida entre hilos (SharedArrayBuffer): sin ellas la placa no arranca.

API:
  GET  /api/firmwares            los firmwares de firmwares/ que compilan en el simulador, del más nuevo al más antiguo
  POST /api/compilar             {"carpeta": "bajo_8_pasos"}  o  {"archivos": {"x.ino": "..."}}
                                 -> {"ok", "id", "log", "errores", "principal", "guia"}
                                 "guia" es lo que muestra la ventana «cómo se usa» (ver guia.py)
  POST /api/placa                el mismo pedido -> compila para el ESP32-S3 real con arduino-cli (ver placa.py)
                                 -> {"ok", "id", "bytes", "log", "errores"}; la imagen queda en /build/_placa/<id>.bin
"""
import json
import mimetypes
import os
import re
import subprocess
import sys
import threading
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer

import compilar
import guia
import placa

_candado_placa = threading.Lock()
AQUI = os.path.dirname(os.path.abspath(__file__))
FIRMWARES = os.path.normpath(os.path.join(AQUI, '..', '..', 'firmwares'))
PUERTO = int(os.environ.get('PUERTO', '8794'))

mimetypes.add_type('application/wasm', '.wasm')
mimetypes.add_type('text/javascript', '.js')


def carpetas():
    """Todas las carpetas de firmwares/ que tienen su .ino."""
    return [d for d in os.listdir(FIRMWARES)
            if os.path.isdir(os.path.join(FIRMWARES, d)) and os.path.exists(os.path.join(FIRMWARES, d, d + '.ino'))]


# Lo que la placa virtual sabe compilar: sus cabeceras (hal/include) y las de C/C++ de Emscripten.
SYSROOT = os.path.join(compilar.EMSDK, 'upstream', 'emscripten', 'cache', 'sysroot', 'include')
RUTAS_INCLUDE = [compilar.HAL_INC, SYSROOT, os.path.join(SYSROOT, 'c++', 'v1')]


def lo_que_falta(carpeta):
    """Las librerías que el sketch incluye y la placa virtual no tiene (WiFi.h, MPU6050.h, ESP32Synth.h…).
    Si falta alguna, el código no compila en el simulador y no se ofrece en la lista."""
    falta = set()
    locales = set(os.listdir(carpeta))
    for f in locales:
        if not f.lower().endswith(('.ino', '.h', '.hpp', '.cpp', '.c')):
            continue
        with open(os.path.join(carpeta, f), encoding='utf-8', errors='replace') as fh:
            src = re.sub(r'/\*.*?\*/', '', fh.read(), flags=re.S)
        for inc in re.findall(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', src, re.M):
            if inc in locales or any(os.path.exists(os.path.join(r, inc)) for r in RUTAS_INCLUDE):
                continue
            falta.add(inc)
    return sorted(falta)


def _edades():
    """Cuándo nació cada firmware: lo más antiguo entre su primer commit en git y la creación del .ino en el disco
    (un archivo vuelto a guardar no se hace más nuevo; uno sin commitear todavía cuenta por su creación)."""
    git = {}
    try:
        salida = subprocess.run(['git', 'log', '--diff-filter=A', '--format=@%at', '--name-only', '--', '.'],
                                cwd=FIRMWARES, capture_output=True, text=True, timeout=20).stdout
        t = None
        for l in salida.splitlines():
            if l.startswith('@'):
                t = int(l[1:])
            elif l.strip() and t:
                partes = l.strip().split('/')
                if len(partes) >= 2 and partes[-1] == partes[-2] + '.ino':
                    git[partes[-2]] = min(t, git.get(partes[-2], t))
    except Exception:
        pass
    edad = {}
    for d in carpetas():
        st = os.stat(os.path.join(FIRMWARES, d, d + '.ino'))
        nacio = getattr(st, 'st_birthtime', st.st_ctime if os.name == 'nt' else st.st_mtime)
        edad[d] = min(nacio, git.get(d, nacio))
    return edad


# Compilan, pero no se ofrecen en el simulador (decisión de Gonzalo, 2026-10-23 y 2026-11-08). Se pueden abrir igual con «abrir .ino».
OCULTOS = {'lanzador_canciones', 'aves_samples', 'reel_taller', 'terror', 'lampara_lava',
           'oled_clip', 'oled_imagen'}
# Más nombres que esconder, uno por línea, en ocultos_local.txt (no va al repo).
try:
    with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'ocultos_local.txt'), encoding='utf-8') as _fh:
        OCULTOS |= {l.strip() for l in _fh if l.strip() and not l.startswith('#')}
except OSError:
    pass


def lista_firmwares():
    """Los firmwares que corren en el simulador, del más nuevo al más antiguo."""
    edad = _edades()
    ok = [d for d in carpetas() if d not in OCULTOS and not lo_que_falta(os.path.join(FIRMWARES, d))]
    return sorted(ok, key=lambda d: -edad.get(d, 0))


def guia_de(archivos, principal, carpeta):
    """La guía de uso del sketch: su encabezado y, si es del repo, lo que falte desde su README."""
    try:
        if principal not in archivos:
            principal = next((k for k in sorted(archivos) if k.endswith('.ino')), None)
        if not principal:
            return None
        g = guia.guia(archivos[principal], os.path.splitext(principal)[0])
        md = os.path.join(carpeta, 'README.md') if carpeta else ''
        if md and os.path.exists(md):
            with open(md, encoding='utf-8', errors='replace') as f:
                guia.completar_con_readme(g, f.read())
        return g
    except Exception as e:  # una guía rota no puede tumbar la compilación
        sys.stderr.write(f'guia: {e}\n')
        return None


class Manejador(SimpleHTTPRequestHandler):
    def __init__(self, *a, **kw):
        super().__init__(*a, directory=AQUI, **kw)

    def end_headers(self):
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        self.send_header('Cross-Origin-Embedder-Policy', 'credentialless')
        self.send_header('Cross-Origin-Resource-Policy', 'same-origin')
        if self.path.endswith(('.html', '.js')) and '/build/' not in self.path:
            self.send_header('Cache-Control', 'no-store')
        super().end_headers()

    def log_message(self, fmt, *args):
        if '/api/' in (args[0] if args else ''):
            sys.stderr.write('%s\n' % (fmt % args))

    def _json(self, obj, code=200):
        cuerpo = json.dumps(obj).encode('utf-8')
        self.send_response(code)
        self.send_header('Content-Type', 'application/json; charset=utf-8')
        self.send_header('Content-Length', str(len(cuerpo)))
        self.end_headers()
        self.wfile.write(cuerpo)

    def do_GET(self):
        if self.path.startswith('/api/firmwares'):
            return self._json({'firmwares': lista_firmwares()})
        return super().do_GET()

    def do_POST(self):
        es_placa = self.path.startswith('/api/placa')
        if not es_placa and not self.path.startswith('/api/compilar'):
            return self._json({'error': 'no existe'}, 404)
        n = int(self.headers.get('Content-Length', '0'))
        try:
            pedido = json.loads(self.rfile.read(n).decode('utf-8'))
        except Exception:
            return self._json({'ok': False, 'log': 'Pedido inválido.', 'errores': []}, 400)
        carpeta = None
        if 'carpeta' in pedido:
            nombre = os.path.basename(pedido['carpeta'])
            carpeta = os.path.join(FIRMWARES, nombre)
            if nombre not in carpetas():
                return self._json({'ok': False, 'log': f'No existe firmwares/{nombre}.', 'errores': []})
            archivos, principal = compilar.leer_carpeta(carpeta)
        else:
            archivos = {os.path.basename(k): v for k, v in (pedido.get('archivos') or {}).items()}
            principal = pedido.get('principal')
            if principal not in archivos:
                principal = None
        try:
            if es_placa:
                with _candado_placa:
                    return self._json(placa.compilar_placa(archivos, principal))
            r = compilar.compilar(archivos, principal)
        except Exception as e:  # el error tiene que llegar a la pantalla, no morir acá
            r = {'ok': False, 'log': f'Error interno del compilador: {e}', 'errores': []}
        r['guia'] = guia_de(archivos, r.get('principal') or principal, carpeta)
        return self._json(r)


if __name__ == '__main__':
    if not os.path.exists(compilar.EMPP):
        print('No encuentro Emscripten en', compilar.EMSDK)
        print('Instálalo con: git clone https://github.com/emscripten-core/emsdk ~/emsdk && emsdk install latest && emsdk activate latest')
        sys.exit(1)
    srv = ThreadingHTTPServer(('127.0.0.1', PUERTO), Manejador)
    print(f'Simulador del PercuSynth en http://localhost:{PUERTO}')
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass
