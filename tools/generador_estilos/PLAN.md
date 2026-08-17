# GENERADOR DE ESTILOS — Diseño del sistema (documento vivo, v0.1)

> **Estado:** diseño en discusión. Sin código todavía. Este documento ES el producto de esta fase:
> se itera aquí hasta que el sistema convenza, y recién entonces se implementa (webapp con sonido
> primero, PercuSynth después).
>
> Nombre provisional del proyecto: *Generador de Estilos* (candidatos de marca: **MAESTRO**,
> **CANCIONERO**, **STYLA** — pendiente).

---

## 1. Visión y flujo completo

Generar canciones con **estilos musicales marcados y reconocibles** (blues, techno, grunge,
synthwave, cumbia…) a partir de parámetros, donde cada estilo está codificado con su teoría real:
armonía, rítmica, melodía, forma y timbre.

```
  VOZ ("quiero un sad blues lento y melancólico")
   │
   ▼
  ASISTENTE IA  (firmwares/asistente_ia: mic → Whisper → LLM con system prompt)
   │  el LLM conoce este sistema y responde SOLO un JSON
   ▼
  SongSpec (JSON)  ──►  MOTOR GENERADOR (determinista por seed)
                          │  1. resuelve StyleDef + macros → SongPlan
                          │  2. genera forma → armonía → ritmo → melodía
                          ▼
                        AUDIO  (fase 1: Web Audio en la webapp;
                                fase final: PercuSynth I2S o MIDI)
   ▲
  POTS en vivo (volumen, filtro, macros en tiempo real)
```

**Principio central:** el estilo es **datos, no código**. Los motores (armonía, ritmo, melodía,
forma) son compartidos; cada estilo es un archivo JSON (`StyleDef`) que define su gramática.
Agregar un estilo nuevo = escribir un JSON, no programar.

**Principio de reproducibilidad** (heredado de `el_reloj_leds`): mismo `SongSpec` + mismo `seed`
= exactamente la misma canción. Cada canción tiene identidad ("blues #4F2A") y se puede volver a
escuchar, compartir o portar al hardware.

---

## 2. Los tres niveles de parámetros

La clave del diseño es separar **quién decide qué**:

| Nivel | Qué es | Quién lo escribe |
|---|---|---|
| **StyleDef** | La gramática completa de un estilo (biblioteca fija) | Nosotros, una vez por estilo |
| **SongSpec** | El pedido de una canción: estilo + macros + ajustes | **La IA** (o un humano) |
| **SongPlan** | La canción concreta resuelta (todos los valores finales) | El motor, determinista por seed |

La IA **no** necesita saber de biquads ni de máscaras de batería: elige un estilo, mueve 5 macros
expresivas y opcionalmente fuerza tonalidad/tempo. El motor traduce eso a música usando la
gramática del estilo. Esto mantiene el JSON de la IA corto, robusto y difícil de romper.

---

## 3. SongSpec — lo que emite la IA

```jsonc
{
  "version": 1,
  "seed": 48213,                    // opcional; si falta, el motor sortea uno y lo reporta
  "estilo": "blues",                // id del StyleDef
  "variante": "sad_blues",          // opcional: preset de macros con nombre (ver §5)

  "macros": {                       // 0.0 – 1.0, todas opcionales (default 0.5)
    "energia":   0.25,
    "oscuridad": 0.85,
    "densidad":  0.30,
    "tension":   0.40,
    "humanidad": 0.80
  },

  "tempo_bpm": 58,                  // opcional; si falta, lo decide energia dentro del rango del estilo
  "tonalidad": { "raiz": "A", "modo": "menor" },   // opcional
  "duracion_min": 2.5,              // opcional; el motor ajusta repeticiones de la forma

  "overrides": {                    // opcional, avanzado: fuerza campos puntuales del StyleDef
    "melodia.registro.centro": "G3"
  },

  "en_vivo": {                      // mapa de pots del PercuSynth (fase hardware)
    "pot1": "volumen_master",
    "pot2": "filtro_global",
    "pot3": "macro.densidad",
    "pot4": "macro.energia"
  }
}
```

**Ejemplo del caso de uso real** — *"quiero una canción tranquila, melancólica, lenta, con un
estilo sad blues"* → la IA responde:

