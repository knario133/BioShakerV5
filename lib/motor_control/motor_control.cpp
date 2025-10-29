#include "motor_control.h"
#include "ui_manager.h" // Needed for g_resetRpmEstimator and uiForceRedraw

// ============================
// Pines
// ============================
#define DIR_PIN 27
#define STEP_PIN 26
#define ENABLE_PIN 25

// ============================
// Motor / Calibración
// ============================

// ============================
// Instancias
// ============================
FastAccelStepperEngine engine;
FastAccelStepper *stepper = NULL;

// ============================
// Variables compartidas
// ============================
float targetRpm  = 0.0f;
float currentRpm = 0.0f;
extern SemaphoreHandle_t rpmMutex;

// ============================
// Rampa de aceleración
// ============================
const double A_CMD   = SPR_CMD / 6.0; // Acceleration for the ramp

/**
 * @brief Initializes the motor, stepper driver, and pins.
 */
void motor_setup() {
  engine.init();
  stepper = engine.stepperConnectToPin(STEP_PIN);
  if (stepper) {
    stepper->setDirectionPin(DIR_PIN);
    stepper->setEnablePin(ENABLE_PIN);
    stepper->setAutoEnable(true);
    stepper->setAcceleration(20000); // High acceleration to not limit the ramp
  }
}

/**
 * @brief FreeRTOS task to control the motor speed.
 *
 * @param parameter Task parameter (not used).
 */
void motor_task(void *parameter) {
  while (true) {
    double sp_rpm;
    if (xSemaphoreTake(rpmMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
      sp_rpm = (double)targetRpm;
      xSemaphoreGive(rpmMutex);
    }

    if (stepper) {
      if (sp_rpm < 1.0) {
        if (stepper->isRunningContinuously()) {
          stepper->stopMove();
          stepper->disableOutputs();
        }
      } else {
        double targetSPS = rpm2sps(sp_rpm);
        stepper->setAcceleration((float)A_CMD);
        stepper->setSpeedInHz((float)targetSPS);
        if (!stepper->isRunningContinuously()) {
          stepper->enableOutputs();
          stepper->runForward();
        } else {
          stepper->applySpeedAcceleration();
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

/**
 * @brief Stops the motor immediately and resets the RPM.
 *
 * @param from_ui True if the stop was triggered from the UI.
 */
void stop_motor_hard(bool from_ui) {
  if (xSemaphoreTake(rpmMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    targetRpm = 0.0f;
    currentRpm = 0.0f;
    xSemaphoreGive(rpmMutex);
  }

  if (stepper) {
    stepper->stopMove();
    stepper->disableOutputs();
  }

  g_resetRpmEstimator = true;
  if (from_ui) {
      uiForceRedraw = true;
  }
}
