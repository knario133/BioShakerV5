#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include <FastAccelStepper.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32RotaryEncoder.h>
#include <Wire.h>
#include <WiFi.h>
#include <math.h>

// ============================
// Firmware info
// ============================
#define FIRMWARE_VERSION "1.2.2-NO-PID-STATUSFIX-APBLINK"

// ============================
// Pines
// ============================
#define DIR_PIN 27
#define STEP_PIN 26
#define ENABLE_PIN 25
#define I2C_SDA 21
#define I2C_SCL 22
#define ENC_CLK 5
#define ENC_DT 18
#define ENC_SW 19

// ============================
// Motor / Calibración
// ============================
// Ajuste para corregir sobrevelocidad observada (~+10%)
const double SPR_CMD  = 3200; // steps/vuelta (comando RPM->SPS)
const double SPR_MEAS = 3659; // steps/vuelta (medición SPS->RPM)
const float  MAX_RPM  = 510.0f;

inline double rpm2sps(double rpm) { return (rpm / 60.0) * SPR_CMD; }

// ============================
// Instancias
// ============================
FastAccelStepperEngine engine;
FastAccelStepper *stepper = NULL;
RotaryEncoder rotaryEncoder(ENC_DT, ENC_CLK, ENC_SW);
LiquidCrystal_I2C lcd(0x27, 16, 2);
AsyncWebServer server(80);

// ============================
// Variables compartidas
// ============================
float targetRpm  = 0.0f;
float currentRpm = 0.0f;
SemaphoreHandle_t rpmMutex;

// ============================
// UI
// ============================
enum UiState { 
  UI_SPLASH, UI_NORMAL, UI_MENU, UI_ADJUST_RPM, UI_WIFI, UI_LANGUAGE, UI_AP_MODE,
  UI_WIFI_DISCONNECTED
};
UiState uiState = UI_SPLASH;
int language = 0; // 0: Español, 1: English
int menuIndex = 0;
volatile bool uiForceRedraw = true;

// ============================
// Aux estados/calculo RPM
// ============================
volatile long KnobValue = 0;
static volatile bool g_resetRpmEstimator = false;
static volatile bool g_offlineRequested  = false;

// ============================
// Rampa 0→60 RPM en 6s (sin PID)
// ============================
// a_cmd = SPR_CMD / 6 steps/s^2  (60 RPM = SPR_CMD steps/s)
const double A_CMD   = SPR_CMD / 6.0; // ≈ 969.7 sps^2 con SPR_CMD=5818
const double LOOP_DT = 0.04;          // motorTask ~40 ms
static double cmdSPS = 0.0;           // velocidad comandada (steps/s)

// ============================
// PROTOTIPOS
// ============================
void stopMotorHard(bool fromUI = false);
void startAPAlways();
void tryConnectSavedWifi(bool asyncRetry);
void goOffline();
bool isStaConnected();
void onWifiEvent(WiFiEvent_t event);

// ===========================================================================
// Utilidades
// ===========================================================================
bool isStaConnected() { return WiFi.status() == WL_CONNECTED; }

// ===========================================================================
// UI Handlers
// ===========================================================================
void handleSplash() {
  static uint32_t t0 = 0;
  if (t0 == 0) {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("BIOSHAKER");
    lcd.setCursor(0, 1); lcd.print("v" FIRMWARE_VERSION);
    t0 = millis();
  }
  if ((millis() - t0 > 1200) || rotaryEncoder.buttonPressed()) {
    t0 = 0; uiState = UI_NORMAL; uiForceRedraw = true;
  }
}

