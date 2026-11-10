# ==============================================================================================================================================
# GENERAR_NOTICIAS — los samples de matriz_oscura: un noticiero del futuro que se glitchea mientras informa (GC Lab Chile)
# ==============================================================================================================================================
# Uso:
#   python generar_noticias.py voces          → lista las voces de tu cuenta de ElevenLabs (para elegir presentadores)
#   python generar_noticias.py                → genera las frases que falten en noticias/crudo/ y arma banco/<banco>/N.wav con glitch
#   python generar_noticias.py --solo-glitch  → no llama a la API: sólo rehace el glitch desde noticias/crudo/
#
# La clave: ELEVENLABS_API_KEY del entorno o, si no está, la de ../sampler_ia/secretos.h (ignorado por git). Nunca
# se imprime ni se copia. Las voces salen en mp3 44.1 kHz a noticias/crudo/ (no se vuelven a pedir si ya están: no
# se gasta dos veces). El GLITCH va horneado en cada sample (al tablero no le cuesta nada): sílabas que tartamudean,
# cortes de señal, el «parlante de tele» (pasa-banda) y una caída de cinta al final; el banco SEÑAL CAÍDA lleva más.
# Después: python preparar_samples.py (mete banco/ en samples.h).
# ==============================================================================================================================================
import json, os, re, subprocess, sys, time, urllib.request, urllib.error
import numpy as np
from scipy.signal import butter, sosfilt

AQUI = os.path.dirname(os.path.abspath(__file__))
SR = 44100
CRUDO = os.path.join(AQUI, "noticias", "crudo")
MODELO = "eleven_multilingual_v2"

# Presentador (H) y presentadora (M): ids de voces de ElevenLabs (python generar_noticias.py voces)
VOZ = {"H": "onwK4e9ZLuTAKqWW03F9", "M": "XrExE9yKIg1WjnnlVkGX"}   # Daniel (presentador de noticias) · Matilda

# Los 3 bancos del panel Samples. Cada frase: (texto, voz, cuánto glitch 0–1)
BANCOS = {
    "titulares": [
        ("Última hora: el agua potable ya cotiza en bolsa.", "H", 0.35),
        ("Se agota el cielo azul en tres regiones.", "M", 0.35),
        ("Gobierno confirma: dormir ahora requiere suscripción.", "H", 0.35),
        ("Nueva ley prohíbe los recuerdos sin licencia.", "M", 0.35),
        ("Las ciudades costeras pasan a ser submarinas.", "H", 0.35),
        ("Inteligencia artificial gana las elecciones municipales.", "M", 0.35),
        ("Alerta: se acaba el silencio en la capital.", "H", 0.35),
        ("Récord de calor: ochenta grados a la sombra.", "M", 0.35),
    ],
    "presentador": [
        ("Buenas noches. Esto es Noticias dos mil noventa y nueve.", "M", 0.3),
        ("Interrumpimos la transmisión con un comunicado oficial.", "H", 0.3),
        ("Mantenga la calma. Su emoción ha sido registrada.", "M", 0.3),
        ("Este noticiero es generado automáticamente.", "H", 0.3),
        ("Repetimos: no salga de su cápsula.", "M", 0.3),
        ("Le recordamos que su oxígeno vence hoy.", "H", 0.3),
        ("La verdad estará disponible en la próxima actualización.", "M", 0.3),
        ("Gracias por su obediencia. Buenas noches.", "H", 0.3),
    ],
    "senal": [
        ("Señal perdida.", "H", 0.9),
        ("Error de sistema en la red humana.", "M", 0.85),
        ("Transmisión no autorizada.", "H", 0.85),
        ("Reiniciando la realidad.", "M", 0.9),
        ("Los datos mienten.", "H", 0.95),
        ("Conexión inestable con el presente.", "M", 0.85),
        ("Su humanidad ha expirado.", "H", 0.9),
        ("Fin de la transmisión.", "M", 0.8),
    ],
}


def clave():
    k = os.environ.get("ELEVENLABS_API_KEY")
    if k:
        return k
    ruta = os.path.join(AQUI, "..", "sampler_ia", "secretos.h")
    m = re.search(r'ELEVEN_API_KEY\s*=\s*"([^"]+)"', open(ruta, encoding="utf-8").read()) if os.path.exists(ruta) else None
    if not m:
        sys.exit("No encuentro la clave: define ELEVENLABS_API_KEY")
    return m.group(1)


