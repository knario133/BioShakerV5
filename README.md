# BioShaker Firmware

Este repositorio contiene el firmware para el BioShaker, un agitador de laboratorio controlado por un ESP32.

## Características

*   Control de velocidad del motor paso a paso (RPM).
*   Interfaz de usuario física con pantalla LCD y encoder rotativo.
*   Interfaz web para control y monitorización remotos.
*   Conectividad WiFi con modo AP y STA.

## Estructura del Proyecto

El código está organizado en los siguientes módulos:

*   `src/main.cpp`: El punto de entrada principal de la aplicación.
*   `lib/motor_control`: Gestiona el control del motor (dependiente de hardware).
*   `lib/ui_manager`: Gestiona la interfaz de usuario (dependiente de hardware).
*   `lib/wifi_manager`: Gestiona la conectividad WiFi y el servidor web (dependiente de hardware).
*   `lib/shared_logic`: Contiene la lógica de negocio pura, independiente del hardware.
*   `src/config.h`: Contiene la configuración global del proyecto.
*   `test/test_native`: Contiene las pruebas unitarias para el entorno `native`.

## Compilación

Para compilar el firmware para el ESP32, necesitarás [PlatformIO](https://platformio.org/).

1.  Clona este repositorio.
2.  Abre el proyecto en Visual Studio Code con la extensión de PlatformIO.
3.  Conecta tu ESP32 y haz clic en el botón "Upload" de PlatformIO.

## Ejecución de Pruebas

Este proyecto utiliza pruebas nativas para verificar la lógica de negocio pura sin necesidad de hardware.

### Requisitos

*   Python 3.11+

### Pasos

1.  Instala PlatformIO Core:
    ```bash
    python -m pip install --upgrade platformio
    ```

2.  Ejecuta las pruebas nativas:
    ```bash
    python -m platformio test -e native
    ```

## Integración Continua

Este repositorio utiliza GitHub Actions para ejecutar las pruebas nativas automáticamente en cada `push` y `pull request`. Puedes ver el estado de las pruebas en la pestaña "Actions".

## Contribuciones

Las contribuciones son bienvenidas. Por favor, abre un "issue" para discutir los cambios propuestos.
