#include "config.h"
#include "ui_manager.h"
#include "motor_control.h"
#include "wifi_manager.h"
#include <LiquidCrystal_I2C.h>
#include <ESP32RotaryEncoder.h>
#include <WiFi.h>

// ============================
// Pines
// ============================
#define I2C_SDA 21
#define I2C_SCL 22
#define ENC_CLK 5
#define ENC_DT 18
#define ENC_SW 19

// ============================
// Constantes de la UI
// ============================
const uint32_t BUTTON_DEBOUNCE_MS = 200;
const uint32_t SPLASH_SCREEN_DURATION_MS = 1200;
const uint32_t BLINK_INTERVAL_MS = 800;
const uint32_t NORMAL_SCREEN_REFRESH_MS = 300;
const uint32_t WIFI_SCAN_INTERVAL_MS = 10000;
const uint32_t RPM_CALCULATION_INTERVAL_MS = 300;

// Extern variables
extern float targetRpm;
extern float currentRpm;
extern SemaphoreHandle_t rpmMutex;
extern FastAccelStepper *stepper;

// Instancias de hardware
RotaryEncoder rotaryEncoder(ENC_DT, ENC_CLK, ENC_SW);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Estado de la UI
UiState uiState = UI_SPLASH;
int language = 0; // 0: Español, 1: English
int menuIndex = 0;
volatile bool uiForceRedraw = true;

// Variables auxiliares
volatile long KnobValue = 0;
volatile bool g_resetRpmEstimator = false;
volatile bool g_offlineRequested  = false;

// Prototipos locales
void handle_splash();
void handle_normal();
void handle_menu();
void handle_adjust_rpm();
void handle_ap_mode();
void handle_language();
void handle_wifi_disconnected();
void handle_wifi();
void IRAM_ATTR knob_callback(long value);

/**
 * @brief Initializes the UI components (LCD and rotary encoder).
 */
void ui_setup() {
    Wire.begin(I2C_SDA, I2C_SCL);
    lcd.init();
    lcd.backlight();
    rotaryEncoder.begin();
    rotaryEncoder.setBoundaries(-1000000, 1000000, false);
    rotaryEncoder.onTurned(knob_callback);
    pinMode(ENC_SW, INPUT_PULLUP);
}

/**
 * @brief FreeRTOS task to manage the user interface.
 */
