#ifndef CONFIG_H
#define CONFIG_H

// ============================
// Firmware info
// ============================
#define FIRMWARE_VERSION "1.2.3-Refactored"

// ============================
// Motor Configuration
// ============================
const float MAX_RPM = 510.0f;
const double SPR_MEAS = 3659; // Steps per revolution for measurement

// ============================
// WiFi Configuration
// ============================
const char* AP_SSID_PREFIX = "BioShaker_";
const char* AP_PASSWORD = "bioshaker";
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_GATEWAY(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);
const int WIFI_CONNECT_TIMEOUT_MS = 20000;

#endif // CONFIG_H