def api(ruta, datos=None):
    req = urllib.request.Request("https://api.elevenlabs.io" + ruta, data=json.dumps(datos).encode() if datos else None,
                                 headers={"xi-api-key": clave(), "Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=120) as r:
            return r.read()
    except urllib.error.HTTPError as e:            # el mensaje de ElevenLabs dice por qué (clave, permisos, créditos)
        sys.exit(f"ElevenLabs respondió {e.code} en {ruta}: {e.read()[:300].decode(errors='replace')}")


def listar_voces():
    for v in json.loads(api("/v1/voices"))["voices"]:
        l = v.get("labels", {})
        print(f'{v["voice_id"]}  {v["name"]:22s} {l.get("gender", ""):7s} {l.get("accent", ""):12s} {l.get("use_case", l.get("use case", ""))}')


def decodificar(ruta):
    c = subprocess.run(["ffmpeg", "-v", "error", "-i", ruta, "-f", "s16le", "-ac", "1", "-ar", str(SR), "-"],
                       check=True, capture_output=True).stdout
    return np.frombuffer(c, np.int16).astype(np.float64) / 32768.0


def glitch(x, cuanto, sem):
    """El noticiero que se rompe: tartamudeo de sílabas, cortes, parlante de tele, caída de cinta al final."""
    r = np.random.default_rng(sem)
    # 1) parlante de tele: pasa-banda 180 Hz – 6 kHz y un poco de compresión suave
    x = sosfilt(butter(2, [180, 6000], "bandpass", fs=SR, output="sos"), x)
    x = np.tanh(2.2 * x / max(np.abs(x).max(), 1e-9)) / np.tanh(2.2)
    v = int(0.01 * SR); env = np.sqrt(np.convolve(x ** 2, np.ones(v) / v, "same"))
    # 2) tartamudeos: se repite un trozo de 40–110 ms (con fundidos de 2 ms) en puntos donde hay voz
    out = []; i = 0; f = int(0.002 * SR)
    n_tart = int(1 + cuanto * 4)
    puntos = sorted(r.choice(np.nonzero(env > env.max() * 0.3)[0], size=min(n_tart, 50), replace=False)) if env.max() > 0 else []
    for p in puntos:
        if p <= i: continue
        out.append(x[i:p]); trozo = x[p:p + int(r.uniform(0.04, 0.11) * SR)].copy()
        if len(trozo) > 2 * f: trozo[:f] *= np.linspace(0, 1, f); trozo[-f:] *= np.linspace(1, 0, f)
        for _ in range(int(r.integers(2, 3 + int(3 * cuanto)))): out.append(trozo)
        i = p
    out.append(x[i:]); y = np.concatenate(out)
    # 3) cortes de señal: huecos de 15–60 ms (con fundido) y algún tramo a media velocidad
    for _ in range(int(cuanto * 3)):
        a = int(r.uniform(0.15, 0.85) * len(y)); b = a + int(r.uniform(0.015, 0.06) * SR)
        if b < len(y): y[a:b] *= 0.0; y[max(0, a - f):a] *= np.linspace(1, 0, min(f, a)); y[b:b + f] *= np.linspace(0, 1, len(y[b:b + f]))
    # 4) caída de cinta al final (la última ~0.35 s baja de tono y se frena)
    n = min(len(y), int(0.35 * SR * (0.5 + cuanto)))
    if n > 100:
        t = np.linspace(0, 1, n); vel = 1.0 - 0.75 * t ** 1.5
        pos = np.cumsum(vel); pos = pos / pos[-1] * (n - 1)
        y[-n:] = np.interp(pos, np.arange(n), y[-n:]) * (1 - t ** 3)
    return y


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "voces":
        listar_voces(); return
    os.makedirs(CRUDO, exist_ok=True)
    solo = "--solo-glitch" in sys.argv
    for nb, (banco, frases) in enumerate(BANCOS.items()):
        dest = os.path.join(AQUI, "banco", banco); os.makedirs(dest, exist_ok=True)
        for k, (texto, voz, cuanto) in enumerate(frases, 1):
            crudo = os.path.join(CRUDO, f"{banco}_{k}.mp3")
            if not os.path.exists(crudo):
                if solo: print("falta", crudo); continue
                datos = api(f"/v1/text-to-speech/{VOZ[voz]}?output_format=mp3_44100_128",
                            {"text": texto, "model_id": MODELO, "language_code": "es",
                             "voice_settings": {"stability": 0.55, "similarity_boost": 0.8, "style": 0.25}})
                open(crudo, "wb").write(datos); time.sleep(0.4)
            x = decodificar(crudo)
            y = glitch(x, cuanto, sem=1000 * nb + k)
            pcm = np.clip(np.round(y * 0.9 * 32767), -32768, 32767).astype("<i2")
            import wave
            with wave.open(os.path.join(dest, f"{k}.wav"), "wb") as w:
                w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes(pcm.tobytes())
            print(f"banco/{banco}/{k}.wav  {len(y) / SR:4.2f} s  [{voz}] {texto}")


if __name__ == "__main__":
    main()
