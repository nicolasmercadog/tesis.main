#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#include "config.h"
#include "measure.h"
#include "mqttclient.h"
#include "ntp.h"
#include "webserver.h"
#include "wificlient.h"
#include <wifi.h>

/**
 * @brief Función de configuración de Arduino
 */

void setup( void ) {
    // Establece la frecuencia del CPU a 240 MHz para mejorar el rendimiento
    setCpuFrequencyMhz( 240 );
    
    // Inicia la comunicación serie a 115200 baudios
    Serial.begin(115200);

    /**
     * Inicialización de hardware y sistema de archivos
     */
    log_i("Start Main Task on Core: %d", xPortGetCoreID() );
    
    // Verifica si el sistema de archivos SPIFFS se inicia correctamente
    if ( !SPIFFS.begin() ) {
        log_i("format SPIFFS ..." );
        SPIFFS.format(); // Formatea SPIFFS en caso de error
    }

    // Inicializa la conexión Wi-Fi
    wificlient_init();

    /**
     * Configuración de tareas
     */
    measure_StartTask(); // Inicia la tarea de medición
    measure_get_fft(); // Obtiene la FFT de los canales
    //startMongoDBTask(); // Inicia la tarea del cliente MQTT
    asyncwebserver_StartTask(); // Inicia la tarea del servidor web asíncrono
    ntp_StartTask(); // Inicia la tarea para sincronización de hora mediante NTP
    esp_log_level_set("*", ESP_LOG_VERBOSE); // Muestra todos los logs posibles

    //sendDataToMongoDB("test_topic", "Test message desde ESP32"); // Código comentado para enviar datos a MongoDB
}

/**
 * @brief Bucle principal de Arduino
 */
void loop() {
    //ioport_loop();
}
