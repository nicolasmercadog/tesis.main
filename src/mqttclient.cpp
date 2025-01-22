#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config/mqtt_config.h"
#include "config.h"
#include "measure.h"
#include "mqttclient.h"
#include "wificlient.h"
#include "ntp.h"

// URL del endpoint del servidor MongoDB (cambiar según necesidad)
const char* serverName = "https://sa-east-1.aws.data.mongodb-api.com/app/conecthttp-fhbjiiu/endpoint/test";

// Buffers de datos medidos
static float measure_rms[VIRTUAL_CHANNELS];  // Buffer para medidas RMS
static float measure_frequency;              // Frecuencia medida actual

char reset_state[64] = "";                   // Buffer para guardar el estado de reinicio

/**
 * @brief Enviar datos al servidor MongoDB
 * 
 * @param topic     Identificador del tipo de datos
 * @param payload   Datos en formato JSON
 */
void sendDataToMongoDB(const char* topic, const char* payload) {
    if (WiFi.status() == WL_CONNECTED) {
        WiFiClientSecure client;
        client.setInsecure();  // Solo para pruebas; usa certificados en producción

        HTTPClient http;
        if (http.begin(client, serverName)) {  // Conecta al endpoint de MongoDB
            http.addHeader("Content-Type", "application/json");

            // Crear el cuerpo del mensaje
            String jsonPayload = "{\"topic\": \"" + String(topic) + "\", \"payload\": " + String(payload) + "}";
            int httpResponseCode = http.POST(jsonPayload);

            if (httpResponseCode > 0) {
                Serial.printf("Datos enviados a MongoDB, respuesta HTTP: %d\n", httpResponseCode);
            } else {
                Serial.printf("Error en la solicitud HTTP: %d\n", httpResponseCode);
            }

            http.end();
        } else {
            Serial.println("Error: No se pudo conectar al servidor MongoDB.");
        }
    } else {
        Serial.println("WiFi no conectado, no se pueden enviar datos a MongoDB.");
    }
}

/**
 * @brief Envía datos de potencia en tiempo real a MongoDB
 */
void sendPowerDataToMongoDB(void) {
    String ip = WiFi.localIP().toString();
    StaticJsonDocument<8192> doc;

    struct tm timeinfo;
    char timeStr[64] = "";

    // Obtener la hora local
    if (getLocalTime(&timeinfo)) {
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    } else {
        strcpy(timeStr, "00:00:00");  // Si falla, un valor predeterminado
        Serial.println("Error al obtener la hora local.");
    }

    // Rellenar datos generales
    doc["topic"] = "power_data";
    doc["id"] = wificlient_get_hostname();
    doc["ip"] = ip.c_str();
    doc["time"] = timeStr;  // Hora local de Buenos Aires
    doc["uptime"] = millis() / 1000;
    doc["reset_state"] = reset_state;
    doc["measurement_valid"] = measure_get_measurement_valid();
    doc["frequency"] = measure_get_max_freq();

    // Agregar datos de los grupos y canales al nivel principal (sin cambios)
    for (int group_id = 0; group_id < 6; group_id++) {
        if (!measure_get_group_active(group_id) || !measure_get_channel_group_id_entrys(group_id))
            continue;

        for (int channel = 0; channel < VIRTUAL_CHANNELS; channel++) {
            if (measure_get_channel_group_id(channel) == group_id && measure_get_channel_type(channel) != NO_CHANNEL_TYPE) {
                char quantity[32] = "";
                char type[32] = "DC";

                // Determinar tipo y cantidad
                switch (measure_get_channel_type(channel)) {
                    case AC_CURRENT: snprintf(type, sizeof(type), "AC");
                    case DC_CURRENT: snprintf(quantity, sizeof(quantity), "current"); break;
                    case AC_VOLTAGE: snprintf(type, sizeof(type), "AC");
                    case DC_VOLTAGE: snprintf(quantity, sizeof(quantity), "voltage"); break;
                    case AC_POWER: snprintf(type, sizeof(type), "AC");
                    case DC_POWER: snprintf(quantity, sizeof(quantity), "power"); break;
                    case AC_REACTIVE_POWER: snprintf(type, sizeof(type), "AC");
                        snprintf(quantity, sizeof(quantity), "reactive power");
                        break;
                    default:
                        snprintf(type, sizeof(type), "n/a");
                        snprintf(quantity, sizeof(quantity), "n/a");
                        break;
                }

                // Construir nombre del campo único para aplanar los datos
                String fieldName = String(measure_get_group_name(group_id)) + "_" + quantity + "_value";
                doc[fieldName] = measure_get_channel_rms(channel);

                fieldName = String(measure_get_group_name(group_id)) + "_" + quantity + "_unit";
                doc[fieldName] = measure_get_channel_report_unit(channel);

                fieldName = String(measure_get_group_name(group_id)) + "_" + quantity + "_type";
                doc[fieldName] = type;

                fieldName = String(measure_get_group_name(group_id)) + "_" + quantity + "_name";
                doc[fieldName] = measure_get_channel_name(channel);
            }
        }
    }

    // Serializar JSON
    String json;
    serializeJson(doc, json);

    // Enviar a MongoDB
    sendDataToMongoDB("power_data", json.c_str());
}

/**
 * @brief Inicia la tarea en segundo plano para enviar datos a MongoDB
 */
void startMongoDBTask(void) {
    xTaskCreatePinnedToCore(
        sendDataToMongoDBTask,  /* Función encargada de la tarea */
        "MongoDB Task",         /* Nombre de la tarea */
        10000,                  /* Tamaño del stack */
        NULL,                   /* Parámetro de entrada (NULL si no se utiliza) */
        1,                      /* Prioridad de la tarea */
        NULL,                   /* Handle de la tarea (puedes guardarlo si lo necesitas) */
        1                       /* Núcleo donde correrá la tarea */
    );
}

void sendDataToMongoDBTask(void* pvParameters) {
    while (true) {
        // Llama a la función que envía los datos
        sendPowerDataToMongoDB();

        // Espera un intervalo antes de volver a enviar
        vTaskDelay(60000 / portTICK_PERIOD_MS);  // 1 minuto
    }
}
