"""
guia.py — la guía de uso de un sketch, leída de su encabezado.

    python guia.py bajo_8_pasos        (imprime la guía de un firmware del repo)

El simulador la muestra en la ventana «cómo se usa»: qué hace el código, qué hace cada
botón y cada perilla, y cómo empezar. No entiende C++: lee los comentarios del encabezado
(formato proto-synth-v2: DESCRIPCIÓN, FUNCIONAMIENTO, MODO DE USO), así que sirve igual para
un firmware del repo que para un .ino recién escrito en el taller. Entiende estas formas:

    - BTN1 (44) → PLAY / STOP. Arranca siempre desde el paso 1.
    - BTN4 → PANEL C : POT1 volumen · POT2 velocidad · POT3 decay
    GENERAL     BTN1 play/stop · BTN2 estilo
                POT1 volumen · POT2 tempo
    - POT1..POT4 = nota de los pasos 1..4      (rango)
    - BTN1/BTN5 → cambia el efecto              (los dos hacen lo mismo)
    - BTN2+BTN4 = entra al menú                 (combinación)
    - IMU → el eje X abre el filtro
"""
import re

# palabras en mayúsculas que son siglas y se quedan así (el resto del texto "gritado" se baja)
SIGLAS = {
    'LED', 'LEDS', 'MIDI', 'IMU', 'FSR', 'LDR', 'OLED', 'BPM', 'DIN', 'USB', 'ADC', 'LFO', 'VCA', 'VCF', 'SVF',
    'DAC', 'SD', 'RGB', 'FX', 'I2S', 'I2C', 'PSRAM', 'WAV', 'MP3', 'DMA', 'CPU', 'GPIO', 'LPF', 'HPF', 'EQ', 'IA',
    'AI', 'GPT', 'TTS', 'STT', 'CC', 'DJ', 'ESP', 'PCM', 'SMD', 'DC', 'AM', 'FM', 'OK', 'PWM', 'ADSR', 'API',
    'SSD', 'RAM', 'RTP', 'NTP', 'JSON', 'WIFI', 'LP', 'HP', 'BP', 'CH', 'GND', 'VCC', 'RMS', 'PPQN', 'SFX',
    'PC', 'IDE', 'OTA', 'OPI', 'SPI', 'SPIFFS', 'APP', 'DAW', 'GM', 'MAC', 'CDC', 'UART', 'ESP32', 'INMP441',
}
ROMANO = re.compile(r'^[IVX]+$')
GRITO = re.compile(r'\b[A-ZÁÉÍÓÚÑÜ][A-ZÁÉÍÓÚÑÜ]+\b')
SEPARADOR = re.compile(r'^\s*[=─━\-_*#]{10,}\s*$')
VIÑETA = r'[-·•*▸►]'
# un control: BTN1, POT 3, IMU
TITULOS = re.compile(r'^(FUNCIONAMIENTO|DESCRIPCI|HARDWARE|LIBRER|ARDUINO|MODO DE USO|CONTROLES)')
TOKEN = re.compile(r'\b(BTN|POT)\s?([1-9])\b|\b(IMU)\b')


def desgritar(t):
    """'PLAY / STOP. Arranca' -> 'play / stop. Arranca' (las siglas y los números romanos se quedan)."""
    def f(m):
        w = m.group(0)
        return w if w in SIGLAS or ROMANO.match(w) else w.lower()
    return GRITO.sub(f, t)


def oraciones(t):
    """Después de bajar el texto gritado: mayúscula al empezar cada oración, y la "Y" / "O" sueltas en minúscula."""
    t = re.sub(r'(?<=[a-záéíóúñ,] )([YO])(?= [a-záéíóúñ])', lambda m: m.group(1).lower(), t)
    t = re.sub(r'([.!?]\s+)([a-záéíóúñ])', lambda m: m.group(1) + m.group(2).upper(), t)
    return mayuscula(t)


def mayuscula(t):
    for i, c in enumerate(t):
        if c.isalpha():
            return t[:i] + c.upper() + t[i + 1:]
    return t


