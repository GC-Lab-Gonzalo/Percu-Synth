"""
compilar.py — convierte un sketch de Arduino (.ino + .h/.cpp de su carpeta) en un
módulo WebAssembly que corre sobre la placa virtual (hal/).

Hace lo mismo que el preprocesador de Arduino IDE:
  1. junta los .ino de la carpeta (el principal primero),
  2. agrega #include <Arduino.h>,
  3. genera los prototipos de las funciones y los inserta antes de la primera,
  4. conserva la numeración de líneas con #line, así los errores apuntan al .ino.
Después compila con Emscripten (em++) contra hal/include y enlaza con la HAL.

Uso por consola (para probar sin la interfaz):
    python compilar.py ../../firmwares/bajo_8_pasos
"""
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import threading

AQUI = os.path.dirname(os.path.abspath(__file__))
HAL_INC = os.path.join(AQUI, 'hal', 'include')
HAL_SRC = os.path.join(AQUI, 'hal', 'src')
BUILD = os.path.join(AQUI, 'build')
EMSDK = os.environ.get('EMSDK') or os.path.join(os.path.expanduser('~'), 'emsdk')
EMPP = os.path.join(EMSDK, 'upstream', 'emscripten', 'em++.exe' if os.name == 'nt' else 'em++')
if not os.path.exists(EMPP):
    EMPP = os.path.join(EMSDK, 'upstream', 'emscripten', 'em++')

VERSION_HAL = '3'
DEFINES = [
    '-DARDUINO=10819', '-DESP32', '-DESP_PLATFORM', '-DARDUINO_ARCH_ESP32', '-DARDUINO_ESP32S3_DEV',
    '-DCONFIG_IDF_TARGET_ESP32S3=1', '-DARDUINO_USB_CDC_ON_BOOT=1', '-DARDUINO_USB_MODE=0',
    '-DBOARD_HAS_PSRAM', '-DESP_ARDUINO_VERSION_MAJOR=3', '-DESP_ARDUINO_VERSION_MINOR=0',
    '-DF_CPU=240000000L', '-DPERCUSYNTH_SIMULADOR=1',
]
CFLAGS = ['-O2', '-std=gnu++17', '-pthread', '-w', '-fno-exceptions', '-I', HAL_INC] + DEFINES
LDFLAGS = [
    '-pthread', '-sPROXY_TO_PTHREAD=1', '-sPTHREAD_POOL_SIZE=10', '-sINITIAL_MEMORY=268435456',
    '-sSTACK_SIZE=1048576', '-sDEFAULT_PTHREAD_STACK_SIZE=262144', '-sMODULARIZE=1',
    '-sEXPORT_NAME=crearPlaca', '-sENVIRONMENT=web,worker', '-sEXIT_RUNTIME=0',
    '-sEXPORTED_FUNCTIONS=_main,_sim_layout,_sim_sd_lista,_sim_sd_copia,_sim_sd_largo,_sim_sd_reservar,_sim_sd_poner,_sim_sd_borrar',
    '-sEXPORTED_RUNTIME_METHODS=UTF8ToString,HEAPU8,ccall',
    '-sALLOW_BLOCKING_ON_MAIN_THREAD=0',
]

_candado = threading.Lock()

# ------------------------------------------------------------------ prototipos
PALABRAS_NO = {'if', 'for', 'while', 'switch', 'catch', 'return', 'sizeof', 'else', 'do', 'case', 'defined'}


def _limpiar(src):
    """Copia del fuente con comentarios, literales y líneas de preprocesador en blanco
    (mismo largo y mismos saltos de línea, para poder mapear posiciones)."""
    out = list(src)
    i, n = 0, len(src)
    inicio_linea = True
    while i < n:
        c = src[i]
        if inicio_linea and c in ' \t':
            i += 1
            continue
        if inicio_linea and c == '#':
            # línea de preprocesador (con continuaciones)
            while i < n:
                if src[i] == '\\' and i + 1 < n and src[i + 1] == '\n':
                    out[i] = ' '; i += 2; continue
                if src[i] == '\n':
                    break
                out[i] = ' '; i += 1
            continue
        inicio_linea = False
        if c == '\n':
            inicio_linea = True; i += 1; continue
        if src.startswith('//', i):
            while i < n and src[i] != '\n':
                out[i] = ' '; i += 1
            continue
        if src.startswith('/*', i):
            j = src.find('*/', i + 2)
            j = n if j < 0 else j + 2
            for k in range(i, j):
                if src[k] != '\n': out[k] = ' '
            i = j; continue
        if c == 'R' and src.startswith('R"', i):
            m = re.match(r'R"([^(\s]*)\(', src[i:])
            if m:
                fin = ')' + m.group(1) + '"'
                j = src.find(fin, i + m.end())
                j = n if j < 0 else j + len(fin)
                for k in range(i, j):
                    if src[k] != '\n': out[k] = ' '
                i = j; continue
        if c in '"\'':
            q = c; j = i + 1
            while j < n and src[j] != q and src[j] != '\n':
                j += 2 if src[j] == '\\' else 1
            for k in range(i + 1, min(j, n)):
                out[k] = ' '
            i = j + 1; continue
        i += 1
    return ''.join(out)


