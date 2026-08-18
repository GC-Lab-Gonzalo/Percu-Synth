// ==============================================================================================
// SECRETOS - PLANTILLA
// ==============================================================================================
//
// Copia este archivo como  secretos.h  (en esta misma carpeta) y escribe ahi tus claves.
// secretos.h esta en el .gitignore del repositorio: nunca se sube a GitHub.
//
//   asistente_naga/
//     asistente_naga.ino
//     secretos.example.h   <- se sube (esta plantilla, sin claves)
//     secretos.h           <- NO se sube (tus claves reales)
//
// ==============================================================================================

#pragma once

// --- WiFi ---
const char* WIFI_SSID = "TU_RED_WIFI";
const char* WIFI_PASS = "TU_PASSWORD_WIFI";

// --- NagaAI (transcripcion + chat + voz, todo con la misma clave) ---
// Se crea en el panel de https://naga.ac  (cuenta con Google / GitHub / Discord).
// Una sola clave sirve para los tres endpoints: no hace falta cuenta de OpenAI ni
// de ElevenLabs aunque uses sus modelos.
const char* NAGA_API_KEY = "TU_API_KEY_DE_NAGA";