def limpiar(t):
    t = re.sub(r'\s+', ' ', t).strip()
    t = re.sub(r'(?<=\S) ?(?:->|=>) ?(?=\S)', ' → ', t)                         # a -> b  =>  a → b
    t = re.sub(r'^(?:\([^()]{1,12}\)\s*)', '', t)                       # el pin: (44), (ADC1), (MPU6050)
    t = re.sub(r'^(?:GPIO|ADC)\s?\d+\s*(?=→|->|=|:)', '', t)              # GPIO44 → …
    t = re.sub(r'^(?:→|->|=>|=|:|—|–|-)\s*', '', t).strip()
    t = re.sub(r'\s*(?:·|,|;|\||/|\+)\s*$', '', t).strip()
    return t


def corto(t, maximo=110):
    """La primera oración, sin el punto final y sin pasarse de largo."""
    t = re.split(r'(?<=[.!?])\s+(?=[A-ZÁÉÍÓÚÑ¿(«"])', t, maxsplit=1)[0]
    t = re.sub(r'\.$', '', t).strip()
    if len(t) > maximo:
        t = t[:maximo].rsplit(' ', 1)[0].rstrip(',;:·(') + '…'
    return oraciones(desgritar(t))


def encabezado(src):
    """Los comentarios de arriba del archivo, hasta la primera línea de código."""
    lineas, bloque = [], False
    for l in src.splitlines():
        s = l.strip()
        if bloque:
            if '*/' in s:
                bloque = False
                s = s.split('*/', 1)[0]
            lineas.append(re.sub(r'^\s*\*?', '', s).rstrip())
            continue
        if not s:
            lineas.append('')
            continue
        if s.startswith('//'):
            c = l.split('//', 1)[1]
            lineas.append(c[1:] if c.startswith(' ') else c)
            continue
        if s.startswith('/*'):
            bloque = '*/' not in s
            lineas.append(s[2:].split('*/', 1)[0].strip())
            continue
        break
    return [l.rstrip() for l in lineas]


def secciones(lineas):
    """[(título, [líneas])]: un título es una línea entre dos separadores ====, o una de las conocidas
    (FUNCIONAMIENTO, DESCRIPCIÓN…) justo encima de un separador, aunque le falte el de arriba."""
    res = [['', []]]
    sep = lambda k: 0 <= k < len(lineas) and SEPARADOR.match(lineas[k])
    i = 0
    while i < len(lineas):
        l = lineas[i]
        if sep(i):
            i += 1
            continue
        if l.strip() and sep(i + 1) and (sep(i - 1) or TITULOS.match(l.strip())):
            res.append([l.strip(), []])
            i += 2
            continue
        res[-1][1].append(l)
        i += 1
    return res


def parrafos(lineas):
    ps, p = [], []
    for l in lineas + ['']:
        if l.strip():
            p.append(l)
        elif p:
            ps.append(p)
            p = []
    return ps


def titulo(lineas, nombre):
    """'PERCU-SYNTH — BAJO 8 PASOS (secuenciador de bajo…) — GC Lab Chile' -> ('Bajo 8 pasos', 'Secuenciador de bajo…')"""
    for l in lineas:
        s = l.strip()
        if not s or SEPARADOR.match(s):
            continue
        s = re.sub(r'(?i)^percu-?\s?synth\s*[—–·:-]\s*', '', s)
        s = re.sub(r'(?i)\s*[—–-]\s*gc lab( chile)?\s*$', '', s).strip()
        if not s:
            break
        # se corta en lo primero que aparezca: " — ", " · ", ": " o un paréntesis
        m = re.search(r'\s+[—–·]\s+|:\s+|\s*\(', s)
        if not m or m.start() == 0:
            t, sub = s, ''
        elif '(' in m.group(0):
            t, resto = s[:m.start()], s[m.end():]
            sub = resto[:resto.find(')')] if ')' in resto else resto
        else:
            t, sub = s[:m.start()], s[m.end():]
        return mayuscula(desgritar(t.strip())), mayuscula(desgritar(sub.strip()))
    return nombre, ''


