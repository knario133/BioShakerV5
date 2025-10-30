#include "config.h"
#include "wifi_manager.h"
#include "motor_control.h"
#include "ui_manager.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include <WiFi.h>

// Extern variables
extern float targetRpm;
extern float currentRpm;
extern SemaphoreHandle_t rpmMutex;

// Web server instance
AsyncWebServer server(80);

// Variables para el escaneo en segundo plano
static volatile bool g_startScan = false;
static String g_scanResults = "[]";
SemaphoreHandle_t g_scanMutex;

// Prototypes
void setup_server();
void on_wifi_event(WiFiEvent_t event);
void wifi_scan_task(void *parameter);

/**
 * @brief Initializes the WiFi manager, starts the AP, and connects to a saved network if available.
 */
void wifi_setup() {
    g_scanMutex = xSemaphoreCreateMutex();
    WiFi.onEvent(on_wifi_event);
    startAPAlways();
    tryConnectSavedWifi(false);
    setup_server();
    xTaskCreatePinnedToCore(wifi_scan_task, "wifiScanTask", 4096, NULL, 1, NULL, 0);
}

/**
 * @brief Starts the WiFi in AP+STA mode.
 */
void startAPAlways() {
  WiFi.mode(WIFI_AP_STA);
  String ssid = String(AP_SSID_PREFIX) + String((uint32_t)ESP.getEfuseMac(), HEX).substring(4);
  WiFi.softAPdisconnect(true);
  delay(30);
  WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
  WiFi.softAP(ssid.c_str(), AP_PASSWORD, 6, 0, 4);
  delay(50);
  uiState = UI_AP_MODE;
  uiForceRedraw = true;
}

/**
 * @brief Tries to connect to a previously saved WiFi network.
 * @param asyncRetry If true, the connection attempt is non-blocking.
 */
void tryConnectSavedWifi(bool asyncRetry) {
  File f = LittleFS.open("/wifiConfig.json", "r");
  if (!f) return;

  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, f)) {
    f.close();
    return;
  }
  f.close();

  String ssid = doc["ssid"] | "";
  String password = doc["password"] | "";
  if (ssid.length() == 0) return;

  WiFi.begin(ssid.c_str(), password.c_str());

  if (!asyncRetry) {
    unsigned long t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < WIFI_CONNECT_TIMEOUT_MS) {
      delay(250);
    }
  }
}

/**
 * @brief Disconnects from WiFi and turns off the radio.
 */
void goOffline() {
  g_offlineRequested = true;
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  uiForceRedraw = true;
}

/**
 * @brief Checks if the device is connected to a WiFi network.
 * @return True if connected, false otherwise.
 */
bool isStaConnected() {
  return WiFi.status() == WL_CONNECTED;
}

/**
 * @brief Sets up the web server and API endpoints.
 */
void setup_server() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (LittleFS.exists("/index.html")) {
      request->send(LittleFS, "/index.html", "text/html");
    } else {
      String html = "<html><head><title>BioShaker</title></head><body>"
                    "<h2>No index.html</h2></body></html>";
      request->send(200, "text/html", html);
    }
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    StaticJsonDocument<512> doc;
    float cur = 0.0f;
    if (xSemaphoreTake(rpmMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
      cur = currentRpm;
      xSemaphoreGive(rpmMutex);
    }

    if (isStaConnected()) {
      doc["wifi"] = true;
      doc["mode"] = "STA";
      doc["ip"] = WiFi.localIP().toString();
      doc["ssid"] = WiFi.SSID();
      doc["rssi"] = WiFi.RSSI();
      doc["currentRpm"] = cur;
    } else {
      doc["wifi"] = false;
      doc["mode"] = "AP";
      doc["ip_ap"] = WiFi.softAPIP().toString();
      doc["ssid"] = "";
      doc["rssi"] = nullptr;
      doc["currentRpm"] = 0.0;
    }

    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  server.on("/rpm", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("value")) {
      float val = request->getParam("value")->value().toFloat();
      if (val < 0) val = 0;
      if (val > MAX_RPM) val = MAX_RPM;
      if (xSemaphoreTake(rpmMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        targetRpm = val;
        xSemaphoreGive(rpmMutex);
      }
      if (val <= 1.0f) stop_motor_hard(false);
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Missing value");
    }
  });

  server.on("/stop", HTTP_POST, [](AsyncWebServerRequest *request) {
    stop_motor_hard(false);
    request->send(200, "application/json", "{\"status\":\"stopped\"}");
  });

  server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (g_startScan) {
      // Si ya hay un escaneo en curso, no hacer nada
      request->send(200, "application/json", "{\"status\":\"scanning\"}");
      return;
    }

    // Iniciar un nuevo escaneo
    g_startScan = true;
    if (xSemaphoreTake(g_scanMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      // Devolver los resultados anteriores mientras se escanean los nuevos
      String currentResults = g_scanResults;
      xSemaphoreGive(g_scanMutex);
      request->send(200, "application/json", currentResults);
    } else {
      request->send(503, "application/json", "{\"status\":\"busy\"}");
    }
  });

  server.on("/saveWifi", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
      String ssid = request->getParam("ssid", true)->value();
      String password = request->getParam("password", true)->value();
      StaticJsonDocument<256> doc;
      doc["ssid"] = ssid;
      doc["password"] = password;
      File configFile = LittleFS.open("/wifiConfig.json", "w");
      serializeJson(doc, configFile);
      configFile.close();
      String response = "{\"status\":\"ok\", \"message\":\"Guardado. El dispositivo se reiniciara. Busque la nueva IP en la pantalla del BioShaker.\"}";
      request->send(200, "application/json", response);
      delay(2000); // Aumentar el retraso para dar tiempo al navegador a mostrar el mensaje
      ESP.restart();
    } else {
      request->send(400, "application/json", "{\"status\":\"error\",\"msg\":\"missing params\"}");
    }
  });

  server.serveStatic("/", LittleFS, "/");
  server.begin();
}

/**
 * @brief WiFi event handler.
 */
void on_wifi_event(WiFiEvent_t event) {
  uiForceRedraw = true;
  switch (event) {
    case SYSTEM_EVENT_STA_GOT_IP:
      g_offlineRequested = false;
      uiState = UI_NORMAL;
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      if (!g_offlineRequested) {
        uiState = UI_WIFI_DISCONNECTED;
        tryConnectSavedWifi(true);
      }
      break;
    default:
      break;
  }
}

/**
 * @brief Tarea de FreeRTOS que gestiona el escaneo de redes WiFi en segundo plano.
 */
void wifi_scan_task(void *parameter) {
  while (true) {
    if (g_startScan) {
      g_startScan = false;
      int n = WiFi.scanNetworks();

      StaticJsonDocument<1024> doc;
      JsonArray redes = doc.to<JsonArray>();
      for (int i = 0; i < n; ++i) {
        redes.add(WiFi.SSID(i));
      }

      if (xSemaphoreTake(g_scanMutex, portMAX_DELAY) == pdTRUE) {
        serializeJson(redes, g_scanResults);
        xSemaphoreGive(g_scanMutex);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
