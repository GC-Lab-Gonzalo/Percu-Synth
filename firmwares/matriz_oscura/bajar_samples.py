# Baja el banco de samples de matriz_oscura: 39 sonidos CC0 de Freesound (sus vistas previas MP3), 8 por
# categoría (7 en fx: la sirena «Electronic Siren» sonaba a teléfono y se sacó), y los deja como banco/<categoría>/N.mp3. Después preparar_samples.py los mete dentro del
# firmware (samples.h). La lista con autores está en fuentes_samples.txt (CC0: no hace falta dar crédito, pero
# queda registrado). Sólo hace falta para rearmar el banco: el firmware ya trae samples.h.
# Uso: python bajar_samples.py      ·   después: python preparar_samples.py
import os, time, urllib.request
AQUI = os.path.dirname(os.path.abspath(__file__))
FS = "https://cdn.freesound.org/previews/"
LISTA = {
    "impactos": [
        (408141, "Jofae", "Cinematic Low Pitch Impact", "408/408141_6512973-hq.mp3"),
        (754424, "zazz.sound.design", "DSGNImpt_Deep Cinematic Impact 5_ZAZZ", "754/754424_726853-hq.mp3"),
        (816376, "harrisonlace", "DSGNStngr_basic trailer boom impact", "816/816376_9839964-hq.mp3"),
        (697057, "InMotionAudio", "DSGNBram_BramDistorted01", "697/697057_14083205-hq.mp3"),
        (856173, "brktkrgll", "Apocalyptic Cinematic Brass Braam Impact", "856/856173_18901108-hq.mp3"),
        (523783, "BennettFilmTeacher", "Boom Explosion", "523/523783_7254895-hq.mp3"),
        (751356, "AudioPapkin", "Sound Design Elements Impact SFX PS 011", "751/751356_8698658-hq.mp3"),
        (558238, "Rizzard", "Distorted Impact Short", "558/558238_10825267-hq.mp3"),
    ],
    "metales": [
        (406562, "Anthousai", "hit - metallic 01", "406/406562_5923045-hq.mp3"),
        (334419, "Markozz", "MetalHit2_boom", "334/334419_2972303-hq.mp3"),
        (244983, "ani_music", "ANI Big Pipe Hit", "244/244983_3008343-hq.mp3"),
        (371353, "Mrthenoronha", "Iron Hits", "371/371353_2402876-hq.mp3"),
        (668993, "vestibule-door", "single big impact, industrial press", "668/668993_14100561-hq.mp3"),
        (456330, "henrikcederblad", "SFX-metal-007", "456/456330_1819746-hq.mp3"),
        (697758, "SoundDesignForYou", "Metal Resonant Scrape Impact", "697/697758_13085623-hq.mp3"),
        (335976, "newagesoup", "metal-gate-slam", "335/335976_4067257-hq.mp3"),
    ],
    "gritos": [
        (398802, "DangAdv64", "Monster Scream Death", "398/398802_7589471-hq.mp3"),
        (347410, "zerokingfull", "Monster Roar 02", "347/347410_4450479-hq.mp3"),
        (444445, "Voices_Of_Marz", "Demon Growl", "444/444445_6670544-hq.mp3"),
        (221547, "joseppujol", "Horror scream (high-pitched voice)", "221/221547_4096172-hq.mp3"),
        (178313, "Thanra", "Male Death Scream", "178/178313_2534855-hq.mp3"),
        (222523, "uagadugu", "Deep Monster Growl", "222/222523_255863-hq.mp3"),
        (265554, "AugustSandberg", "Studio roar", "265/265554_1934171-hq.mp3"),
        (518773, "Creat142100", "Glitchy Robot Scream - 6", "518/518773_8278798-hq.mp3"),
    ],
    "voces": [
        (823850, "JW_Audio", "Dark, Demonic, Low, Voice, Horror, Recording 4", "823/823850_13183432-hq.mp3"),
        (686852, "dragonboi50120", "Dark Creepy Voice", "686/686852_12694808-hq.mp3"),
        (559107, "AgentA1cr", "Possessed Laugh", "559/559107_10501287-hq.mp3"),
        (350679, "DRFX", "Deep Evil Laugh", "350/350679_5106192-hq.mp3"),
        (554623, "AsaMakes", "Male Vocal Glitch Godlike Voice", "554/554623_12337091-hq.mp3"),
        (261336, "Stereo Surgeon", "Slowmo Voice Growl", "261/261336_2297168-hq.mp3"),
        (824559, "emma.emma.emma", "Arderás en el infierno (demon voice)", "824/824559_16857550-hq.mp3"),
        (362570, "phatkatz4", "Access Denied", "362/362570_3446152-hq.mp3"),
    ],
    "fx": [
        (511597, "robpnicholson", "Dirty Siren", "511/511597_9262049-hq.mp3"),
        (18143, "nickm", "ET-1SirenGlitch", "18/18143_60262-hq.mp3"),
        (341289, "LookIMadeAThing", "Glitch Noise 5", "341/341289_3968707-hq.mp3"),
        (371603, "JacksonMiller0", "Odd glitch", "371/371603_3717478-hq.mp3"),
        (834162, "JW_Audio", "Glitch, One-shot, Stutter, Pitched, Hard, Digital 13", "834/834162_13183432-hq.mp3"),
        (375926, "Glitchedtones", "Laser Zaps", "375/375926_3294528-hq.mp3"),
        (500546, "waxsocks", "Heavy Laser Cannon", "500/500546_1766049-hq.mp3"),
    ],
}
UA = "GCLabChile-PercuSynth/1.0 (sample bank for an ESP32 instrument; https://github.com/GC-Lab-Gonzalo/Percu-Synth)"

def main():
    for cat, lista in LISTA.items():
        os.makedirs(os.path.join(AQUI, "banco", cat), exist_ok=True)
        for n, (fid, autor, titulo, ruta) in enumerate(lista, 1):
            destino = os.path.join(AQUI, "banco", cat, f"{n}.mp3")
            if os.path.exists(destino) and os.path.getsize(destino) > 0:
                continue
            for intento in range(4):
                try:
                    with urllib.request.urlopen(urllib.request.Request(FS + ruta, headers={"User-Agent": UA}), timeout=60) as r:
                        open(destino, "wb").write(r.read())
                    print(f"banco/{cat}/{n}.mp3  <-  {fid} · {autor} · {titulo}")
                    time.sleep(0.8)
                    break
                except Exception as e:
                    print(f"     {fid}: {e} (reintento {intento + 1})"); time.sleep(5 + 5 * intento)

if __name__ == "__main__":
    main()