def descripcion(secs):
    for t, ls in secs:
        if t.upper().startswith('DESCRIPCI'):
            ps = parrafos(ls)
            if not ps:
                return ''
            texto = ' '.join(l.strip() for l in ps[0])
            texto = re.sub(r'\s+', ' ', texto).strip()
            if texto.endswith(':'):
                texto = texto[:-1].rstrip() + '.'
            # si el primero es muy corto, sigue con el segundo (si no es una lista)
            if len(texto) < 140 and len(ps) > 1 and not re.match(r'\s*' + VIÑETA, ps[1][0]):
                seg = re.sub(r'\s+', ' ', ' '.join(l.strip() for l in ps[1])).strip()
                texto += ' ' + (seg[:-1] + '.' if seg.endswith(':') else seg)
            if len(texto) > 420:
                cortado = texto[:420]
                p = cortado.rfind('. ')
                texto = cortado[:p + 1] if p > 160 else cortado.rsplit(' ', 1)[0] + '…'
            return oraciones(desgritar(texto))
    return ''


def _sangria(l):
    return len(l) - len(l.lstrip())


def _es_inicio(l):
    s = l.strip()
    if re.match(VIÑETA + r'\s', s) or re.match(r'\d+[.)]\s', s):
        return True
    if TOKEN.match(s):
        return True
    # "GENERAL     BTN1 play/stop" · "EN TODOS LOS PANELES:  BTN4 → …" · "EL IMU — un solo eje"
    m = re.match(r'^([A-ZÁÉÍÓÚÑ][A-ZÁÉÍÓÚÑ0-9 /]{1,30}?):?\s+(?=(?:BTN|POT)\s?[1-9]\b)', s)
    if m:
        return True
    return bool(re.match(r'^(?:EL |LA )?(IMU)\b', s))


CTX_RAYA = re.compile(r'^\s*[─━=-]{2,}\s*(.+?)\s*[─━=-]{3,}\s*$')       # ── PANEL A (normal) ─────
CTX_NO = re.compile(r'(?i)\b(pots?|botones|perillas|potenci[oó]metros|controles|paneles|todos|siempre|combos)\b')


def _ctx(t):
    """'PANEL A — MEZCLA (cian) — panel inicial' -> 'Panel A · mezcla'. Vacío si no sirve de contexto."""
    partes = [p.strip() for p in re.split(r'\s+[—–]\s+', t.strip().rstrip(':').strip()) if p.strip()]
    if not partes or CTX_NO.search(partes[0]):
        return ''
    c = partes[0]
    if len(partes) > 1 and not TOKEN.search(partes[1]) and not CTX_NO.search(partes[1]) and len(partes[1]) < 24:
        c += ' · ' + re.sub(r'\s*\([^)]*\)', '', partes[1])
    c = mayuscula(desgritar(c))
    return c if len(c) <= 40 else ''


def _es_cabeza(s):
    """'MODO SEQ  : transporte de performance:' — un título en mayúsculas que abre una lista."""
    return s.endswith(':') and len(s) < 100 and bool(re.match(r'[A-ZÁÉÍÓÚÑ]{2,}\b', s))


def items(bloques):
    """Junta cada viñeta con sus líneas de continuación (las que vienen más adentro) y le anota el
    contexto en que está (un título de panel como '── PANEL B ──' o 'MODO SEQ :' encima de la lista)."""
    res = []
    for lineas in bloques:
        actual, sangria, parrafo, ctx, usado = None, 0, False, '', False

        def cerrar():
            nonlocal actual, usado
            if actual:
                res.append((ctx, actual))
                usado = True
            actual = None
        for l in lineas:
            s = l.strip()
            if not s:
                cerrar()
                continue
            # sigue la viñeta: lo que viene más adentro ("GENERAL  BTN1 …" y debajo "         POT1 …"),
            # o la línea siguiente de un párrafo que empezó nombrando un control ("EL IMU — un solo eje: …")
            if actual is not None and ((_sangria(l) > sangria and not re.match(VIÑETA + r'\s', s))
                                       or (parrafo and not _es_inicio(l) and not _es_cabeza(s))):
                actual.append(l)
                continue
            cerrar()
            m = CTX_RAYA.match(l)
            if m:
                ctx, usado = _ctx(m.group(1)), False
                continue
            if _es_inicio(l):
                actual, sangria = [l], _sangria(l)
                parrafo = not re.match(r'(?:' + VIÑETA + r'|\d+[.)])\s', s)
                continue
            if _es_cabeza(s):                                         # "MODO SEQ  : transporte…:"
                ctx, usado = _ctx(re.split(r'\s*:', s)[0]), False
            elif usado:                                               # un párrafo después de la lista: se acabó ese panel
                ctx, usado = '', False
        cerrar()
    return res


