"""
PERCUSYNTH - SERVIDOR DE SAMPLES (reemplazo de ElevenLabs Text-to-Sound Effects)
GC Lab Chile - Gonzalo Sandoval

Expone un endpoint con la MISMA forma que /v1/sound-generation, de modo que el
firmware sampler_ia.ino sigue funcionando cambiando solo el host y la cabecera.

  POST /sfx
  {"text": "rusty metallic hit with long tail", "duration_seconds": 2.5, "loop": false}
  -> cuerpo de la respuesta: PCM crudo, 22050 Hz, MONO, int16 little-endian

Modelo: stabilityai/stable-audio-open-1.5 (Apache 2.0, 44.1 kHz estereo, hasta 47 s).
El repo en HuggingFace es gated: hay que aceptar la licencia en la web y guardar el
token en un secret de Modal llamado "huggingface".

    modal secret create huggingface HF_TOKEN=hf_xxxxx
    modal deploy percusynth_sfx_server.py

Si 1.5 te da problemas con diffusers, "stabilityai/stable-audio-open-1.0" usa la
misma pipeline y esta mas probado.

--------------------------------------------------------------------------------
CAMBIOS EN sampler_ia.ino (todos dentro de fetchSfx, salvo el ultimo)
--------------------------------------------------------------------------------
1. Host y ruta:
       if (!client.connect("TU-APP--sfx.modal.run", 443)) return 0;
       client.print("POST /sfx HTTP/1.1\r\n");
       client.print("Host: TU-APP--sfx.modal.run\r\n");

2. Cabecera de auth: cambia  xi-api-key: ...  por
       client.print(String("x-api-key: ") + SFX_API_KEY + "\r\n");

3. Cuerpo: quita "prompt_influence" y "model_id". Deja text, duration_seconds y loop.
   (guidance_scale vive ahora en el servidor; 7.0 equivale mas o menos a un
   prompt_influence alto, bajalo a ~4 si quieres resultados mas locos)

4. En generateIntoSlot(): borra el reintento en ulaw_8000. Este servidor devuelve
   siempre pcm_22050, asi que SLOT_RATE_ULAW y ulaw2linear() quedan sin uso.
   (el parametro outFormat de fetchSfx tambien sobra; puedes dejarlo e ignorarlo)

Nada mas cambia: el recorte de silencio, el anclaje a cruce por cero, la
normalizacion y el dimensionado de slots (22050 x 5 s = 220 KB) siguen validos.
--------------------------------------------------------------------------------
"""

import io
import os

import modal

MODEL_ID = "stabilityai/stable-audio-open-1.5"
OUT_RATE = 22050          # lo que espera SLOT_RATE_PCM en el firmware
MAX_SECS = 5.0            # MAX_SLOT_SECS: mas que esto no cabe en el slot
CACHE_DIR = "/cache"

image = (
    modal.Image.debian_slim(python_version="3.11")
    .pip_install(
        "torch==2.4.0",
        "diffusers==0.31.0",
        "transformers==4.44.2",
        "accelerate==0.34.2",
        "scipy==1.14.1",
        "numpy==1.26.4",
        "fastapi[standard]==0.115.0",
    )
    .env({"HF_HOME": CACHE_DIR})
)

app = modal.App("percusynth-sfx")
cache = modal.Volume.from_name("percusynth-sfx-cache", create_if_missing=True)