def _partir_args(args):
    partes, prof, cur = [], 0, ''
    for ch in args:
        if ch in '(<[{': prof += 1
        elif ch in ')>]}': prof -= 1
        if ch == ',' and prof == 0:
            partes.append(cur); cur = ''
        else:
            cur += ch
    if cur.strip():
        partes.append(cur)
    return partes


def _sin_defaults(args):
    res = []
    for a in _partir_args(args):
        prof, corte = 0, None
        for k, ch in enumerate(a):
            if ch in '(<[{': prof += 1
            elif ch in ')>]}': prof -= 1
            elif ch == '=' and prof == 0:
                corte = k; break
        res.append((a[:corte] if corte is not None else a).strip())
    return ', '.join(res)


RE_FUNC = re.compile(r'^(?P<pre>[\w\s\*&:<>,]*?[\w\*&>])\s*\b(?P<nombre>[A-Za-z_]\w*)\s*\((?P<args>.*)\)\s*(const\s*)?$', re.S)


def generar_prototipos(src):
    """Devuelve (posición de inserción, [prototipos]) para un fuente .ino ya unido."""
    limpio = _limpiar(src)
    funciones = []
    prof, frontera, primera = 0, 0, None
    i, n = 0, len(limpio)
    while i < n:
        c = limpio[i]
        if c == '{':
            if prof == 0:
                cab = limpio[frontera:i]
                cab_s = ' '.join(cab.split())
                m = RE_FUNC.match(cab_s)
                if (m and '=' not in cab_s.split('(')[0] and m.group('nombre') not in PALABRAS_NO
                        and not re.match(r'^(struct|class|union|enum|namespace|typedef|template|extern)\b', m.group('pre'))
                        and not m.group('pre').rstrip().endswith('::') and 'operator' not in cab_s):
                    pre = m.group('pre').strip()
                    if not re.fullmatch(r'(return|else|new|delete)', pre):
                        funciones.append((frontera, pre, m.group('nombre'), m.group('args')))
                        if primera is None:
                            primera = frontera
            prof += 1
        elif c == '}':
            prof -= 1
            if prof == 0:
                frontera = i + 1
        elif c == ';' and prof == 0:
            frontera = i + 1
        i += 1
    if primera is None:
        return 0, []
    # tipos declarados DESPUÉS del punto de inserción: un prototipo que los nombre no compilaría
    tardios = set()
    for m in re.finditer(r'\b(?:struct|class|union|enum(?:\s+class)?)\s+([A-Za-z_]\w*)', limpio):
        if m.start() > primera:
            tardios.add(m.group(1))
    for m in re.finditer(r'\btypedef\b[^;]*?\b([A-Za-z_]\w*)\s*;', limpio):
        if m.start() > primera:
            tardios.add(m.group(1))
    for m in re.finditer(r'\busing\s+([A-Za-z_]\w*)\s*=', limpio):
        if m.start() > primera:
            tardios.add(m.group(1))
    temprano = set()
    for m in re.finditer(r'\b(?:struct|class|union|enum(?:\s+class)?)\s+([A-Za-z_]\w*)', limpio[:primera]):
        temprano.add(m.group(1))
    tardios -= temprano
    protos, vistos = [], set()
    for (_, pre, nombre, args) in funciones:
        args_ok = _sin_defaults(args)
        texto = f'{pre} {nombre}({args_ok});'
        ids = set(re.findall(r'[A-Za-z_]\w*', pre + ' ' + args_ok))
        if ids & tardios:
            continue
        if texto in vistos:
            continue
        vistos.add(texto)
        protos.append(texto)
    # insertar al comienzo de la línea donde empieza la cabecera de la primera función
    k = primera
    while k < len(limpio) and limpio[k] in ' \t\r\n':
        k += 1
    lin = src.rfind('\n', 0, k) + 1
    if src[lin:k].strip():          # hay otro código antes en esa misma línea
        lin = k
    return lin, protos