def _grupos(texto):
    """Los controles que empiezan un tramo de texto, unidos cuando van juntos: POT1..POT4, BTN1/BTN5, BTN2+BTN4."""
    marcas = []
    for m in TOKEN.finditer(texto):
        antes = texto[:m.start()]
        previo = antes.rstrip(' \t')
        # sólo empieza un tramo si viene al principio, tras una viñeta/separador o tras varios espacios;
        # "el próximo toque del BTN5" es una mención, no un control nuevo
        empieza = (not previo or previo[-1] in '-·•*:;|,(\n→>' or antes.endswith('  ')
                   or re.search(r'(?:^|\n)\s*(?:\d+[.)]\s*)?(?:EL |LA )?$', antes) is not None)
        nombre = (m.group(1) + m.group(2)) if m.group(1) else m.group(3)
        if re.match(r'\s*[,)](?!\s*(?:BTN|POT)\s?[1-9])', texto[m.end():]):                   # "(…de BTN2, o …)": también es una mención
            empieza = False
        marcas.append({'ini': m.start(), 'fin': m.end(), 'nombre': nombre, 'empieza': empieza})
    grupos = []
    for mk in marcas:
        if grupos:
            g = grupos[-1]
            entre = texto[g['fin']:mk['ini']]
            # POT1–4 / POT1..4: el segundo número viene sin prefijo y ya quedó dentro del primero
            if re.fullmatch(r'\s*(?:\.\.\.?|–|-|a)\s*', entre):
                tipo = 'rango'
            elif re.fullmatch(r'\s*(?:/|,|y)\s*', entre):
                tipo = 'lista'
            elif re.fullmatch(r'\s*\+\s*', entre):
                tipo = 'combo'
            else:
                tipo = None
            if tipo and g['tipo'] in (None, tipo):
                g['tipo'] = tipo
                g['nombres'].append(mk['nombre'])
                g['fin'] = mk['fin']
                continue
        if not mk['empieza']:
            continue
        grupos.append({'ini': mk['ini'], 'fin': mk['fin'], 'nombres': [mk['nombre']], 'tipo': None})
    # BTN2/3/4: los números siguientes vienen sin prefijo
    for g in grupos:
        m = re.match(r'(?:\s*/\s*[1-9]\b)+', texto[g['fin']:])
        if m and len(g['nombres']) == 1:
            g['nombres'] += [g['nombres'][0][:-1] + d for d in re.findall(r'[1-9]', m.group(0))]
            g['tipo'] = 'lista'
            g['fin'] += m.end()
    # POT1–4 (sin el segundo "POT")
    for g in grupos:
        m = re.match(r'\s*(?:\.\.\.?|–|-)\s*([1-9])\b', texto[g['fin']:])
        if m and len(g['nombres']) == 1:
            g['nombres'].append(g['nombres'][0][:-1] + m.group(1))
            g['tipo'] = 'rango'
            g['fin'] += m.end()
    return grupos


def _expandir(g):
    n = g['nombres']
    if g['tipo'] == 'rango' and len(n) == 2 and n[0][:3] == n[1][:3]:
        a, b = int(n[0][3:]), int(n[1][3:])
        return [n[0][:3] + str(k) for k in range(min(a, b), max(a, b) + 1)]
    return n


