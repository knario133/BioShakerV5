#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

/**
 * @file wifi_manager.h
 * @brief Gestión de la conectividad WiFi y del servidor web.
 */

/**
 * @brief Inicializa el WiFi, el servidor web y los eventos asociados.
 */
void wifi_setup();

/**
 * @brief Inicia el dispositivo en modo AP+STA.
 *
 * Crea un punto de acceso para la configuración inicial.
 */
void startAPAlways();

/**
 * @brief Intenta conectarse a una red WiFi previamente guardada.
 *
 * @param asyncRetry Si es `true`, el intento de conexión no es bloqueante.
 */
void tryConnectSavedWifi(bool asyncRetry);

/**
 * @brief Desconecta el WiFi y apaga la radio.
 */
void goOffline();

/**
 * @brief Comprueba si el dispositivo está conectado a una red WiFi.
 *
 * @return `true` si está conectado, `false` en caso contrario.
 */
bool isStaConnected();

#endif // WIFI_MANAGER_H