def preprocesar(carpeta_archivos, principal):
    """carpeta_archivos: dict nombre -> texto. Devuelve el .cpp listo para compilar."""
    inos = [principal] + sorted(k for k in carpeta_archivos if k.endswith('.ino') and k != principal)
    partes = []
    for nombre in inos:
        texto = carpeta_archivos[nombre].replace('\r\n', '\n')
        partes.append(f'#line 1 "{nombre}"\n{texto}\n')
    unido = ''.join(partes)
    pos, protos = generar_prototipos(unido)
    linea = unido.count('\n', 0, pos) + 1
    # #line apunta al archivo .ino en el que cae la inserción
    archivo, base = inos[0], 0
    for m in re.finditer(r'^#line 1 "([^"]+)"\n', unido, re.M):
        if m.start() <= pos:
            archivo, base = m.group(1), unido.count('\n', 0, m.end())
    linea_ino = linea - base
    bloque = ''
    if protos:
        bloque = '\n'.join(protos) + f'\n#line {linea_ino} "{archivo}"\n'
    return '#include <Arduino.h>\n' + unido[:pos] + bloque + unido[pos:]


# ------------------------------------------------------------------ compilación
def _hash_hal():
    h = hashlib.sha1(VERSION_HAL.encode())
    h.update(' '.join(CFLAGS).encode())
    for raiz in (HAL_INC, HAL_SRC):
        for d, _, fs in sorted(os.walk(raiz)):
            for f in sorted(fs):
                p = os.path.join(d, f)
                h.update(p.encode())
                with open(p, 'rb') as fh:
                    h.update(fh.read())
    return h.hexdigest()[:16]


def _correr(cmd, cwd):
    p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, encoding='utf-8', errors='replace')
    return p.returncode, (p.stdout or '') + (p.stderr or '')


def _objetos_hal(log):
    clave = _hash_hal()
    dest = os.path.join(BUILD, '_hal', clave)
    listos = os.path.join(dest, 'ok')
    srcs = sorted(f for f in os.listdir(HAL_SRC) if f.endswith('.cpp'))
    objs = [os.path.join(dest, f[:-4] + '.o') for f in srcs]
    if os.path.exists(listos):
        return objs
    os.makedirs(dest, exist_ok=True)
    for f, o in zip(srcs, objs):
        rc, salida = _correr([EMPP] + CFLAGS + ['-c', os.path.join(HAL_SRC, f), '-o', o], AQUI)
        if rc != 0:
            log.append(f'[placa virtual] error compilando {f}:\n{salida}')
            raise RuntimeError('hal')
    open(listos, 'w').close()
    return objs


def perifericos(archivos):
    """Lo que el sketch espera tener enchufado, para que la interfaz lo ponga sola:
    pantalla OLED o IMU en el header, microSD, micrófono y sensores en EXT1 (GPIO 3) / EXT2 (GPIO 9)."""
    txt = '\n'.join(_sin_comentarios(v) for k, v in archivos.items() if k.endswith(('.ino', '.h', '.cpp')))
    ino = '\n'.join(_sin_comentarios(v) for k, v in archivos.items() if k.endswith('.ino'))
    p = {
        'oled': bool(re.search(r'\b0x3[cC]\b|SSD1306', txt)),
        # 0x68 solo no basta (aparece en fuentes e imágenes): hace falta también el registro 0x3B
        # (ACCEL_XOUT_H), y sólo se busca en el .ino (los .h de datos están llenos de bytes así)
        'imu': bool(re.search(r'MPU6050|MPU_ADDR', txt) or (re.search(r'\b0x68\b', ino) and re.search(r'\b0x3[bB]\b', ino))),
        'sd': bool(re.search(r'#\s*include\s*[<"]SD\.h', txt)),
        'mic': 'i2s_channel_read' in txt,
        'ext': {},
    }
    # "#define FSR_PIN 3", "const int LDR_PIN = 9"…
    for m in re.finditer(r'\b(\w*(FSR|LDR|DHT)\w*)\s*=?\s*(3|9)\b', txt):
        if 'PIN' in m.group(1).upper():
            p['ext'][m.group(3)] = m.group(2).lower()
    # "DHT dht(3, DHT11)" o "dht.setup(9, DHTesp::DHT22)"
    for m in re.finditer(r'\bDHT\s+\w+\s*\(\s*(3|9)\s*,|\.setup\s*\(\s*(3|9)\s*,\s*DHTesp', txt):
        p['ext'][m.group(1) or m.group(2)] = 'dht'
    if 'dht' in p['ext'].values():
        p['dht22'] = bool(re.search(r'\bDHT22\b|\bAM2302\b|\bDHT21\b', txt))
    return p