def controles(secs):
    textos = [ls for t, ls in secs if re.match(r'(?i)(FUNCIONAMIENTO|CONTROLES|USO|MANEJO)', t)]
    if not textos:
        textos = [ls for t, ls in secs if not re.match(r'(?i)(HARDWARE|ARDUINO|LIBRER|REPOSITORIO)', t)]
    bloques, pasos = [], []
    for ls in textos:
        # "MODO DE USO:" son los pasos para empezar, no la descripción de los controles
        modo = None
        for i, l in enumerate(ls):
            if re.match(r'^\s*(MODO DE USO|C[OÓ]MO (USAR|EMPEZAR)|PARA EMPEZAR)\b', l, re.I):
                modo = i
                break
        if modo is None:
            bloques.append(ls)
        else:
            bloques.append(ls[:modo])
            pasos += ls[modo + 1:]
    por = {}
    combos = []
    orden_ctx = []                                                  # los paneles/modos, en el orden en que aparecen
    for ctx0, it in items(bloques):
        texto = '\n'.join(it)
        texto = re.sub(r'^\s*(?:' + VIÑETA + r'|\d+[.)])\s*', '', texto)
        gs = _grupos(texto)
        if not gs:
            continue
        lider = limpiar(texto[:gs[0]['ini']]).rstrip(':').strip()
        lider = re.sub(r'^(?:EL|LA|LOS|LAS)$', '', lider)
        ctx = _ctx(lider) if lider else ctx0
        panel = dentro = ''
        for k, g in enumerate(gs):
            fin = gs[k + 1]['ini'] if k + 1 < len(gs) else len(texto)
            crudo = texto[g['fin']:fin]
            t = limpiar(crudo)
            if not t or re.fullmatch(r'\d+', t):                    # "BTN1 -> 44": es el pin, no una función
                continue
            nombres = _expandir(g)
            # "BTN2 → PANEL A : POT1..POT4 = …": el botón abre un panel y las perillas que siguen son de ese panel
            sigue_pot = k + 1 < len(gs) and gs[k + 1]['nombres'][0].startswith('POT')
            if nombres[0].startswith('BTN') and sigue_pot and crudo.rstrip().endswith(':') and len(t) <= 34 and '. ' not in t:
                panel = mayuscula(desgritar(t.rstrip(':').strip()))
                t = panel
            entrada = {'ctx': dentro or (panel if (panel and nombres[0].startswith('POT')) else ctx),
                       'texto': corto(t), 'largo': oraciones(desgritar(re.sub(r'\s+', ' ', t)))}
            # "BTN2 + BTN4 → entra y sale del MENÚ. Adentro: BTN1 Em–C · … POT1 ataque": lo que sigue es del menú
            m = re.search(r'(?i)\bentra(?:\s+y\s+sale)?\s+(?:del|al|a)\s+(men[uú]|panel\s+\w+)', t)
            if m:
                dentro = mayuscula(desgritar(m.group(1)))
            if entrada['ctx'] and entrada['ctx'] not in orden_ctx:
                orden_ctx.append(entrada['ctx'])
            if g['tipo'] == 'combo' and len(nombres) > 1:
                combos.append({'teclas': nombres, 'texto': entrada['texto'], 'largo': entrada['largo']})
                continue
            for n in nombres:
                lst = por.setdefault(n, [])
                if not any(e['texto'] == entrada['texto'] and e['ctx'] == entrada['ctx'] for e in lst):
                    lst.append(entrada)
    pasos_txt = []
    for _, it in items([pasos]):
        t = re.sub(r'^\s*(?:\d+[.)]|' + VIÑETA + r')\s*', '', ' '.join(l.strip() for l in it))
        if t:
            pasos_txt.append(oraciones(desgritar(re.sub(r'\s+', ' ', t).rstrip('.'))))
    return por, combos, pasos_txt[:6], orden_ctx