@app.cls(
    image=image,
    gpu="A10G",
    volumes={CACHE_DIR: cache},
    secrets=[modal.Secret.from_name("huggingface")],
    # El firmware bloquea el audio y manda MIDI Stop mientras espera. Mantener el
    # contenedor caliente unos minutos evita el arranque en frio (~40 s cargando pesos)
    # entre un sample y el siguiente durante un taller o un ensayo.
    scaledown_window=600,
    timeout=600,
)
class SfxGenerator:
    @modal.enter()
    def load(self):
        import torch
        from diffusers import StableAudioPipeline

        self.torch = torch
        self.pipe = StableAudioPipeline.from_pretrained(
            MODEL_ID,
            torch_dtype=torch.float16,
            token=os.environ["HF_TOKEN"],
            cache_dir=CACHE_DIR,
        ).to("cuda")
        cache.commit()

    def _render(self, text, duration, loop, steps, guidance, seed):
        import numpy as np
        from scipy.signal import resample_poly

        # Para loopear se genera un poco de mas y ese sobrante se funde sobre el
        # arranque. Stable Audio Open no tiene flag de loop como ElevenLabs, asi
        # que la costura hay que coserla a mano.
        tail = 0.25 if loop else 0.0
        end_s = min(duration + tail, MAX_SECS)

        gen = None
        if seed is not None:
            gen = self.torch.Generator("cuda").manual_seed(int(seed))

        audio = self.pipe(
            prompt=text,
            # El modelo tiende a meter cola de reverb y ruido de sala en los golpes
            # secos; esto lo mantiene util para percusion disparada por boton.
            negative_prompt="low quality, muffled, background noise, hiss, music, speech",
            num_inference_steps=steps,
            guidance_scale=guidance,
            audio_end_in_s=float(end_s),
            num_waveforms_per_prompt=1,
            generator=gen,
        ).audios[0]

        x = audio.float().cpu().numpy()          # (canales, muestras) a 44100

        # A mono: el firmware sintetiza el ancho con pan + Haas, mandar estereo
        # solo gastaria PSRAM y habria que sumarlo igual.
        x = x.mean(axis=0) if x.ndim > 1 else x

        # 44100 -> 22050 es exactamente /2, asi que resample_poly es limpio y barato.
        x = resample_poly(x, 1, 2)

        if loop:
            xf = int(0.25 * OUT_RATE)
            if len(x) > 2 * xf:
                head, tail_a = x[:xf], x[-xf:]
                r = np.linspace(0.0, 1.0, xf, dtype=np.float32)
                x = np.concatenate([head * r + tail_a * (1.0 - r), x[xf:-xf]])

        # Techo a -1 dBFS. El firmware normaliza otra vez en finishSlot(), pero
        # llegar ya cerca de escala completa le da mas margen al recorte de silencio.
        peak = float(np.max(np.abs(x))) or 1.0
        x = x / peak * 0.89

        return (np.clip(x, -1.0, 1.0) * 32767.0).astype("<i2").tobytes()

    @modal.method()
    def generate(self, text, duration, loop, steps, guidance, seed):
        return self._render(text, duration, loop, steps, guidance, seed)


@app.function(image=image, secrets=[modal.Secret.from_name("percusynth-sfx-key")])
@modal.fastapi_endpoint(method="POST", label="sfx")
def sfx(payload: dict, x_api_key: str = ""):
    from fastapi import HTTPException, Response

    if x_api_key != os.environ["SFX_API_KEY"]:
        raise HTTPException(status_code=401, detail="bad key")

    text = (payload.get("text") or "").strip()
    if not text:
        raise HTTPException(status_code=400, detail="text vacio")

    duration = min(max(float(payload.get("duration_seconds", 2.0)), 0.5), MAX_SECS)
    loop = bool(payload.get("loop", False))

    # steps: 100 es el punto dulce para one-shots percusivos. Bajar a 50 corta el
    # tiempo a la mitad y en golpes secos casi no se nota; en texturas si.
    steps = int(payload.get("steps", 100))
    guidance = float(payload.get("guidance", 7.0))
    seed = payload.get("seed")

    pcm = SfxGenerator().generate.remote(text, duration, loop, steps, guidance, seed)
    return Response(content=pcm, media_type="application/octet-stream")


@app.local_entrypoint()
def probar(prompt: str = "short dry metallic percussion hit, rusty, tight transient"):
    """Prueba desde el PC sin tocar el ESP32:  modal run percusynth_sfx_server.py"""
    pcm = SfxGenerator().generate.remote(prompt, 1.5, False, 100, 7.0, None)
    with open("sample.raw", "wb") as f:
        f.write(pcm)
    print(f"{len(pcm)//2} muestras @ {OUT_RATE} Hz -> sample.raw")
    print(f"Escuchar:  ffplay -f s16le -ar {OUT_RATE} -ac 1 sample.raw")