void handleNormal() {
  static String lastLine0 = ""; static uint32_t lastRefresh = 0;
  String l0; static bool blink = false; static uint32_t lastBlink = 0;
  bool apOn = (WiFi.getMode() & WIFI_AP); bool staOn = isStaConnected();

  if (staOn) l0 = WiFi.localIP().toString();
  else if (apOn) {
    if (millis() - lastBlink >= 800) { blink = !blink; lastBlink = millis(); }
    l0 = blink ? (language==0 ? "MODO AP" : "AP MODE") : WiFi.softAPIP().toString();
  } else l0 = (language==0)?"Sin WiFi":"No WiFi";

  bool timeToRefresh = (millis() - lastRefresh) >= 300;
  if (l0 != lastLine0 || uiForceRedraw || timeToRefresh) {
    lcd.setCursor(0,0); char line[17]; snprintf(line,sizeof(line),"%-16s", l0.c_str()); lcd.print(line);
    lastLine0 = l0; lastRefresh = millis(); uiForceRedraw = false;
  }

  float cur=0, tgt=0;
  if (xSemaphoreTake(rpmMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    cur = currentRpm; tgt = targetRpm; xSemaphoreGive(rpmMutex);
  }
  char buf[17]; snprintf(buf,sizeof(buf),"A:%3.0f T:%3.0f", cur, tgt);
  static char lastRpm[17]="";
  if (strcmp(buf,lastRpm)!=0) {
    lcd.setCursor(0,1); char l2[17]; snprintf(l2,sizeof(l2),"%-16s", buf); lcd.print(l2);
    strcpy(lastRpm, buf);
  }
}

void handleMenu() {
  static int currentMenuIndex = -1;
  const char* labelConnectSaved = (language==0) ? "Conectar WiFi (guardada)" : "Connect saved WiFi";
  const char* labelDisconnect   = (language==0) ? "Desconectar WiFi"         : "Disconnect WiFi";
  const char* label3 = (isStaConnected() || (WiFi.getMode() & WIFI_AP)) ? labelDisconnect : labelConnectSaved;

  const char* items[][2] = {
    {"Ajustar RPM","Adjust RPM"}, {"Detener Motor","Stop Motor"}, {"Configurar WiFi","WiFi Setup"},
    {label3, label3}, {"Idioma","Language"}, {"Volver","Back"}
  };
  const int menuCount = sizeof(items)/sizeof(items[0]);

  if (menuIndex != currentMenuIndex || uiForceRedraw) {
    int top = (menuIndex/2)*2;
    for (int i=0;i<2;++i) {
      int idx = top+i; char lineBuf[17];
      if (idx < menuCount) {
        const char* txt = items[idx][language];
        snprintf(lineBuf,sizeof(lineBuf),"%c%s",(idx==menuIndex?'>':' '), txt);
      } else lineBuf[0]='\0';
      lcd.setCursor(0,i); char padded[17]; snprintf(padded,sizeof(padded),"%-16s", lineBuf); lcd.print(padded);
    }
    currentMenuIndex = menuIndex; uiForceRedraw = false;
  }
}

void handleAdjustRpm() {
  if (uiForceRedraw) { lcd.clear(); lcd.setCursor(0,0); lcd.print((language==0)?"Ajustar RPM":"Adjust RPM"); uiForceRedraw=false; }
  float newTarget=0; if (xSemaphoreTake(rpmMutex,pdMS_TO_TICKS(5))==pdTRUE){ newTarget=targetRpm; xSemaphoreGive(rpmMutex); }
  lcd.setCursor(0,1); char buf[17]; snprintf(buf,sizeof(buf),"RPM: %.0f      ", newTarget); lcd.print(buf);
}

// AP fijo (no se usa mucho ya, pero lo dejo por compatibilidad)
void handleApMode() {
  if (uiForceRedraw) { lcd.clear(); uiForceRedraw=false; }
  float cur=0,tgt=0; if (xSemaphoreTake(rpmMutex,pdMS_TO_TICKS(5))==pdTRUE){cur=currentRpm;tgt=targetRpm;xSemaphoreGive(rpmMutex);}
  lcd.setCursor(0,0); {const char* t=(language==0)?"MODO AP":"AP MODE"; char l0[17]; snprintf(l0,sizeof(l0),"%-16s",t); lcd.print(l0);}
  lcd.setCursor(0,1); {char l1[17]; snprintf(l1,sizeof(l1),"A:%3.0f T:%3.0f",cur,tgt); char pad[17]; snprintf(pad,sizeof(pad),"%-16s",l1); lcd.print(pad);}
}

void handleLanguage() {
  if (uiForceRedraw) { lcd.clear(); lcd.setCursor(0,0); lcd.print("Idioma/Language  "); uiForceRedraw=false; }
  lcd.setCursor(0,1); char buf[17]; if (language==0) strncpy(buf,">Esp  En        ",sizeof(buf)); else strncpy(buf," Esp >En        ",sizeof(buf)); buf[16]='\0'; lcd.print(buf);
}

// Desconectado o offline manual: sigue mostrando A/T abajo
void handleWifiDisconnected() {
  static uint32_t lastRefresh=0;
  if (uiForceRedraw) { lcd.clear(); uiForceRedraw=false; }
  lcd.setCursor(0,0); { const char* t=(language==0)?(g_offlineRequested?"Sin WiFi":"WiFi Perdido"):(g_offlineRequested?"No WiFi":"WiFi Lost"); char l1[17]; snprintf(l1,sizeof(l1),"%-16s",t); lcd.print(l1); }
  if (millis()-lastRefresh>=250) {
    float cur=0,tgt=0; if (xSemaphoreTake(rpmMutex,pdMS_TO_TICKS(5))==pdTRUE){cur=currentRpm;tgt=targetRpm;xSemaphoreGive(rpmMutex);}
    lcd.setCursor(0,1); char l2[17]; snprintf(l2,sizeof(l2),"A:%3.0f T:%3.0f",cur,tgt); char pad[17]; snprintf(pad,sizeof(pad),"%-16s",l2); lcd.print(pad); lastRefresh=millis();
  }
}

// ======== AHORA PARPADEA EN CONFIGURAR WIFI ========
void handleWifi() {
  static uint32_t lastScanTime=0; static int networkCount=-1; static int selectedNetworkIndex=0;
  static bool blink=false; static uint32_t lastBlink=0;

  if (uiForceRedraw) { lcd.clear(); uiForceRedraw=false; networkCount=-1; }

  // Línea 0: parpadeo entre "MODO AP" y la IP del AP
  if (millis()-lastBlink >= 800) { blink = !blink; lastBlink = millis(); }
  String l0 = blink ? (language==0 ? "MODO AP" : "AP MODE") : WiFi.softAPIP().toString();
  lcd.setCursor(0,0); char line0[17]; snprintf(line0,sizeof(line0),"%-16s", l0.c_str()); lcd.print(line0);

  // Escaneo cada 10 s (no bloquea /status ni la UI)
  if (networkCount<0 || millis()-lastScanTime>10000) {
    networkCount = WiFi.scanNetworks();
    lastScanTime = millis();
    selectedNetworkIndex = 0;
  }

  // Línea 1: muestra conteo y SSID seleccionado (simple)
  lcd.setCursor(0,1);
  if (networkCount > 0) {
    char ssidLine[17];
    snprintf(ssidLine,sizeof(ssidLine),"%d/%d %s",
             selectedNetworkIndex+1, networkCount,
             WiFi.SSID(selectedNetworkIndex).c_str());
    lcd.print(ssidLine);
  } else {
    lcd.print("No networks found");
  }
}

// ===========================================================================
// FreeRTOS Tasks
// ===========================================================================
void IRAM_ATTR knobCallback(long value) { KnobValue = -value; rotaryEncoder.resetEncoderValue(); }

void uiTask(void *parameter) {
  static uint32_t lastRpmCalc=0; static long lastStepperPos=0; static float smoothedRpm=0.0f;

  while (true) {
    long delta=0; if (KnobValue!=0){ delta=KnobValue; KnobValue=0; uiForceRedraw=true; }
    if (delta!=0) {
      if (xSemaphoreTake(rpmMutex,pdMS_TO_TICKS(5))==pdTRUE) {
        if (uiState==UI_ADJUST_RPM){ targetRpm += delta; if (targetRpm<0) targetRpm=0; if (targetRpm>MAX_RPM) targetRpm=MAX_RPM; }
        else if (uiState==UI_MENU){ const int menuCount=6; menuIndex+=delta; if (menuIndex<0) menuIndex=menuCount-1; if (menuIndex>=menuCount) menuIndex=0; }
        else if (uiState==UI_LANGUAGE){ language=(language+delta)%2; if (language<0) language=1; }
        xSemaphoreGive(rpmMutex);
      }
    }

    static uint32_t lastBtn=0;
    if (rotaryEncoder.buttonPressed() && (millis()-lastBtn>200)) {
      lastBtn=millis(); uiForceRedraw=true; lcd.clear();
      switch (uiState) {
        case UI_SPLASH: uiState=UI_NORMAL; break;
        case UI_NORMAL: uiState=UI_MENU; menuIndex=0; break;
        case UI_WIFI_DISCONNECTED: uiState=UI_MENU; menuIndex=0; break;
        case UI_MENU:
          switch (menuIndex) {
            case 0: uiState=UI_ADJUST_RPM; break;
            case 1: stopMotorHard(true); uiState=UI_NORMAL; break;
            case 2: startAPAlways(); uiState=UI_WIFI; break; // <-- entra a pantalla con parpadeo
            case 3:
              if (isStaConnected() || (WiFi.getMode() & WIFI_AP)) { goOffline(); uiState=UI_WIFI_DISCONNECTED; }
              else { g_offlineRequested=false; WiFi.mode(WIFI_STA); tryConnectSavedWifi(false); uiState = isStaConnected()?UI_NORMAL:UI_WIFI_DISCONNECTED; }
              break;
            case 4: uiState=UI_LANGUAGE; break;
            case 5: uiState=UI_NORMAL; break;
          } break;
        case UI_ADJUST_RPM: case UI_AP_MODE: case UI_LANGUAGE: case UI_WIFI: uiState=UI_NORMAL; break;
      }
    }

    switch (uiState) {
      case UI_SPLASH: handleSplash(); break;
      case UI_NORMAL: handleNormal(); break;
      case UI_MENU: handleMenu(); break;
      case UI_ADJUST_RPM: handleAdjustRpm(); break;
      case UI_AP_MODE: handleApMode(); break;
      case UI_LANGUAGE: handleLanguage(); break;
      case UI_WIFI: handleWifi(); break;
      case UI_WIFI_DISCONNECTED: handleWifiDisconnected(); break;
    }

    // --- Medición de RPM usando SPR_MEAS calibrado ---
    if (g_resetRpmEstimator) {
      long posNow = stepper ? stepper->getCurrentPosition() : 0;
      lastStepperPos = posNow; smoothedRpm=0.0f;
      if (xSemaphoreTake(rpmMutex,pdMS_TO_TICKS(5))==pdTRUE){ currentRpm=0.0f; xSemaphoreGive(rpmMutex); }
      g_resetRpmEstimator=false;
    }
    if (millis()-lastRpmCalc >= 300) {
      lastRpmCalc = millis();
      long pos = stepper ? stepper->getCurrentPosition() : 0;
      // rpm = deltaSteps / SPR_MEAS * (60/dt); con dt=0.3 => factor 200
      float rpm = ((pos - lastStepperPos) / (float)SPR_MEAS) * 200.0f;
      lastStepperPos = pos;
      smoothedRpm = 0.35f * rpm + 0.65f * smoothedRpm;
      if (xSemaphoreTake(rpmMutex,pdMS_TO_TICKS(5))==pdTRUE){ currentRpm = smoothedRpm; xSemaphoreGive(rpmMutex); }
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ===============================
// Tarea del motor (rampa abierta)
// ===============================
/*void motorTask(void *parameter) {
  while (true) {
    double sp_rpm;
    if (xSemaphoreTake(rpmMutex,pdMS_TO_TICKS(5))==pdTRUE) {
      sp_rpm = (double)targetRpm;
      xSemaphoreGive(rpmMutex);
    }

    if (sp_rpm <= 1.0) {
      cmdSPS = 0.0;
      if (stepper){ if (stepper->isRunningContinuously()) stepper->stopMove(); stepper->disableOutputs(); }
    } else {
      // Rampa con aceleración A_CMD (con SPR calibrado)
      double targetSPS = rpm2sps(sp_rpm);
      if (cmdSPS < targetSPS)      cmdSPS = min(cmdSPS + A_CMD * LOOP_DT, targetSPS);
      else if (cmdSPS > targetSPS) cmdSPS = max(cmdSPS - A_CMD * LOOP_DT, targetSPS);

      if (stepper) {
        // Aceleración del driver suficientemente alta para no limitar la rampa
        stepper->setAcceleration(20000);
        stepper->setSpeedInHz((float)cmdSPS);
        if (!stepper->isRunningContinuously()) { stepper->enableOutputs(); stepper->runForward(); }
        else { stepper->applySpeedAcceleration(); }
      }
    }
    vTaskDelay(pdMS_TO_TICKS((int)(LOOP_DT*1000)));
  }
}*/
// ===============================
// Tarea del motor (rampa gestionada por la librería)
// ===============================
void motorTask(void *parameter) {
  while (true) {
    double sp_rpm;
    if (xSemaphoreTake(rpmMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
      sp_rpm = (double)targetRpm;
      xSemaphoreGive(rpmMutex);
    }

    if (stepper) {
      if (sp_rpm < 1.0) {
        // << CAMBIO: Lógica de parada simplificada
        if (stepper->isRunningContinuously()) {
          stepper->stopMove();
          stepper->disableOutputs();
        }
      } else {
        // << CAMBIO: Lógica de control delegada a la librería
        double targetSPS = rpm2sps(sp_rpm);

        // 1. Definimos la aceleración (la rampa que queremos)
        stepper->setAcceleration((float)A_CMD);
        
        // 2. Definimos la velocidad objetivo
        stepper->setSpeedInHz((float)targetSPS);

        // 3. Nos aseguramos de que el motor esté encendido y aplicando los cambios
        if (!stepper->isRunningContinuously()) {
          stepper->enableOutputs();
          stepper->runForward();
        } else {
          // Si ya se está moviendo, aplica la nueva velocidad/aceleración
          stepper->applySpeedAcceleration(); 
        }
      }
    }
    
    // El delay puede ser un poco más largo, ya que no calculamos la rampa manualmente
    vTaskDelay(pdMS_TO_TICKS(50)); 
  }
}

// ===========================================================================
// Servidor Web
// ===========================================================================
void sendRoot(AsyncWebServerRequest *request) {
  if (LittleFS.exists("/index.html")) request->send(LittleFS, "/index.html", "text/html");
  else {
    String html = "<html><head><meta name='viewport' content='width=device-width,initial-scale=1'/>"
                  "<title>BioShaker</title></head><body>";
    html += "<h2>No index.html</h2><h3>Archivos:</h3><ul>";
    File root = LittleFS.open("/"); File file = root.openNextFile();
    while (file) { html += "<li><a href='" + String(file.name()) + "'>" + String(file.name()) + "</a> (" + String(file.size()) + " bytes)</li>"; file = root.openNextFile(); }
    html += "</ul></body></html>";
    request->send(200, "text/html", html);
  }
}

void setupServer() {
  server.on("/", HTTP_GET, sendRoot);

  // ======== /status: seguro y EXACTO a tu formato ========
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
    // Usamos StaticJsonDocument para evitar heap/fragmentación que puede causar resets
    StaticJsonDocument<512> doc;

    const bool sta = (WiFi.status() == WL_CONNECTED);
    const bool ap  = (WiFi.getMode() & WIFI_AP);

    float cur = 0.0f;
    if (xSemaphoreTake(rpmMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
      cur = currentRpm;
      xSemaphoreGive(rpmMutex);
    }

    if (sta) {
      // STA conectado
      doc["wifi"] = true;
      doc["mode"] = "STA";
      doc["ip"]   = WiFi.localIP().toString();
      // Proteger llamadas: SSID/RSSI válidos solo en STA
      String ssid = WiFi.SSID();  // seguro en STA
      doc["ssid"] = ssid;
      doc["rssi"] = WiFi.RSSI();
      doc["currentRpm"] = cur;
    } else {
      // AP o desconectado
      doc["wifi"] = false;
      doc["mode"] = "AP";
      doc["ip_ap"] = WiFi.softAPIP().toString();
      doc["ssid"]  = "";
      doc["rssi"]  = nullptr;     // null
      doc["currentRpm"] = 0.0;
    }

    String json; serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  server.on("/rpm", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("value")) {
      float val = request->getParam("value")->value().toFloat();
      if (val < 0) val = 0; if (val > MAX_RPM) val = MAX_RPM;
      if (xSemaphoreTake(rpmMutex,pdMS_TO_TICKS(5))==pdTRUE){ targetRpm=val; xSemaphoreGive(rpmMutex); }
      if (val <= 1.0f) stopMotorHard(false);
      request->send(200, "text/plain", "OK");
    } else request->send(400, "text/plain", "Missing value");
  });

  server.on("/stop", HTTP_POST, [](AsyncWebServerRequest *request){
    stopMotorHard(false);
    request->send(200, "application/json", "{\"status\":\"stopped\"}");
  });

  server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request){
    int n = WiFi.scanNetworks(); StaticJsonDocument<1024> doc; JsonArray redes = doc.to<JsonArray>();
    for (int i=0;i<n;++i) redes.add(WiFi.SSID(i));
    String response; serializeJson(redes, response);
    request->send(200, "application/json", response);
  });

  server.on("/saveWifi", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
      String ssid = request->getParam("ssid", true)->value();
      String password = request->getParam("password", true)->value();
      StaticJsonDocument<256> doc; doc["ssid"]=ssid; doc["password"]=password;
      File configFile = LittleFS.open("/wifiConfig.json", "w"); serializeJson(doc, configFile); configFile.close();
      request->send(200, "application/json", "{\"status\":\"ok\"}"); delay(500); ESP.restart();
    } else request->send(400, "application/json", "{\"status\":\"error\",\"msg\":\"missing params\"}");
  });

  server.serveStatic("/", LittleFS, "/");
  server.begin();
}