def _clave(ctx):
    """'Panel B (notas / tonalidad)' -> 'b' · 'Modo perc' -> 'perc' · 'Menú' -> 'menu' · 'General' -> 'general'"""
    c = re.split(r'\s+·\s+|\s*\(', ctx.lower())[0].strip()
    c = re.sub(r'^(?:panel|modo|banco)\s+', '', c)
    return c.translate(str.maketrans('áéíóúü', 'aeiouu')).strip()


def _dim(ctx):
    return 'modo' if ctx.lower().startswith('modo ') else 'panel'


def modelo_paneles(por, combos, orden_ctx):
    """Cómo se pasa de un panel a otro, leído de los mismos textos, para que el simulador siga en qué panel está
    la placa y muestre sólo lo que hace cada control AHÍ. Tipos de cambio:
      ciclo     'BTN4 → siguiente panel (General → Bajo → …)'          · 'BTN4+BTN5 cicla los paneles A → B → C → A'
      ir        'BTN2 → PANEL A'                                         · '… y panel general'
      alternar  'BTN1+BTN5 a la vez → Panel B' (el mismo combo vuelve)  · 'BTN1+BTN3 cambia modo de tocar (perc ↔ seq)'
      mantener  'BTN5 mantener → panel B (mientras lo tengas apretado)'"""
    if not orden_ctx:
        return None
    for lst in por.values():
        for e in lst:
            e['dim'] = _dim(e['ctx']) if e['ctx'] else ''
    dims = {}
    for c in orden_ctx:
        dims.setdefault(_dim(c), []).append(c)
    claves = {d: {_clave(c): c for c in cs} for d, cs in dims.items()}

    def buscar(nombre, dim=None):
        k = _clave(nombre.strip(' ().,:;'))
        for d in ([dim] if dim else claves):
            if d in claves and k in claves[d]:
                return d, claves[d][k]
        return None, None

    cambios = []
    acciones = [(c['teclas'], c['largo']) for c in combos]
    acciones += [([n], e['largo']) for n, lst in por.items() if n.startswith('BTN') for e in lst if not e['ctx']]
    for teclas, texto in acciones:
        t = texto.lower()
        m = re.search(r'([\wáéíóúñ]+)\s*↔\s*([\wáéíóúñ]+)', texto)
        if m:
            d1, a = buscar(m.group(1))
            d2, b = buscar(m.group(2))
            if a and b and d1 == d2:
                cambios.append({'teclas': teclas, 'tipo': 'alternar', 'dim': d1, 'a': a, 'b': b})
            continue
        if re.search(r'siguiente panel|panel siguiente|cicla (?:los )?paneles', t):
            orden = []
            for trozo in re.split(r'\s*→\s*', texto):
                pal = re.findall(r'[\wáéíóúñ]+', trozo)
                c = buscar(pal[-1], 'panel')[1] if pal else None
                if c and c not in orden:
                    orden.append(c)
            cambios.append({'teclas': teclas, 'tipo': 'ciclo', 'dim': 'panel',
                            'orden': orden if len(orden) > 1 else dims.get('panel', [])})
            continue
        m = re.search(r'(?:panel\s+([\wáéíóúñ]+)|men[uú])', t)
        if not m:
            continue
        c = buscar(m.group(1) if m.group(1) else 'menu', 'panel')[1]
        if re.search(r'mantener|mientras', t) and c:
            cambios.append({'teclas': teclas, 'tipo': 'mantener', 'dim': 'panel', 'a': c})
        elif len(teclas) == 1 and (re.match(r'(?:panel|banco|men[uú])\b', t) or re.search(r'\bpanel \w+$', t)):
            cambios.append({'teclas': teclas, 'tipo': 'ir', 'dim': 'panel', 'a': c or ''})
        elif c and (len(teclas) > 1 or re.search(r'entra|sale|vuelve', t)):
            cambios.append({'teclas': teclas, 'tipo': 'alternar', 'dim': 'panel', 'a': c})
    # sin un botón que cambie de panel no hay paneles que seguir: los subtítulos ("ESCALA —", "SÍNTESIS —") sólo agrupan
    con_cambio = {c['dim'] for c in cambios}
    dims = {d: cs for d, cs in dims.items() if d in con_cambio}
    if not dims:
        return None
    # el panel de partida: el primero que aparece, salvo que a ése se entre con un botón y haya perillas sin panel:
    # entonces se parte en el principal, el que no tiene nombre (el MENÚ de industrial_matriz, los paneles de la_partida)
    pots = [e for n, lst in por.items() if n.startswith('POT') for e in lst]
    destinos = {c.get('a') for c in cambios} | {x for c in cambios for x in c.get('orden', [])[1:]}
    principal = (any(not e['ctx'] for e in pots) and any(e['dim'] == 'panel' for e in pots)
                 and dims.get('panel', [''])[0] in destinos)
    res = {}
    for d, cs in dims.items():
        inicial = '' if (d == 'panel' and principal) else cs[0]
        res[d] = {'opciones': ([''] if inicial == '' else []) + cs, 'inicial': inicial}
    return {'dims': res, 'cambios': cambios}