```json
{ "version": 1, "estilo": "blues", "variante": "sad_blues",
  "macros": { "energia": 0.2, "oscuridad": 0.85, "densidad": 0.3, "tension": 0.4, "humanidad": 0.8 },
  "tempo_bpm": 58, "tonalidad": { "raiz": "A", "modo": "menor" } }
```

Otros pedidos:

- *"techno oscuro y rápido para bailar"* →
  `{ "estilo": "techno", "macros": { "energia": 0.9, "oscuridad": 0.8, "densidad": 0.7, "tension": 0.6 } }`
- *"algo ochentero épico, tipo película"* →
  `{ "estilo": "synthwave", "macros": { "energia": 0.7, "oscuridad": 0.4, "densidad": 0.6 }, "tonalidad": { "raiz": "F#", "modo": "menor" } }`

---

## 4. Las 5 macros expresivas

> **APLAZADO (feedback Gonzalo, 2026-07-16):** primero se monta el motor con parámetros completos
> pegados a mano (Claude genera el JSON de una canción específica → Gonzalo lo pega en la webapp).
> Las macros se evaluarán después, si hacen falta.

Son la interfaz principal de la IA. Cada macro modula parámetros **dentro de los límites del
estilo** — nunca rompe la gramática (energía 1.0 en un blues sigue siendo un blues, uno movido).

