// ==============================================================================================
// SECRETOS - PLANTILLA
// ==============================================================================================
//
// Copia este archivo como  secretos.h  (en esta misma carpeta) y escribe ahi tus redes.
// secretos.h esta en el .gitignore del repositorio: nunca se sube a GitHub.
//
//   laser_midi_wifi/
//     laser_midi_wifi.ino
//     secretos.example.h   <- se sube (esta plantilla, sin claves)
//     secretos.h           <- NO se sube (tus redes reales)
//
// ==============================================================================================

#pragma once

// --- MODO_AP = true: la red que CREA el ESP32 (el computador se conecta a ella) ---
// Inventa un nombre y una clave. La clave debe tener 8 caracteres o mas, si no la red no levanta.
const char* AP_SSID  = "NOMBRE_DE_LA_RED_DEL_ESP32";
const char* AP_CLAVE = "CLAVE_DE_8_O_MAS_CARACTERES";

// --- MODO_AP = false: tu red de siempre (el ESP32 se une a ella) ---
// Tiene que ser una red de 2.4 GHz: el ESP32-C3 no ve las de 5 GHz.
const char* WIFI_SSID  = "TU_RED_WIFI";
const char* WIFI_CLAVE = "TU_CLAVE_WIFI";
