# ==============================================================================================================================================
# GENERAR_ELEVENLABS — pide sonidos de naturaleza a ElevenLabs y los deja listos para sinfonia_natural — GC Lab Chile
# ==============================================================================================================================================
# Uso (en una terminal, con tu clave en una variable de entorno; la clave NUNCA va escrita en un archivo del repo):
#   PowerShell:  $env:ELEVENLABS_API_KEY = "tu_clave";  python generar_elevenlabs.py
#   bash:        ELEVENLABS_API_KEY=tu_clave python generar_elevenlabs.py
#
#   python generar_elevenlabs.py --solo vertiente zorzal     (sólo esos)
#   python generar_elevenlabs.py --seg 5                     (duración pedida a cada uno)
#   python generar_elevenlabs.py --sin-preparar              (sólo baja los mp3 a sonidos/)
#
# Baja cada sonido a sonidos/ia_<nombre>.mp3 (no repite los que ya están: borra uno para pedirlo de nuevo) y al final llama a
# preparar_muestras.py, que arma muestras.h. Cada pedido gasta créditos de tu cuenta de ElevenLabs.
# Puedes mezclar: deja también en sonidos/ audios bajados de la web (Freesound, Wikimedia Commons…) y entran igual.
# Los textos están en inglés porque así los entiende mejor el modelo; describen SÓLO el sonido, sin música.
# ==============================================================================================================================================
import argparse, os, subprocess, sys
import requests

URL = "https://api.elevenlabs.io/v1/sound-generation"
AQUI = os.path.dirname(os.path.abspath(__file__))
CARPETA = os.path.join(AQUI, "sonidos")

# Sólo animales: lluvia, viento y agua sueltos en BTN5 se confundían con la lluvia (la vertiente va aparte, en el POT4).
# Se guardan como sonidos/ia_<nombre>.mp3 para no pisar las grabaciones que ya están. La flash está casi llena
# (~2.5 de ~2.65 MB): antes de agregar, saca de sonidos/ los que quieras reemplazar.
SONIDOS = {
    "zorzal":      "a single thrush singing a melodic phrase in a quiet forest at dawn, no music",
    "chucao":      "a forest bird calling loudly twice from dense undergrowth, natural field recording, no music",
    "ranas":       "a few small frogs croaking softly near a pond at night, no music",
    "carpintero":  "a woodpecker drumming on a tree trunk in the distance, forest ambience, no music",
    "buho":        "an owl hooting twice in the distance at night, no music",
    "tortola":     "a dove cooing softly three times in a quiet garden, no music",
}


def pedir(texto, seg, clave):
    r = requests.post(URL, params={"output_format": "mp3_44100_128"},
                      headers={"xi-api-key": clave, "Content-Type": "application/json"},
                      json={"text": texto, "duration_seconds": seg, "prompt_influence": 0.5}, timeout=120)
    if r.status_code != 200:
        raise RuntimeError(f"HTTP {r.status_code}: {r.text[:300]}")
    return r.content


def main():
    ap = argparse.ArgumentParser(description="Sonidos de naturaleza con ElevenLabs para sinfonia_natural")
    ap.add_argument("--solo", nargs="*", help="nombres a generar: " + ", ".join(SONIDOS))
    ap.add_argument("--seg", type=float, default=4.0, help="duración pedida a cada sonido (0.5–22 s)")
    ap.add_argument("--sin-preparar", action="store_true", help="no armar muestras.h al final")
    a = ap.parse_args()
    clave = os.environ.get("ELEVENLABS_API_KEY", "").strip()
    if not clave:
        raise SystemExit("Falta la clave: define la variable de entorno ELEVENLABS_API_KEY (ver la cabecera del script)")
    os.makedirs(CARPETA, exist_ok=True)
    nombres = a.solo or list(SONIDOS)
    for n in nombres:
        if n not in SONIDOS:
            raise SystemExit(f"No conozco «{n}». Los que hay: {', '.join(SONIDOS)}")
        destino = os.path.join(CARPETA, "ia_" + n + ".mp3")
        if os.path.exists(destino):
            print(f"  {n}: ya está, lo dejo"); continue
        print(f"  {n}: pidiendo {a.seg:.1f} s…", end="", flush=True)
        try:
            open(destino, "wb").write(pedir(SONIDOS[n], a.seg, clave))
            print(" listo")
        except Exception as e:
            print(f" falló ({e})")
    if not a.sin_preparar:
        subprocess.run([sys.executable, os.path.join(AQUI, "preparar_muestras.py"), CARPETA], check=False)


if __name__ == "__main__":
    main()
