// ==============================================================================================
// SECRETOS - PLANTILLA
// ==============================================================================================
//
// Copia este archivo como  secretos.h  (en esta misma carpeta) y escribe ahi tus claves.
// secretos.h esta en el .gitignore del repositorio: nunca se sube a GitHub.
//
//   asistente_ia/
//     asistente_ia.ino
//     secretos.example.h   <- se sube (esta plantilla, sin claves)
//     secretos.h           <- NO se sube (tus claves reales)
//
// ==============================================================================================

#pragma once

// --- WiFi ---
const char* WIFI_SSID = "TU_RED_WIFI";
const char* WIFI_PASS = "TU_PASSWORD_WIFI";

// --- OpenAI (Whisper + GPT + TTS) ---
// https://platform.openai.com/api-keys
const char* OPENAI_API_KEY = "sk-proj-TU_API_KEY";