| Macro | Qué modula en el motor |
|---|---|
| **energia** | BPM (posición dentro del rango del estilo) · densidad de batería (hats, ghosts) · nº de capas activas · drive/brillo del timbre · velocity media · amplitud de la curva de intensidad de la forma |
| **oscuridad** | elección de progresión/modo dentro del vocabulario del estilo (mayor ↔ menor ↔ frigio) · registro (baja el centro) · cutoff del filtro · color del acorde (añade m9/b9 si tensión lo permite) · espacio/reverb más largo y oscuro |
| **densidad** | notas por compás de la melodía · subdivisión predominante (blancas ↔ semicorcheas) · actividad del bajo · relleno percusivo · silencios entre frases |
| **tension** | disonancias permitidas (extensiones 7/9/b9/#11 según estilo) · cromatismos de aproximación · cadencias rotas vs. resueltas · cuánto tarda en resolver |
| **humanidad** | micro-timing (±ms por nota) · variación de velocity · fluctuación sutil de tempo (rubato leve) · "imperfecciones" expresivas (anticipos, fills que se apuran) · 0.0 = máquina perfecta (techno), 1.0 = banda en vivo |

Cada StyleDef declara **rangos válidos** por macro (ej.: techno limita `humanidad` a 0–0.3 porque
el género ES máquina; blues limita `humanidad` a 0.4–1.0 por la razón inversa).

---

## 5. Variantes con nombre

Una **variante** es un preset de macros + overrides dentro de un estilo, con nombre evocador que
la IA puede reconocer directamente del lenguaje del usuario:

```jsonc
// dentro de blues.json
"variantes": {
  "sad_blues":    { "macros": { "energia": 0.2, "oscuridad": 0.85 }, "overrides": { "armonia.progresion_preferida": "blues_menor_12" } },
  "texas_shuffle":{ "macros": { "energia": 0.8, "oscuridad": 0.3 },  "overrides": { "tempo.feel": "shuffle_duro" } },
  "slow_burn":    { "macros": { "energia": 0.35, "tension": 0.7 } }
}
```

Esto da vocabulario natural al asistente ("ponme un texas shuffle") sin multiplicar estilos.

---

## 6. StyleDef — la gramática de un estilo (el corazón del sistema)

Esquema completo. Cada campo se explica una vez aquí; en §7 hay tres estilos reales completos.

```jsonc
{
  "meta": {
    "id": "blues",
    "nombre": "Blues",
    "familia": "afroamericana",
    "descripcion_ia": "12 compases, shuffle ternario, dominantes con 7ª, blue notes, call & response",
    "tags": ["triste", "crudo", "guitarra", "clásico"]   // ayudan al LLM a mapear lenguaje → estilo
  },

  // ── TEMPO Y FEEL ─────────────────────────────────────────────
  "tempo": {
    "bpm": { "min": 55, "max": 100, "default": 72 },   // energia interpola dentro del rango
    "metrica": "4/4",
    "grid": 12,             // subdivisiones por compás: 16 = semicorcheas, 12 = ternario (12/8)
    "swing": 0.0,           // solo para grid 16: 0.5 recto … 0.66 shuffle (retrasa las débiles)
    "humanidad_rango": [0.4, 1.0]
  },

  // ── ARMONÍA ──────────────────────────────────────────────────
  "armonia": {
    "tonalidades_preferidas": ["E", "A", "G", "C"],   // idiomáticas del estilo
    "modos": ["mixolidio", "menor"],                  // oscuridad elige entre ellos

    // Vocabulario: grados CON CALIDAD EXPLÍCITA (no solo diatónico —
    // el I7 del blues no es diatónico y es la esencia del género)
    "vocabulario": [
      { "grado": "I",   "calidad": "7",    "peso": 10 },
      { "grado": "IV",  "calidad": "7",    "peso": 8 },
      { "grado": "V",   "calidad": "7",    "peso": 8 },
      { "grado": "bVI", "calidad": "7",    "peso": 2 }   // color, aparece con tension alta
    ],
    // calidades soportadas por el motor:
    // maj, m, 5 (power chord), 7, m7, maj7, m7b5, dim7, sus2, sus4, add9, m9, 7#9

    // Progresiones: plantillas idiomáticas con peso (el motor elige y puede variar)
    "progresiones": [
      { "id": "blues_12_clasico",
        "compases": ["I7","I7","I7","I7","IV7","IV7","I7","I7","V7","IV7","I7","V7"],
        "peso": 10 },
      { "id": "blues_12_quick_change",
        "compases": ["I7","IV7","I7","I7","IV7","IV7","I7","I7","V7","IV7","I7","V7"],
        "peso": 5 },
      { "id": "blues_menor_12",
        "compases": ["im7","im7","im7","im7","ivm7","ivm7","im7","im7","VI7","V7","im7","V7"],
        "peso": 5, "requiere": { "oscuridad_min": 0.6 } }
    ],
    "ritmo_armonico": "1_acorde_por_compas",   // o "2_por_compas", "1_cada_2", "libre"
    "cadencias": { "final": "V7→I", "turnaround": ["I7 VI7 ii7 V7", "I7 IV7 I7 V7"] },
    "voicing": ["fundamental", "shell_37"]      // cómo se despliega el acorde en el pad/comping
  },

  // ── RÍTMICA (percusión) ──────────────────────────────────────
  "ritmica": {
    "kit": ["kick", "snare", "hat_c", "hat_o", "ride", "crash"],  // voces disponibles del estilo
    // Máscara probabilística por voz: peso 0-10 por step del grid.
    // 10 = siempre, 0 = nunca, intermedios = probabilidad (densidad la escala).
    "patrones": {
      "kick":  [10,0,0, 0,0,0, 7,0,0, 0,0,0],        // grid 12 (ternario): 1 y "3&"
      "snare": [0,0,0,  10,0,0, 0,0,0, 10,0,0],      // backbeat 2 y 4
      "ride":  [10,0,6, 10,0,6, 10,0,6, 10,0,6]      // patrón shuffle clásico
    },
    "acentos": [10,3,5, 8,3,5, 9,3,5, 8,3,6],        // curva de acentuación por step
    "ghosts": { "voz": "snare", "prob": 0.3 },        // notas fantasma (escala con humanidad)
    "fills": { "cada_compases": 4, "intensidad": 0.5, "estilo": "redoble_snare" },
    "variacion_por_seccion": true                     // el patrón respira según la forma
  },

  // ── BAJO ─────────────────────────────────────────────────────
  "bajo": {
    "patron": "walking_blues",
    // patrones del motor: walking, fundamental_quinta, octavas, offbeat (dub/reggae),
    //                     rolling_16 (techno), arpegio, riff_unisono (grunge/metal), sincopado_2_3
    "registro": "E1-G2",
    "lock_con_kick": 0.6,       // 0-1: cuánto se pega al bombo
    "cromatismo": 0.4           // notas de paso cromáticas (walking real)
  },

  // ── MELODÍA ──────────────────────────────────────────────────
  "melodia": {
    // La escala MELÓDICA puede diferir de la armonía: aquí vive la teoría real del estilo
    "escala": "pentatonica_menor_blues",   // añade b5; 3ª ambigua contra acordes dominantes
    "blue_notes": true,
    "registro": { "centro": "C4", "rango_semitonos": 14 },

    // Gramática de fraseo: CÓMO se construyen las frases
    "gramatica": "call_response",
    // gramáticas del motor: call_response | riff | arpegio | hook | lirica | pregunta_larga
    "fraseo": {
      "call":     { "compases": 2, "densidad_rel": 0.6 },
      "response": { "compases": 2, "variacion": ["transponer_al_acorde", "ornamentar", "invertir_contorno"] },
      "respiro":  { "prob_silencio_entre_frases": 0.7 }   // el blues respira
    },

    // Motivo: EL upgrade clave sobre cancion_aleatoria — repetición con variación
    "motivo": {
      "notas": [3, 6],               // longitud del motivo semilla
      "repeticion": 0.7,             // prob. de reusar el motivo vs. crear uno nuevo
      "ops_variacion": ["transponer_diatonico", "ornamentar", "desplazar_ritmo", "mutar_final"]
    },

    "anclaje_armonico": {
      "tiempos_fuertes": "nota_del_acorde",       // fuertes = chord tones, débiles = paso
      "aproximaciones_cromaticas": true,
      "nota_objetivo_cadencia": "fundamental"     // dónde aterriza la frase al resolver
    },
    "articulacion": { "legato": 0.4, "bend": 0.6, "vibrato_final_frase": true },
    "ritmo_frase": ["negras_con_sincopa", "corcheas_shuffle", "silencios_largos"]  // paletas rítmicas
  },

  // ── FORMA (secciones) ────────────────────────────────────────
  "forma": {
    "secciones": [
      { "id": "intro",    "compases": 4,  "intensidad": 0.30, "capas": ["bajo", "ride"] },
      { "id": "chorus_a", "compases": 12, "intensidad": 0.55, "capas": ["bajo", "bateria", "comping", "melodia"] },
      { "id": "chorus_b", "compases": 12, "intensidad": 0.75, "capas": ["todas"], "melodia": "variacion" },
      { "id": "solo",     "compases": 12, "intensidad": 0.90, "melodia": "improvisacion_densa" },
      { "id": "chorus_out","compases": 12, "intensidad": 0.60, "melodia": "motivo_original" },
      { "id": "outro",    "compases": 2,  "intensidad": 0.30, "cadencia": "final_ritardando" }
    ],
    "curva": "arco",              // arco | creciente | build_drop | terraza (techno suma capas)
    "transiciones": ["fill_bateria", "silencio_subito", "riser"],   // pegamento entre secciones
    "duracion_ajustable": "repetir_chorus"   // cómo estirar/encoger para cumplir duracion_min
  },

  // ── TIMBRE (por rol) ─────────────────────────────────────────
  // Fase webapp: presets Web Audio. Fase PercuSynth: se mapean al motor de voces I2S.
  "timbre": {
    "melodia": { "preset": "guitarra_blues", "osc": "saw+tri", "drive": 0.5, "lpf": 2800, "vibrato": true },
    "comping": { "preset": "piano_electrico", "osc": "fm_suave", "decay": "medio" },
    "bajo":    { "preset": "bajo_redondo", "osc": "sine+saw_sub", "lpf": 600 },
    "bateria": { "preset": "kit_acustico_synth", "kick": "sweep_grave", "snare": "tono+ruido", "ride": "metal_hp" }
  },

  // ── MEZCLA Y ESPACIO ─────────────────────────────────────────
  "mezcla": {
    "niveles": { "melodia": 0.9, "comping": 0.55, "bajo": 0.8, "bateria": 0.7 },
    "espacio": { "reverb": "sala_pequena", "delay": null },
    "stereo": { "comping": -0.3, "ride": 0.4 }
  },

  // ── LÍMITES DE MACROS (ver §4) ───────────────────────────────
  "macros_rango": {
    "humanidad": [0.4, 1.0],
    "tension":   [0.0, 0.7]
  },

  "variantes": { /* ver §5 */ }
}
```

---

## 7. Tres estilos trabajados (mismo esquema, músicas opuestas)

Resumen de los valores que diferencian a cada uno — para validar que el esquema captura el ADN:

| Campo | **Blues** | **Techno** | **Synthwave** |
|---|---|---|---|
| bpm / grid / feel | 55–100 · grid 12 (ternario) | 125–140 · grid 16 recto · humanidad 0–0.2 | 80–110 · grid 16 recto |
| vocabulario | I7, IV7, V7 (dominantes) | im, im7, bVI (estático) | im, VI, III, VII, iv, v (tríadas modales) |
| progresión | 12 compases fijos + turnaround | 1–2 acordes, cambia cada 4–8 compases | lazos de 4: i–VI–III–VII / VI–VII–i–i |
| ritmo armónico | 1 por compás | casi estático (la tensión es tímbrica) | 1 por compás, lazo hipnótico |
| batería | ride shuffle + backbeat, ghosts | four-on-floor, hats offbeat, clap 2&4, sin ghosts | kick 1 y 3(+síncopa), snare 2&4 enorme (gated) |
| bajo | walking con cromatismo | rolling_16 en fundamental / offbeat | octavas en corcheas |
| melodía | call & response, pentatónica blues + blue notes, respiros largos | hook corto (1–2 compases) repetido con mutación tímbrica, escala frigia/menor | arpegio 16avos (gramática `arpegio`) + lead lírica en coro |
| forma | intro → chorus×N → solo → chorus → outro (arco) | terraza: capas entran/salen cada 8/16 + break con riser + drop | intro arp → verso → CORO grande → puente → coro (curva `creciente`) |
| timbre | guitarra/EP/kit "acústico" sintetizado | análogo crudo: kick 909, stabs, rumble | analog brass, pads lush, snare con cola de reverb |
| macros_rango | humanidad 0.4–1.0 | humanidad 0–0.2, oscuridad 0.4–1.0 | libre |

*(los tres JSON completos se escribirán como los primeros archivos de la biblioteca cuando
aprobemos el esquema)*

---

## 8. Catálogo de estilos propuesto

Organizado por **familias** (los estilos de una familia comparten rasgos del motor). Cada estilo
= un JSON de ~150 líneas. **Marca los que te importan** — el orden de implementación sale de aquí.

| Familia | Estilos | ADN en una línea |
|---|---|---|
| **Electrónica** | techno | four-on-floor, armonía estática, terraza de capas |
| | house | 120–125, hats swing sutil, acordes m7/maj7 en stabs |
| | trance | 132–140, superserrucho, breakdown épico, arpegios (ya dominas esto) |
| | synthwave | lazos 80s, arpegio + snare gigante |
| | drum & bass | 170–175, breakbeat 2-step, sub reese |
| | ambient | sin pulso marcado, capas lentas (pariente de paisajes_relax) |
| | dub techno | acordes en delay, espacio infinito |
| | lofi hip-hop | 70–90, swing MPC, acordes jazz (m9, maj9), vinyl feel (humanidad alta) |
| **Afroamericana** | blues | 12 compases, shuffle, dominantes, blue notes |
| | jazz swing | walking bass, ii–V–I, ride swing, extensiones 9/13 |
| | funk | 16avos sincopados, un acorde (E9), ghost notes densos |
| | soul | 6/8 o 4/4 lento, progresiones I–vi–IV–V, melodía melismática |
| **Rock** | rock clásico | power chords + pentatónica, backbeat directo |
| | grunge | power chords cromáticos, verso quieto/coro fuerte |
| | punk | 160–200, tres acordes, corcheas constantes |
| | metal | riff unísono bajo+guitarra, frigio/locrio, doble bombo |
| **Jamaicana** | reggae/dub | skank en contratiempo, bajo protagonista, one drop |
| **Hip-hop** | boom bap | 85–95, swing duro, sample-feel |
| | trap | 130–150 half-time, hats en tresillos/rolls, 808 glide |
| **Cine** | cinematic épico | ostinatos, crescendo orquestal-synth |

*(familias Latina y Andina eliminadas del catálogo a pedido de Gonzalo — no le interesan)*

**Features del motor que exige el catálogo completo** (checklist de implementación):
grid 12 y 16 · swing paramétrico · half-time feel (trap) · acordes con calidad (13 calidades) ·
walking bass · formas por terraza y por arco · hat rolls · glide/portamento de bajo.
Todo cabe en los motores del §9 — ningún estilo exige un motor exclusivo.

---

## 9. Motores compartidos (lo que se implementará en fase webapp)

1. **Reloj:** grid 12/16, swing, half-time, micro-timing por humanidad, secciones. Continuo y
   global (aprendizaje de oximeter_midi_sync: todo siempre on-grid, mutear no desfasa).
2. **Motor armónico:** grados con calidad explícita → notas; plantillas de progresión + variación
   pesada por seed; turnarounds y cadencias; voicings (fundamental/shell/open — reusa la idea de
   voicing de cancion_aleatoria ampliada).
3. **Motor de motivos:** genera motivo semilla → lo repite aplicando operaciones de variación →
   gramáticas de frase (call&response, riff, hook, arpegio, lírica). *La pieza nueva más importante.*
4. **Motor de batería:** máscaras probabilísticas por voz + acentos + ghosts + fills + variación
   por sección. (Evolución directa del generador de percusión de cancion_aleatoria.)
5. **Motor de bajo:** 8 patrones idiomáticos parametrizados.
6. **Motor de forma:** scheduler de secciones + matriz capas×sección + transiciones + ajuste de duración.
7. **Render:** fase 1 Web Audio (roles → sintetizadores por preset); fase 2 PercuSynth (motor de
   voces estéreo de cancion_aleatoria) y/o salida MIDI (motor de oximeter_midi_sync).
8. **PRNG con seed** (LCG estilo el_reloj): cero `random()` sin semilla en todo el sistema.

---

## 10. Contrato con el asistente IA

El system prompt del asistente incluirá: (a) el catálogo con `descripcion_ia` y `tags` de cada
estilo, (b) la semántica de las 5 macros, (c) el esquema SongSpec, (d) la regla **"responde
únicamente el JSON"**. Bosquejo:

```
Eres el compositor del PercuSynth. El usuario te pide música en lenguaje natural.
Respondes SOLO un JSON SongSpec válido, sin texto adicional.
Estilos disponibles: [catálogo con tags]. Macros (0-1): energia, oscuridad,
densidad, tension, humanidad — [semántica §4].
Mapea el lenguaje emocional a macros: "triste/melancólico" → oscuridad alta +
energia baja; "para bailar" → energia alta; "épico" → tension + energia altas; …
Si el usuario nombra una variante conocida (sad blues, texas shuffle), úsala.
Si pide algo ambiguo, elige el estilo más cercano por tags — no preguntes.
```

Ventaja de la separación en niveles: aunque el LLM alucine un valor, el motor **valida contra el
esquema y los rangos del estilo** (clamp + defaults) — nunca sale música rota.

---

## 11. Roadmap

| Fase | Entregable | Criterio de éxito |
|---|---|---|
| **F0 (ahora)** | Este documento iterado contigo | El esquema te convence y el catálogo está priorizado |
| **F1** | Webapp: motores + 3 estilos semilla (blues, techno, synthwave) + editor manual de SongSpec + audio Web Audio | Escuchas 10 canciones por estilo y **reconoces el estilo a ciegas** |
| **F2** | Biblioteca ampliada (estilos = JSONs) + variantes | Cada estilo nuevo toma horas, no días |
| **F3** | Integración asistente IA (texto primero, voz después) → SongSpec | "Sad blues lento" por voz suena a sad blues lento |
| **F4** | PercuSynth: port del motor al ESP32 (audio I2S) y/o modo MIDI hacia gear | Autónomo en hardware con pots en vivo |

---

## 12. Decisiones tomadas (2026-07-16)

1. **Catálogo:** familias Latina y Andina eliminadas. El resto queda.
2. **Macros:** aplazadas. Flujo de trabajo acordado: Gonzalo pide una canción en lenguaje natural
   → Claude genera el JSON completo de parámetros → Gonzalo lo pega a mano en la webapp → se
   escucha y se itera. La IA por voz llega después, cuando el motor convenza.
3. **Modo de trabajo:** construir y probar, no seguir documentando. La webapp del motor vive en
   `tools/generador_estilos/index.html` con 4 estilos semilla (blues, techno, synthwave, grunge).