def guia(src, nombre=''):
    lineas = encabezado(src)
    secs = secciones(lineas)
    tit, sub = titulo(lineas, nombre)
    por, combos, pasos, orden_ctx = controles(secs)
    return {'titulo': tit, 'subtitulo': sub, 'descripcion': descripcion(secs),
            'controles': por, 'combos': combos[:6], 'pasos': pasos,
            'paneles': modelo_paneles(por, combos, orden_ctx)}


def _md_plano(t):
    t = re.sub(r'\*\*|__|`', '', t)
    t = re.sub(r'(?<![\w*])\*(?!\s)([^*]+)\*', r'\1', t)
    t = re.sub(r'\[([^\]]+)\]\([^)]*\)', r'\1', t)
    return re.sub(r'\s+', ' ', t).strip()


def completar_con_readme(g, md):
    """Lo que el encabezado no trae lo saca del README de la carpeta: la descripción (primer párrafo)
    y, si el encabezado no describe ningún control, la tabla | BTN1 | … |."""
    if not g['descripcion']:
        p, dentro = [], False
        for l in md.splitlines():
            s = l.strip()
            if s.startswith('```'):
                dentro = not dentro
                continue
            if dentro or s.startswith('#') or re.match(r'(?i)autor', s) or s.startswith('|'):
                if p:
                    break
                continue
            if not s:
                if p:
                    break
                continue
            p.append(s)
        texto = _md_plano(' '.join(p))
        if len(texto) > 420:
            c = texto[:420]
            k = c.rfind('. ')
            texto = c[:k + 1] if k > 160 else c.rsplit(' ', 1)[0] + '…'
        g['descripcion'] = texto
    if not any(k.startswith('BTN') for k in g['controles']):
        for l in md.splitlines():
            m = re.match(r'^\|\s*\**\s*((?:BTN|POT)\s?[1-9](?:\s*[/+,]\s*(?:BTN|POT)?\s?[1-9])*)\s*\**\s*\|\s*(.+?)\s*\|?\s*$', l)
            if not m:
                continue
            texto = _md_plano(m.group(2).split('|')[0])
            nombres = [a + b for a, b in re.findall(r'(BTN|POT)\s?([1-9])', m.group(1))]
            for d in re.findall(r'[/,]\s*([1-9])\b', m.group(1)):
                nombres.append(nombres[0][:3] + d)
            if '+' in m.group(1) and len(nombres) > 1:
                g['combos'].append({'teclas': nombres, 'texto': corto(texto)})
                continue
            for n in nombres:
                g['controles'].setdefault(n, []).append({'ctx': '', 'texto': corto(texto), 'largo': texto})
    return g


if __name__ == '__main__':
    import json
    import os
    import sys
    raiz = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'firmwares'))
    for n in sys.argv[1:]:
        with open(os.path.join(raiz, n, n + '.ino'), encoding='utf-8', errors='replace') as f:
            g = guia(f.read(), n)
        md = os.path.join(raiz, n, 'README.md')
        if os.path.exists(md):
            with open(md, encoding='utf-8', errors='replace') as f:
                completar_con_readme(g, f.read())
        print(json.dumps(g, ensure_ascii=False, indent=1))
