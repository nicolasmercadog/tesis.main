#include <Arduino.h>  // Librería base de Arduino
#include <WiFi.h>     // Librería para manejar el WiFi en ESP32
#include <esp_wifi.h> // Librería específica para funcionalidades avanzadas de WiFi en ESP32

#include "config/wifi_config.h" // Archivo de configuración específico del proyecto para el WiFi
#include "wificlient.h"         // Archivo de cabecera de este cliente WiFi

// Estructura de configuración para el cliente WiFi
static wificlient_config_t wificlient_config;

// Manejador de tarea para el cliente WiFi
static TaskHandle_t _wificlient_Task;

// Declaración de la función de la tarea WiFi (definida más adelante)
static void wificlient_Task(void *pvParameters);

/**
 * @brief Inicializa el cliente WiFi
 */
void wificlient_init(void) {
    static bool wifi_client_init = false; // Bandera para evitar inicializaciones múltiples

    if (wifi_client_init) {
        log_e("WiFi client is already running");
        return;
    }

    // Cargar configuración desde el archivo JSON
    wificlient_config.load();

    // Configurar modo WiFi como cliente (STA)
    if (!WiFi.mode(WIFI_STA)) {
        log_e("Failed to set WiFi mode to STA");
        return;
    }

    // Configurar ancho de banda según la configuración
    esp_wifi_set_bandwidth(ESP_IF_WIFI_STA, 
        wificlient_config.low_bandwidth ? WIFI_BW_HT20 : WIFI_BW_HT40);

    // Configurar potencia de transmisión
    esp_wifi_set_max_tx_power(
        wificlient_config.low_power ? 44 : 80);

    // Configurar hostname
    if (!WiFi.setHostname(wificlient_config.hostname)) {
        log_e("Failed to set hostname");
        return;
    }

    // Crear tarea del cliente WiFi
    xTaskCreatePinnedToCore(
        wificlient_Task,             // Función de la tarea
        "wificlient Task",           // Nombre de la tarea
        5000,                        // Tamaño de la pila
        NULL,                        // Parámetros
        1,                           // Prioridad
        &_wificlient_Task,           // Manejador de la tarea
        1                            // Núcleo
    );

    wifi_client_init = true; // Marcar como inicializado
}
/**
 * @brief Monitorea la conexión WiFi y realiza reconexiones o inicia el modo softAP si es necesario.
 */
static void wificlient_Task(void *pvParameters) {
    static bool wifi_client_task_init = false; // Bandera para evitar múltiples inicializaciones
    static bool APMODE = false;               // Bandera para indicar si está en modo AP

    if (wifi_client_task_init) {
        log_e("WiFi client task is already running");
        return;
    }

    wifi_client_task_init = true; // Marcar la tarea como inicializada

    while (true) {
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Espera de 1 segundo

        // Verificar si WiFi está desconectado
        if (WiFi.status() != WL_CONNECTED) {
            int wlan_timeout = wificlient_config.timeout; // Tiempo de espera desde la configuración
            log_i("WiFi connection lost, attempting to reconnect...");

            // Intentar reconexión
            WiFi.reconnect();

            // Esperar reconexión dentro del tiempo de espera
            while (WiFi.status() != WL_CONNECTED) {
                vTaskDelay(1000 / portTICK_PERIOD_MS); // Espera de 1 segundo

                // Si el tiempo de espera se agota
                if (--wlan_timeout <= 0) {
                    // Iniciar modo softAP si está habilitado
                    if (!APMODE && wificlient_config.enable_softap) {
                        WiFi.softAP(wificlient_config.softap_ssid, wificlient_config.softap_password);
                        log_i("Failed to reconnect. Starting softAP with SSID \"%s\"",
                              wificlient_config.softap_ssid);
                        log_i("AP IP address: %s", WiFi.softAPIP().toString().c_str());
                        APMODE = true;
                    }
                    break;
                }
            }

            // Si se reconecta correctamente, registrar la dirección IP
            if (WiFi.status() == WL_CONNECTED) {
                log_i("Connected to WiFi. IP address: %s", WiFi.localIP().toString().c_str());
                APMODE = false; // Desactivar el modo AP si estaba activo
            }
        }
    }
}
/**
 * @brief Obtiene el hostname configurado.
 */
const char *wificlient_get_hostname(void) {
    return wificlient_config.hostname;
}

/**
 * @brief Establece el hostname.
 * @param hostname Hostname a configurar.
 */