def _sin_comentarios(src):
    src = re.sub(r'/\*.*?\*/', ' ', src, flags=re.S)
    return re.sub(r'//[^\n]*', ' ', src)


RE_ERR = re.compile(r'^(?P<archivo>[^:\n]+?):(?P<linea>\d+):(?P<col>\d+): (?P<tipo>error|fatal error|warning): (?P<msg>.*)$', re.M)


def compilar(archivos, principal=None):
    """archivos: dict nombre -> texto. Devuelve dict con ok, id, log, errores."""
    if principal is None:
        inos = sorted(k for k in archivos if k.endswith('.ino'))
        if not inos:
            return {'ok': False, 'log': 'No hay ningún archivo .ino.', 'errores': []}
        principal = inos[0]
    h = hashlib.sha1(_hash_hal().encode())
    h.update(' '.join(LDFLAGS).encode())
    for k in sorted(archivos):
        h.update(k.encode()); h.update(archivos[k].encode('utf-8', 'replace'))
    bid = h.hexdigest()[:16]
    dest = os.path.join(BUILD, bid)
    usa = perifericos(archivos)
    if os.path.exists(os.path.join(dest, 'sim.wasm')):
        return {'ok': True, 'id': bid, 'log': '(ya compilado)', 'errores': [], 'principal': principal, 'perifericos': usa}
    with _candado:
        log = []
        try:
            objs = _objetos_hal(log)
        except RuntimeError:
            return {'ok': False, 'log': '\n'.join(log), 'errores': []}
        trabajo = os.path.join(BUILD, '_tmp_' + bid)
        shutil.rmtree(trabajo, ignore_errors=True)
        os.makedirs(trabajo)
        for k, v in archivos.items():
            if '/' in k or '\\' in k or k.startswith('.'):
                continue
            with open(os.path.join(trabajo, k), 'w', encoding='utf-8', newline='\n') as fh:
                fh.write(v)
        cpp = preprocesar(archivos, principal)
        with open(os.path.join(trabajo, '_sketch.cpp'), 'w', encoding='utf-8', newline='\n') as fh:
            fh.write(cpp)
        extra = [k for k in archivos if k.endswith(('.cpp', '.c'))]
        cmd = [EMPP] + CFLAGS + ['-I', trabajo, '_sketch.cpp'] + extra + objs + LDFLAGS + ['-o', 'sim.js']
        rc, salida = _correr(cmd, trabajo)
        salida = salida.replace(trabajo + os.sep, '').replace(trabajo.replace('\\', '/') + '/', '')
        # la línea con el comando completo de em++ no le sirve a nadie en el taller
        salida = '\n'.join(l for l in salida.split('\n') if not l.startswith('em++: error:'))
        errores = [dict(archivo=os.path.basename(m.group('archivo')), linea=int(m.group('linea')),
                        col=int(m.group('col')), tipo=m.group('tipo'), msg=m.group('msg'))
                   for m in RE_ERR.finditer(salida)]
        if rc != 0:
            shutil.rmtree(trabajo, ignore_errors=True)
            return {'ok': False, 'log': salida, 'errores': errores, 'principal': principal}
        os.makedirs(dest, exist_ok=True)
        for f in os.listdir(trabajo):
            if f.startswith('sim.'):
                shutil.move(os.path.join(trabajo, f), os.path.join(dest, f))
        with open(os.path.join(dest, 'info.json'), 'w', encoding='utf-8') as fh:
            json.dump({'principal': principal}, fh)
        shutil.rmtree(trabajo, ignore_errors=True)
        return {'ok': True, 'id': bid, 'log': salida, 'errores': errores, 'principal': principal, 'perifericos': usa}


def leer_carpeta(carpeta):
    archivos = {}
    for f in os.listdir(carpeta):
        p = os.path.join(carpeta, f)
        if os.path.isfile(p) and f.lower().endswith(('.ino', '.h', '.hpp', '.cpp', '.c')):
            with open(p, 'r', encoding='utf-8', errors='replace') as fh:
                archivos[f] = fh.read()
    nombre = os.path.basename(os.path.normpath(carpeta)) + '.ino'
    return archivos, (nombre if nombre in archivos else None)


if __name__ == '__main__':
    carpeta = sys.argv[1]
    archivos, principal = leer_carpeta(carpeta)
    if '--pre' in sys.argv:
        print(preprocesar(archivos, principal or sorted(k for k in archivos if k.endswith('.ino'))[0]))
        sys.exit(0)
    r = compilar(archivos, principal)
    print(r['log'][-6000:])
    print('OK' if r['ok'] else 'FALLÓ', r.get('id', ''))
