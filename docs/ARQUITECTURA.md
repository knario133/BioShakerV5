# Arquitectura del Firmware del BioShaker

Este documento describe la arquitectura de alto nivel del firmware del BioShaker, sus componentes principales y cómo interactúan.

## Filosofía de Diseño

El firmware está diseñado siguiendo un enfoque **modular y multitarea**, utilizando [FreeRTOS](https://www.freertos.org/) para gestionar las diferentes responsabilidades del sistema de forma concurrente. Esto permite que la interfaz de usuario, el control del motor y el servidor web operen de forma independiente sin bloquearse mutuamente.

## Componentes Principales

El código está dividido en las siguientes librerías y módulos:

### `src/main.cpp`

*   **Responsabilidad**: Es el punto de entrada de la aplicación. Su única función es inicializar los módulos principales y crear las tareas de FreeRTOS.
*   **Tareas Creadas**:
    *   `ui_task`: Gestiona la interfaz de usuario (prioridad 1).
    *   `motor_task`: Controla el motor (prioridad 2, más alta para asegurar una respuesta precisa).

### `lib/motor_control`

*   **Responsabilidad**: Control directo del motor paso a paso.
*   **Componentes Clave**:
    *   Utiliza la librería `FastAccelStepper` para generar los pulsos de control del motor.
    *   Implementa una rampa de aceleración suave para evitar movimientos bruscos.
    *   La tarea `motor_task` lee continuamente la variable `targetRpm` y ajusta la velocidad del motor.

### `lib/ui_manager`

*   **Responsabilidad**: Gestionar toda la interacción con el usuario a través de la pantalla LCD y el encoder rotativo.
*   **Componentes Clave**:
    *   Implementa una **máquina de estados** (`UiState`) para gestionar las diferentes pantallas (splash, normal, menú, etc.).
    *   La tarea `ui_task` lee las entradas del encoder, actualiza el estado de la UI y redibuja la pantalla cuando es necesario.
    *   También calcula las RPM actuales midiendo los pasos del motor.

### `lib/wifi_manager`

*   **Responsabilidad**: Gestionar la conectividad WiFi y el servidor web asíncrono.
*   **Componentes Clave**:
    *   Utiliza `ESPAsyncWebServer` para servir la interfaz web (`index.html`) y gestionar las llamadas a la API.
    *   Implementa un modo dual **AP+STA**. Si no puede conectarse a una red guardada, crea un punto de acceso para la configuración.
    *   **API Endpoints**:
        *   `/status` (GET): Devuelve un JSON con el estado actual del dispositivo.
        *   `/rpm` (GET): Fija una nueva velocidad de RPM.
        *   `/stop` (POST): Detiene el motor.
        *   `/scan` (GET): Escanea y devuelve las redes WiFi disponibles.
        *   `/saveWifi` (POST): Guarda las credenciales de una nueva red y reinicia.

### `lib/shared_logic`

*   **Responsabilidad**: Contener lógica de negocio "pura", es decir, funciones que no dependen de ningún hardware específico.
*   **Componentes Clave**:
    *   `rpm2sps()`: Convierte RPM a pasos por segundo.
    *   Este módulo es el único que se prueba en el **entorno `native`** para la integración continua.

## Concurrencia y Sincronización

Para evitar condiciones de carrera y asegurar la integridad de los datos compartidos entre tareas, se utiliza un **mutex** (`rpmMutex`).

*   **Datos Protegidos**: `targetRpm` y `currentRpm`.
*   Cualquier tarea que necesite leer o escribir estas variables debe primero adquirir el mutex.