void ui_task(void *parameter) {
  static uint32_t lastRpmCalc = 0;
  static long lastStepperPos = 0;
  static float smoothedRpm = 0.0f;

  while (true) {
    // Handle rotary encoder input
    long delta = 0;
    if (KnobValue != 0) {
      delta = KnobValue;
      KnobValue = 0;
      uiForceRedraw = true;
    }
    if (delta != 0) {
      if (xSemaphoreTake(rpmMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        if (uiState == UI_ADJUST_RPM) {
          targetRpm += delta;
          if (targetRpm < 0) targetRpm = 0;
          if (targetRpm > MAX_RPM) targetRpm = MAX_RPM;
        } else if (uiState == UI_MENU) {
          const int menuCount = 6;
          menuIndex += delta;
          if (menuIndex < 0) menuIndex = menuCount - 1;
          if (menuIndex >= menuCount) menuIndex = 0;
        } else if (uiState == UI_LANGUAGE) {
          language = (language + delta) % 2;
          if (language < 0) language = 1;
        }
        xSemaphoreGive(rpmMutex);
      }
    }

    // Handle button presses
    static uint32_t lastBtn = 0;
    if (rotaryEncoder.buttonPressed() && (millis() - lastBtn > BUTTON_DEBOUNCE_MS)) {
      lastBtn = millis();
      uiForceRedraw = true;
      lcd.clear();
      switch (uiState) {
        case UI_SPLASH: uiState = UI_NORMAL; break;
        case UI_NORMAL: uiState = UI_MENU; menuIndex = 0; break;
        case UI_WIFI_DISCONNECTED: uiState = UI_MENU; menuIndex = 0; break;
        case UI_MENU:
          switch (menuIndex) {
            case 0: uiState = UI_ADJUST_RPM; break;
            case 1: stop_motor_hard(true); uiState = UI_NORMAL; break;
            case 2: startAPAlways(); uiState = UI_WIFI; break;
            case 3:
              if (isStaConnected() || (WiFi.getMode() & WIFI_AP)) {
                goOffline();
                uiState = UI_WIFI_DISCONNECTED;
              } else {
                g_offlineRequested = false;
                WiFi.mode(WIFI_STA);
                tryConnectSavedWifi(false);
                uiState = isStaConnected() ? UI_NORMAL : UI_WIFI_DISCONNECTED;
              }
              break;
            case 4: uiState = UI_LANGUAGE; break;
            case 5: uiState = UI_NORMAL; break;
          }
          break;
        case UI_ADJUST_RPM: case UI_AP_MODE: case UI_LANGUAGE: case UI_WIFI:
          uiState = UI_NORMAL;
          break;
      }
    }

    // Update the display based on the current state
    switch (uiState) {
      case UI_SPLASH: handle_splash(); break;
      case UI_NORMAL: handle_normal(); break;
      case UI_MENU: handle_menu(); break;
      case UI_ADJUST_RPM: handle_adjust_rpm(); break;
      case UI_AP_MODE: handle_ap_mode(); break;
      case UI_LANGUAGE: handle_language(); break;
      case UI_WIFI: handle_wifi(); break;
      case UI_WIFI_DISCONNECTED: handle_wifi_disconnected(); break;
    }

    // RPM Calculation
    if (g_resetRpmEstimator) {
      lastStepperPos = stepper ? stepper->getCurrentPosition() : 0;
      smoothedRpm = 0.0f;
      if (xSemaphoreTake(rpmMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        currentRpm = 0.0f;
        xSemaphoreGive(rpmMutex);
      }
      g_resetRpmEstimator = false;
    }
    if (millis() - lastRpmCalc >= RPM_CALCULATION_INTERVAL_MS) {
      lastRpmCalc = millis();
      long pos = stepper ? stepper->getCurrentPosition() : 0;
      float rpm = ((pos - lastStepperPos) / (float)SPR_MEAS) * (60000.0f / RPM_CALCULATION_INTERVAL_MS);
      lastStepperPos = pos;
      smoothedRpm = 0.35f * rpm + 0.65f * smoothedRpm;
      if (xSemaphoreTake(rpmMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        currentRpm = smoothedRpm;
        xSemaphoreGive(rpmMutex);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void handle_splash() {
  static uint32_t t0 = 0;
  if (t0 == 0) {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("BIOSHAKER");
    lcd.setCursor(0, 1); lcd.print("v" FIRMWARE_VERSION);
    t0 = millis();
  }
  if ((millis() - t0 > SPLASH_SCREEN_DURATION_MS) || rotaryEncoder.buttonPressed()) {
    t0 = 0; uiState = UI_NORMAL; uiForceRedraw = true;
  }
}

void handle_normal() {
  static String lastLine0 = ""; static uint32_t lastRefresh = 0;
  String l0; static bool blink = false; static uint32_t lastBlink = 0;
  bool apOn = (WiFi.getMode() & WIFI_AP); bool staOn = isStaConnected();

  if (staOn) l0 = WiFi.localIP().toString();
  else if (apOn) {
    if (millis() - lastBlink >= BLINK_INTERVAL_MS) { blink = !blink; lastBlink = millis(); }
    l0 = blink ? (language==0 ? "MODO AP" : "AP MODE") : WiFi.softAPIP().toString();
  } else l0 = (language==0)?"Sin WiFi":"No WiFi";

  bool timeToRefresh = (millis() - lastRefresh) >= NORMAL_SCREEN_REFRESH_MS;
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

void handle_menu() {
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

void handle_adjust_rpm() {
  if (uiForceRedraw) { lcd.clear(); lcd.setCursor(0,0); lcd.print((language==0)?"Ajustar RPM":"Adjust RPM"); uiForceRedraw=false; }
  float newTarget=0; if (xSemaphoreTake(rpmMutex,pdMS_TO_TICKS(5))==pdTRUE){ newTarget=targetRpm; xSemaphoreGive(rpmMutex); }
  lcd.setCursor(0,1); char buf[17]; snprintf(buf,sizeof(buf),"RPM: %.0f      ", newTarget); lcd.print(buf);
}

void handle_ap_mode() {
  if (uiForceRedraw) { lcd.clear(); uiForceRedraw=false; }
  float cur=0,tgt=0; if (xSemaphoreTake(rpmMutex,pdMS_TO_TICKS(5))==pdTRUE){cur=currentRpm;tgt=targetRpm;xSemaphoreGive(rpmMutex);}
  lcd.setCursor(0,0); {const char* t=(language==0)?"MODO AP":"AP MODE"; char l0[17]; snprintf(l0,sizeof(l0),"%-16s",t); lcd.print(l0);}
  lcd.setCursor(0,1); {char l1[17]; snprintf(l1,sizeof(l1),"A:%3.0f T:%3.0f",cur,tgt); char pad[17]; snprintf(pad,sizeof(pad),"%-16s",l1); lcd.print(pad);}
}

void handle_language() {
  if (uiForceRedraw) { lcd.clear(); lcd.setCursor(0,0); lcd.print("Idioma/Language  "); uiForceRedraw=false; }
  lcd.setCursor(0,1); char buf[17]; if (language==0) strncpy(buf,">Esp  En        ",sizeof(buf)); else strncpy(buf," Esp >En        ",sizeof(buf)); buf[16]='\0'; lcd.print(buf);
}

void handle_wifi_disconnected() {
  static uint32_t lastRefresh=0;
  if (uiForceRedraw) { lcd.clear(); uiForceRedraw=false; }
  lcd.setCursor(0,0); { const char* t=(language==0)?(g_offlineRequested?"Sin WiFi":"WiFi Perdido"):(g_offlineRequested?"No WiFi":"WiFi Lost"); char l1[17]; snprintf(l1,sizeof(l1),"%-16s",t); lcd.print(l1); }
  if (millis()-lastRefresh>=250) {
    float cur=0,tgt=0; if (xSemaphoreTake(rpmMutex,pdMS_TO_TICKS(5))==pdTRUE){cur=currentRpm;tgt=targetRpm;xSemaphoreGive(rpmMutex);}
    lcd.setCursor(0,1); char l2[17]; snprintf(l2,sizeof(l2),"A:%3.0f T:%3.0f",cur,tgt); char pad[17]; snprintf(pad,sizeof(pad),"%-16s",l2); lcd.print(pad); lastRefresh=millis();
  }
}

void handle_wifi() {
  static uint32_t lastScanTime=0; static int networkCount=-1; static int selectedNetworkIndex=0;
  static bool blink=false; static uint32_t lastBlink=0;

  if (uiForceRedraw) { lcd.clear(); uiForceRedraw=false; networkCount=-1; }

  if (millis()-lastBlink >= BLINK_INTERVAL_MS) { blink = !blink; lastBlink = millis(); }
  String l0 = blink ? (language==0 ? "MODO AP" : "AP MODE") : WiFi.softAPIP().toString();
  lcd.setCursor(0,0); char line0[17]; snprintf(line0,sizeof(line0),"%-16s", l0.c_str()); lcd.print(line0);

  if (networkCount<0 || millis()-lastScanTime>WIFI_SCAN_INTERVAL_MS) {
    networkCount = WiFi.scanNetworks();
    lastScanTime = millis();
    selectedNetworkIndex = 0;
  }

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

void IRAM_ATTR knob_callback(long value) {
  KnobValue = -value;
  rotaryEncoder.resetEncoderValue();
}
