#ifndef CONFIG_H
#define CONFIG_H

#include <cstddef>
#include <IPAddress.h>

// ============================
// Firmware info
// ============================
#define FIRMWARE_VERSION "1.2.3-Refactored"

// ============================
// Motor Configuration
// ============================
extern const float MAX_RPM;
extern const double SPR_MEAS;

// ============================
// WiFi Configuration
// ============================
extern const char* AP_SSID_PREFIX;
extern const char* AP_PASSWORD;
extern const IPAddress AP_IP;
extern const IPAddress AP_GATEWAY;
extern const IPAddress AP_SUBNET;
extern const int WIFI_CONNECT_TIMEOUT_MS;

#endif // CONFIG_H
