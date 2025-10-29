#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include "motor_control.h"
#include "ui_manager.h"
#include "wifi_manager.h"
#include "config.h"

// ============================
// Definiciones de Configuración
// ============================
const float MAX_RPM = 510.0f;
const double SPR_MEAS = 3659;
const char* AP_SSID_PREFIX = "BioShaker_";
const char* AP_PASSWORD = NULL;
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_GATEWAY(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);
const int WIFI_CONNECT_TIMEOUT_MS = 20000;

// ============================
// Variables Globales
// ============================
SemaphoreHandle_t rpmMutex;

void setup() {
  Serial.begin(115200);
  delay(80);

  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed");
  }

  rpmMutex = xSemaphoreCreateMutex();

  motor_setup();
  ui_setup();
  wifi_setup();

  xTaskCreatePinnedToCore(ui_task, "uiTask", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(motor_task, "motorTask", 4096, NULL, 2, NULL, 1);
}

void loop() {
  // El loop principal está ahora vacío, ya que todo se gestiona en las tareas de FreeRTOS.
  // Se podría añadir aquí lógica de bajo nivel si fuera necesario en el futuro.
  vTaskDelay(pdMS_TO_TICKS(1000));
}