void wificlient_set_hostname(const char *hostname) {
    if (hostname == nullptr || strlen(hostname) >= sizeof(wificlient_config.hostname)) {
        log_e("Invalid hostname");
        return;
    }
    strlcpy(wificlient_config.hostname, hostname, sizeof(wificlient_config.hostname));
}

/**
 * @brief Obtiene el SSID configurado.
 */
const char *wificlient_get_ssid(void) {
    return wificlient_config.ssid;
}

/**
 * @brief Establece el SSID.
 * @param ssid SSID a configurar.
 */
void wificlient_set_ssid(const char *ssid) {
    if (ssid == nullptr || strlen(ssid) >= sizeof(wificlient_config.ssid)) {
        log_e("Invalid SSID");
        return;
    }
    strlcpy(wificlient_config.ssid, ssid, sizeof(wificlient_config.ssid));
}

/**
 * @brief Obtiene la contraseña configurada.
 */
const char *wificlient_get_password(void) {
    return wificlient_config.password;
}

/**
 * @brief Establece la contraseña.
 * @param password Contraseña a configurar.
 */
void wificlient_set_password(const char *password) {
    if (password == nullptr || strlen(password) >= sizeof(wificlient_config.password)) {
        log_e("Invalid password");
        return;
    }
    strlcpy(wificlient_config.password, password, sizeof(wificlient_config.password));
}

/**
 * @brief Obtiene el SSID del modo softAP.
 */
const char *wificlient_get_softap_ssid(void) {
    return wificlient_config.softap_ssid;
}

/**
 * @brief Establece el SSID del modo softAP.
 * @param softap_ssid SSID del softAP.
 */
void wificlient_set_softap_ssid(const char *softap_ssid) {
    if (softap_ssid == nullptr || strlen(softap_ssid) >= sizeof(wificlient_config.softap_ssid)) {
        log_e("Invalid softAP SSID");
        return;
    }
    strlcpy(wificlient_config.softap_ssid, softap_ssid, sizeof(wificlient_config.softap_ssid));
}

/**
 * @brief Obtiene la contraseña del modo softAP.
 */
const char *wificlient_get_softap_password(void) {
    return wificlient_config.softap_password;
}

/**
 * @brief Establece la contraseña del modo softAP.
 * @param softap_password Contraseña del softAP.
 */
void wificlient_set_softap_password(const char *softap_password) {
    if (softap_password == nullptr || strlen(softap_password) >= sizeof(wificlient_config.softap_password)) {
        log_e("Invalid softAP password");
        return;
    }
    strlcpy(wificlient_config.softap_password, softap_password, sizeof(wificlient_config.softap_password));
}

/**
 * @brief Obtiene el estado de habilitación del modo softAP.
 */
bool wificlient_get_enable_softap(void) {
    return wificlient_config.enable_softap;
}

/**
 * @brief Configura el estado de habilitación del modo softAP.
 * @param enable_softap True para habilitar, false para deshabilitar.
 */
void wificlient_set_enable_softap(bool enable_softap) {
    wificlient_config.enable_softap = enable_softap;
}

/**
 * @brief Obtiene el tiempo de espera configurado para reconexión.
 */
int wificlient_get_timeout(void) {
    return wificlient_config.timeout;
}

/**
 * @brief Configura el tiempo de espera para reconexión.
 * @param timeout Tiempo de espera en segundos.
 */
void wificlient_set_timeout(int timeout) {
    wificlient_config.timeout = timeout;
}

/**
 * @brief Obtiene el estado de configuración de ancho de banda bajo.
 */
bool wificlient_get_low_bandwidth(void) {
    return wificlient_config.low_bandwidth;
}

/**
 * @brief Configura el ancho de banda.
 * @param low_bandwidth True para ancho de banda bajo, false para alto.
 */
void wificlient_set_low_bandwidth(bool low_bandwidth) {
    wificlient_config.low_bandwidth = low_bandwidth;

    esp_wifi_set_bandwidth(ESP_IF_WIFI_STA, 
        low_bandwidth ? WIFI_BW_HT20 : WIFI_BW_HT40);
}

/**
 * @brief Obtiene el estado de configuración de bajo consumo.
 */
bool wificlient_get_low_power(void) {
    return wificlient_config.low_power;
}

/**
 * @brief Configura el consumo de energía.
 * @param low_power True para bajo consumo, false para alto.
 */
void wificlient_set_low_power(bool low_power) {
    wificlient_config.low_power = low_power;

    esp_wifi_set_max_tx_power(low_power ? 44 : 80);
}

/**
 * @brief Guarda la configuración actual en almacenamiento persistente.
 */
void wificlient_save_settings(void) {
    wificlient_config.save();
}
