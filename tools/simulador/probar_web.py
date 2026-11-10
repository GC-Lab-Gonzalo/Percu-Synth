"""
probar_web.py — sirve la exportación de exportar_web.py como la serviría Hostinger, para probarla antes de subirla.

    python probar_web.py [carpeta] [puerto]      (por defecto web/public_html/simulador y 8797)

Sólo archivos estáticos (no hay /api) con las cabeceras de su .htaccess: COOP same-origin y COEP require-corp.
"""
import os
import sys
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer

AQUI = os.path.dirname(os.path.abspath(__file__))
CARPETA = sys.argv[1] if len(sys.argv) > 1 else os.path.normpath(
    os.path.join(AQUI, '..', '..', '..', 'web', 'public_html', 'simulador'))
PUERTO = int(sys.argv[2]) if len(sys.argv) > 2 else 8797


class Estatico(SimpleHTTPRequestHandler):
    extensions_map = {**SimpleHTTPRequestHandler.extensions_map, '.wasm': 'application/wasm', '.js': 'text/javascript'}

    def end_headers(self):
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        self.send_header('Cross-Origin-Resource-Policy', 'same-origin')
        super().end_headers()


if __name__ == '__main__':
    print(f'{CARPETA} en http://localhost:{PUERTO}')
    ThreadingHTTPServer(('127.0.0.1', PUERTO), partial(Estatico, directory=CARPETA)).serve_forever()