// ===========================================================================
// WiFi
// ===========================================================================
void onWifiEvent(WiFiEvent_t event) {
  uiForceRedraw = true;
  switch (event) {
    case SYSTEM_EVENT_STA_GOT_IP:
      g_offlineRequested=false; uiState=UI_NORMAL;
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      if (!g_offlineRequested) { uiState=UI_WIFI_DISCONNECTED; tryConnectSavedWifi(true); }
      break;
    default: break;
  }
}

void startAPAlways() {
  WiFi.mode(WIFI_AP_STA);
  String ssid = String("BioShaker_") + String((uint32_t)ESP.getEfuseMac(), HEX).substring(4);
  IPAddress apIP(192,168,4,1), gw(192,168,4,1), mask(255,255,255,0);
  WiFi.softAPdisconnect(true); delay(30);
  WiFi.softAPConfig(apIP, gw, mask);
  WiFi.softAP(ssid.c_str(), "bioshaker", 6, 0, 4);
  delay(50);
  uiState = UI_AP_MODE; uiForceRedraw = true;
}

void tryConnectSavedWifi(bool asyncRetry) {
  File f = LittleFS.open("/wifiConfig.json","r"); if (!f) return;
  StaticJsonDocument<256> doc; if (deserializeJson(doc, f)) { f.close(); return; } f.close();
  String ssid = doc["ssid"] | ""; String password = doc["password"] | ""; if (ssid.length()==0) return;
  WiFi.begin(ssid.c_str(), password.c_str());
  if (!asyncRetry) {
    unsigned long t = millis(); while (WiFi.status()!=WL_CONNECTED && millis()-t<20000) delay(250);
  }
}

