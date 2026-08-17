// ==============================================================================================
// SECRETOS - PLANTILLA
// ==============================================================================================
//
// Copia este archivo como  secretos.h  (en esta misma carpeta) y escribe ahi tus claves.
// secretos.h esta en el .gitignore del repositorio: nunca se sube a GitHub.
//
//   oscilador_ia/
//     oscilador_ia.ino
//     secretos.example.h   <- se sube (esta plantilla, sin claves)
//     secretos.h           <- NO se sube (tus claves reales)
//
// ==============================================================================================

#pragma once

// --- WiFi ---
const char* WIFI_SSID = "TU_RED_WIFI";
const char* WIFI_PASS = "TU_PASSWORD_WIFI";

// --- OpenAI (Whisper + GPT para armar el prompt) ---
// https://platform.openai.com/api-keys
const char* OPENAI_API_KEY = "sk-proj-TU_API_KEY";

// --- ElevenLabs (generacion del sample) ---
// https://elevenlabs.io/app/settings/api-keys
// OJO: el formato pcm_22050 requiere plan Pro. En Starter el sketch cae solo a ulaw_8000.
const char* ELEVEN_API_KEY = "sk_TU_API_KEY";
