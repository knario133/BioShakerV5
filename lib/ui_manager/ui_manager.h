#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <Arduino.h>

/**
 * @file ui_manager.h
 * @brief Gestión de la interfaz de usuario (LCD y encoder rotativo).
 */

/**
 * @brief Define los diferentes estados (pantallas) de la interfaz de usuario.
 */
enum UiState {
  UI_SPLASH,             ///< Pantalla de bienvenida.
  UI_NORMAL,             ///< Pantalla principal que muestra el estado.
  UI_MENU,               ///< Menú de opciones.
  UI_ADJUST_RPM,         ///< Pantalla para ajustar las RPM.
  UI_WIFI,               ///< Pantalla para configurar el WiFi.
  UI_LANGUAGE,           ///< Pantalla para cambiar el idioma.
  UI_AP_MODE,            ///< Pantalla que indica que se está en Modo AP.
  UI_WIFI_DISCONNECTED   ///< Pantalla que indica que se ha perdido la conexión WiFi.
};

extern UiState uiState; // La máquina de estados es global
extern volatile bool g_resetRpmEstimator;
extern volatile bool uiForceRedraw;

/**
 * @brief Inicializa los componentes de la interfaz de usuario (LCD y encoder).
 */
void ui_setup();

/**
 * @brief Tarea de FreeRTOS que gestiona la lógica de la interfaz de usuario.
 *
 * Esta tarea procesa las entradas del encoder, gestiona la máquina de estados
 * y actualiza la pantalla LCD.
 *
 * @param parameter Puntero a los parámetros de la tarea (no se usa).
 */
void ui_task(void *parameter);

#endif // UI_MANAGER_H
