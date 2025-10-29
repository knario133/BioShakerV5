# Manual de Usuario Detallado

Este manual cubre todas las funcionalidades del BioShaker, tanto de la interfaz física como de la interfaz web.

## Interfaz Física (LCD y Encoder)

La pantalla principal muestra el estado actual del dispositivo. La línea superior muestra la información de la red y la inferior, las RPM actuales y objetivo.

### Menú Principal

Para acceder al menú, pulsa el botón del encoder. Las opciones son:

*   **Ajustar RPM**: Permite fijar la velocidad de agitación.
*   **Detener Motor**: Para el motor de forma inmediata.
*   **Configurar WiFi / Desconectar WiFi**: Esta opción cambia según el estado de la conexión. Permite conectarse a una red guardada o desconectarse de la red actual.
*   **Idioma**: Cambia el idioma entre Español e Inglés.
*   **Volver**: Cierra el menú y vuelve a la pantalla principal.

## Interfaz Web

Para acceder a la interfaz web, primero debes conectar tu BioShaker a una red WiFi (ver [[Guía de Conexión WiFi]]). Una vez conectado, introduce la dirección IP del dispositivo en un navegador.

La interfaz web te permite controlar el BioShaker de forma remota.

### Estado Conectado (Modo STA)

Cuando el BioShaker está conectado a tu red WiFi, la interfaz se ve así. Puedes ver las RPM en tiempo real, el modo de conexión y la dirección IP.

![Interfaz Web - Conectado](https://github.com/tu-usuario/tu-repo/blob/main/docs/screenshots/web-ui-connected.png?raw=true)

### Modo de Configuración (Modo AP)

Si el BioShaker no está conectado a ninguna red, entrará en Modo AP para que puedas configurarlo. La interfaz mostrará "MODO AP" parpadeando.

![Interfaz Web - Modo AP](https://github.com/tu-usuario/tu-repo/blob/main/docs/screenshots/web-ui-ap-mode.png?raw=true)

### Estado Desconectado

Si el navegador no puede comunicarse con el BioShaker (por ejemplo, si está fuera de rango), verás un mensaje de advertencia.

![Interfaz Web - Desconectado](https://github.com/tu-usuario/tu-repo/blob/main/docs/screenshots/web-ui-disconnected.png?raw=true)

### Controles

*   **Slider de Velocidad y Campo Numérico**: Ajusta las RPM deseadas.
*   **Botón "Modificar Velocidad"**: Envía la nueva velocidad al BioShaker.
*   **Botón "Detener Motor"**: Detiene el motor.
*   **Botón "Configurar Conexión WIFI"**: Te lleva a la página de configuración de WiFi, donde puedes escanear y conectarte a nuevas redes.
