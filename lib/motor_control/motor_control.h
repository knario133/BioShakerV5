#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include "config.h"
#include <Arduino.h>
#include <FastAccelStepper.h>
#include "shared_logic.h"

/**
 * @file motor_control.h
 * @brief Funciones para el control del motor paso a paso.
 */

/**
 * @brief Inicializa el motor, el driver y los pines asociados.
 */
void motor_setup();

/**
 * @brief Tarea de FreeRTOS que controla la velocidad del motor.
 *
 * Esta tarea lee continuamente el valor de `targetRpm` y ajusta la velocidad
 * del motor para que coincida, aplicando una rampa de aceleración suave.
 *
 * @param parameter Puntero a los parámetros de la tarea (no se usa).
 */
void motor_task(void *parameter);

/**
 * @brief Detiene el motor de forma inmediata.
 *
 * @param from_ui `true` si la parada fue iniciada desde la interfaz de usuario física,
 *                lo que fuerza un redibujado de la pantalla.
 */
void stop_motor_hard(bool from_ui);

#endif // MOTOR_CONTROL_H
