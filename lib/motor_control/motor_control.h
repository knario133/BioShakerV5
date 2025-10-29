#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <Arduino.h>
#include <FastAccelStepper.h>
#include "shared_logic.h"

void motor_setup();
void motor_task(void *parameter);
void stop_motor_hard(bool from_ui);

#endif // MOTOR_CONTROL_H