void goOffline() { 
  g_offlineRequested=true; 
  WiFi.softAPdisconnect(true); 
  WiFi.disconnect(true,true); 
  WiFi.mode(WIFI_OFF); 
  uiForceRedraw=true; 
}

// ===========================================================================
// Control de motor utilitario
// ===========================================================================
/*void stopMotorHard(bool fromUI) {
  if (xSemaphoreTake(rpmMutex,pdMS_TO_TICKS(10))==pdTRUE){ targetRpm=0.0f; currentRpm=0.0f; xSemaphoreGive(rpmMutex); }
  cmdSPS=0.0;
  if (stepper){ stepper->stopMove(); stepper->disableOutputs(); stepper->forceStopAndNewPosition(stepper->getCurrentPosition()); }
  g_resetRpmEstimator=true;
  if (fromUI) uiForceRedraw=true;
}*/
void stopMotorHard(bool fromUI) {
  if (xSemaphoreTake(rpmMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    targetRpm = 0.0f;
    currentRpm = 0.0f;
    xSemaphoreGive(rpmMutex);
  }
  cmdSPS = 0.0;
  if (stepper) {
    stepper->stopMove();
    stepper->disableOutputs();
    // stepper->forceStopAndNewPosition(stepper->getCurrentPosition()); // << CAMBIO: Eliminada esta línea problemática
  }
  g_resetRpmEstimator = true;
  if (fromUI) uiForceRedraw = true;
}

// ===========================================================================
// Setup / Loop
// ===========================================================================
void setup() {
  Serial.begin(115200); delay(80);
  if (!LittleFS.begin()) Serial.println("LittleFS mount failed");

  Wire.begin(I2C_SDA, I2C_SCL); lcd.init(); lcd.backlight();
  rotaryEncoder.begin(); rotaryEncoder.setBoundaries(-1000000,1000000,false); rotaryEncoder.onTurned(knobCallback);
  pinMode(ENC_SW, INPUT_PULLUP);

  engine.init();
  stepper = engine.stepperConnectToPin(STEP_PIN);
  if (stepper) {
    stepper->setDirectionPin(DIR_PIN);
    stepper->setEnablePin(ENABLE_PIN);
    stepper->setAutoEnable(true);
    // Aceleración del driver >> A_CMD (≈ 970) para no limitar la rampa
    stepper->setAcceleration(20000);
  }

  rpmMutex = xSemaphoreCreateMutex();

  // WiFi
  WiFi.onEvent(onWifiEvent);
  startAPAlways();
  tryConnectSavedWifi(false);

  setupServer();

  xTaskCreatePinnedToCore(uiTask, "uiTask", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(motorTask, "motorTask", 4096, NULL, 2, NULL, 1);
}

void loop() {
  if ((uiState==UI_NORMAL) && (WiFi.getMode()!=WIFI_AP) && (WiFi.status()!=WL_CONNECTED) && !g_offlineRequested) {
    uiState = UI_WIFI_DISCONNECTED; uiForceRedraw = true;
  }
  delay(800);
}
