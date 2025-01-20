#include <WiFi.h>
#include <WiFiClient.h>
#include <Update.h>
#include <SPIFFS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFSEditor.h>

// Configuración personalizada y dependencias específicas del proyecto
#include "config/mqtt_config.h"
#include "config/wifi_config.h"
#include "config.h"
#include "mqttclient.h"
#include "webserver.h"
#include "measure.h"
#include "wificlient.h"

// Declaración del servidor web asíncrono y el socket web
AsyncWebServer asyncserver(WEBSERVERPORT); // Puerto definido en un archivo de configuración
AsyncWebSocket ws("/ws");                  // Ruta del WebSocket

// Tarea asociada al servidor web
TaskHandle_t _WEBSERVER_Task;

// Página HTML para actualización OTA (Over-The-Air)
static const char *serverIndex =
    "<!DOCTYPE html>\n<html>\n<head>\n"
    "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>\n"
    "<script src='/jquery.min.js'></script>\n"
    "<style>\n"
    "#progressbarfull {\n"
    "  background-color: #20201F;\n"
    "  border-radius: 20px;\n"
    "  width: 320px;\n"
    "  padding: 4px;\n"
    "}\n"
    "#progressbar {\n"
    "  background-color: #20CC00;\n"
    "  width: 3%;\n"
    "  height: 16px;\n"
    "  border-radius: 10px;\n"
    "}\n"
    "</style>\n"
    "</head>\n<body>\n"
    "<h2>Update by Browser</h2>\n"
    "<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>\n"
    "  <input type='file' name='update'>\n"
    "  <br><br>\n"
    "  <input type='submit' value='Update'>\n"
    "</form>\n"
    "<div id='prg'>Progress: 0%</div>\n"
    "<div id=\"progressbarfull\">\n"
    "  <div id=\"progressbar\"></div>\n"
    "</div>\n"
    "<script>\n"
    "$('form').submit(function(e) {\n"
    "  e.preventDefault();\n"
    "  var form = $('#upload_form')[0];\n"
    "  var data = new FormData(form);\n"
    "  $.ajax({\n"
    "    url: '/update',\n"
    "    type: 'POST',\n"
    "    data: data,\n"
    "    contentType: false,\n"
    "    processData: false,\n"
    "    xhr: function() {\n"
    "      var xhr = new window.XMLHttpRequest();\n"
    "      xhr.upload.addEventListener('progress', function(evt) {\n"
    "        if (evt.lengthComputable) {\n"
    "          var per = evt.loaded / evt.total;\n"
    "          document.getElementById(\"prg\").innerHTML = 'Progress: ' + Math.round(per * 100) + '%';\n"
    "          document.getElementById(\"progressbar\").style.width = Math.round(per * 100) + '%';\n"
    "        }\n"
    "      }, false);\n"
    "      return xhr;\n"
    "    },\n"
    "    success: function(d, s) {\n"
    "      document.getElementById(\"prg\").innerHTML = 'Progress: success';\n"
    "      console.log('success!');\n"
    "    },\n"
    "    error: function(a, b, c) {\n"
    "      document.getElementById(\"prg\").innerHTML = 'Progress: error';\n"
    "    }\n"
    "  });\n"
    "});\n"
    "</script>\n"
    "</body>\n</html>";

static void asyncwebserver_Task(void *pvParameters);

static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    static int selectedchannel = 0;

    // Manejo de eventos del WebSocket
    switch (type)
    {
    case WS_EVT_CONNECT:
    {
        // Evento cuando un cliente se conecta al WebSocket
        // Aquí puedes añadir cualquier lógica que necesites ejecutar al conectarse
        break;
    }

    case WS_EVT_ERROR:
    {
        // Evento cuando ocurre un error en la conexión del WebSocket
        // Es útil para registrar o depurar errores
        break;
    }

    case WS_EVT_PONG:
    {
        // Evento de respuesta a un mensaje PING
        // Utilizado para verificar la conectividad con el cliente
        break;
    }

    case WS_EVT_DISCONNECT:
    {
        // Evento cuando un cliente se desconecta del WebSocket
        // Puedes usarlo para limpiar recursos asociados al cliente
        break;
    }

    case WS_EVT_DATA:
    {
        // Evento cuando se reciben datos a través del WebSocket

        /*
         * Crear un buffer separado para almacenar los datos recibidos
         * Terminarlos con un carácter nulo (\0) para manejarlo como una cadena de caracteres
         */
        char *cmd = (char *)calloc(len + 1, sizeof(uint8_t)); // Reservar memoria
        memcpy(cmd, data, len);                               // Copiar los datos recibidos
        cmd[len] = '\0';                                      // Terminar con un NULL para convertirlo en una cadena válida

        /*
         * Separar el comando recibido y su valor asociado (si existe)
         * El formato esperado es "comando\\valor"
         */
        char *value = cmd; // Puntero que recorrerá la cadena
        while (*value)
        {
            if (*value == '\\')
            {                  // Detectar el separador
                *value = '\0'; // Terminar el comando
                value++;       // Mover el puntero al inicio del valor
                break;
            }
            value++;
        }

        /*
         * Manejo de comandos específicos recibidos del cliente WebSocket
         */
        if (!strcmp("SAV", cmd))
        {
            // Comando para guardar configuraciones en SPIFFS
            measure_save_settings();    // Guardar configuraciones de medición
            mqtt_save_settings();       // Guardar configuraciones de MQTT
            wificlient_save_settings(); // Guardar configuraciones de WiFi

            // Enviar respuesta al cliente indicando éxito
            client->printf("status\\Save");
        }
        /**
         * Comando: "get_channel_list"
         * Propósito: Enviar al cliente la lista de nombres de los canales y su estado de uso.
         */
        if (!strcmp("get_channel_list", cmd))
        {
            for (int i = 0; i < VIRTUAL_CHANNELS; i++)
            {
                // Enviar el nombre del canal al cliente
                client->printf("get_channel_list\\channel_%d_name\\%s", i, measure_get_channel_name(i));

                // Enviar el estado de uso del canal al cliente
                client->printf("get_channel_use_list\\channel%d\\%s\\channel\\%d\\%s",
                               i,
                               (measure_get_channel_type(i) != NO_CHANNEL_TYPE) &&
                                       measure_get_group_active(measure_get_channel_group_id(i))
                                   ? "true"
                                   : "false",
                               i,
                               measure_get_channel_name(i));
            }
        }
        /**
         * Comando: "get_channel_config"
         * Propósito: Enviar al cliente la configuración detallada del canal seleccionado.
         */
        else if (!strcmp("get_channel_config", cmd))
        {
            char tmp[64] = ""; // Buffer temporal para cadenas de datos

            // Validar que el canal seleccionado esté dentro del rango válido
            if (selectedchannel >= VIRTUAL_CHANNELS)
            {
                selectedchannel = 0; // Restablecer al canal 0 si el seleccionado es inválido
            }

            // Enviar los datos básicos del canal seleccionado
            client->printf("channel\\%d", selectedchannel);
            client->printf("channel_type\\%01x", measure_get_channel_type(selectedchannel));
            client->printf("checkbox\\channel_true_rms\\%s", measure_get_channel_true_rms(selectedchannel) ? "true" : "false");
            client->printf("channel_report_exp\\%d", measure_get_channel_report_exp(selectedchannel));
            client->printf("channel_phaseshift\\%d", measure_get_channel_phaseshift(selectedchannel));
            client->printf("channel_opcodeseq_str\\%s", measure_get_channel_opcodeseq_str(selectedchannel, sizeof(tmp), tmp));
            client->printf("old_channel_opcodeseq_str\\%s", measure_get_channel_opcodeseq_str(selectedchannel, sizeof(tmp), tmp));
            client->printf("channel_offset\\%f", measure_get_channel_offset(selectedchannel));
            client->printf("channel_ratio\\%f", measure_get_channel_ratio(selectedchannel));
            client->printf("channel_name\\%s", measure_get_channel_name(selectedchannel));
            client->printf("channel_group_id\\%d", measure_get_channel_group_id(selectedchannel));

            // Enviar opciones disponibles para los canales
            for (int i = 0; i < VIRTUAL_CHANNELS; i++)
            {
                if (strlen(measure_get_channel_name(i)) > 0)
                {
                    client->printf("option\\channel\\%d\\%s", i, measure_get_channel_name(i));
                }
            }

            // Enviar opciones disponibles para los grupos
            for (int i = 0; i < MAX_GROUPS; i++)
            {
                if (strlen(measure_get_group_name(i)) > 0)
                {
                    client->printf("option\\channel_group_id\\%d\\%s", i, measure_get_group_name(i));
                }
            }
        }
        /**
         * Comando: "get_wlan_settings"
         * Propósito: Enviar al cliente la configuración actual de la red WiFi.
         */
        else if (!strcmp("get_wlan_settings", cmd))
        {
            client->printf("ssid\\%s", wificlient_get_ssid());                                                // Nombre de la red WiFi (SSID)
            client->printf("password\\%s", "********");                                                       // Ocultar contraseña
            client->printf("checkbox\\enable_softap\\%s", wificlient_get_enable_softap() ? "true" : "false"); // Estado del SoftAP
            client->printf("softap_ssid\\%s", wificlient_get_softap_ssid());                                  // SSID del SoftAP
            client->printf("softap_password\\%s", "********");                                                // Ocultar contraseña del SoftAP
            client->printf("checkbox\\low_power\\%s", wificlient_get_low_power() ? "true" : "false");         // Modo de bajo consumo
            client->printf("checkbox\\low_bandwidth\\%s", wificlient_get_low_bandwidth() ? "true" : "false"); // Modo de bajo ancho de banda
        }

        /**
         * Comando: "get_mqtt_settings"
         * Propósito: Enviar al cliente la configuración actual del cliente MQTT.
         */
        else if (!strcmp("get_mqtt_settings", cmd))
        {
            client->printf("mqtt_server\\%s", mqtt_client_get_server());                                            // Dirección del servidor MQTT
            client->printf("mqtt_port\\%d", mqtt_client_get_port());                                                // Puerto del servidor MQTT
            client->printf("mqtt_username\\%s", mqtt_client_get_username());                                        // Nombre de usuario MQTT
            client->printf("mqtt_password\\%s", "********");                                                        // Ocultar contraseña MQTT
            client->printf("mqtt_topic\\%s", mqtt_client_get_topic());                                              // Tópico configurado
            client->printf("mqtt_interval\\%d", mqtt_client_get_interval());                                        // Intervalo de envío de datos
            client->printf("checkbox\\mqtt_realtimestats\\%s", mqtt_client_get_realtimestats() ? "true" : "false"); // Estado de estadísticas en tiempo real
        }

        /**
         * Comando: "get_measurement_settings"
         * Propósito: Enviar al cliente la configuración relacionada con las mediciones.
         */
        else if (!strcmp("get_measurement_settings", cmd))
        {
            client->printf("network_frequency\\%f", measure_get_network_frequency()); // Frecuencia de red
            client->printf("samplerate_corr\\%d", measure_get_samplerate_corr());     // Corrección de la frecuencia de muestreo
        }

        /**
         * Comando: "get_hostname_settings"
         * Propósito: Enviar al cliente el nombre de host configurado.
         */
        else if (!strcmp("get_hostname_settings", cmd))
        {
            client->printf("hostname\\%s", wificlient_get_hostname()); // Nombre del host
        }

        /**
         * Comando: "get_group_settings"
         * Propósito: Enviar al cliente la configuración del grupo seleccionado.
         */
        else if (!strcmp("get_group_settings", cmd))
        {
            // Validar que el canal seleccionado esté dentro del rango válido
            if (selectedchannel >= MAX_GROUPS)
            {
                selectedchannel = 0; // Restablecer al grupo 0 si es inválido
            }

            // Enviar información del grupo seleccionado
            client->printf("channel\\%d", selectedchannel);
            client->printf("group_name\\%s", measure_get_group_name(selectedchannel));
            client->printf("group_active\\%d", measure_get_group_active(selectedchannel) ? 1 : 0);

            // Enviar opciones para todos los grupos
            for (int i = 0; i < MAX_GROUPS; i++)
            {
                if (strlen(measure_get_group_name(i)))
                {
                    client->printf("option\\channel\\%d\\%s", i, measure_get_group_name(i));
                }
            }

            // Enviar información de canales y su uso en los grupos
            for (int i = 0; i < VIRTUAL_CHANNELS; i++)
            {
                client->printf("get_group_use_list\\channel%d_%d\\true\\channel_%d_name\\%s",
                               i,
                               measure_get_channel_group_id(i),
                               i,
                               measure_get_channel_name(i));
            }

            // Enviar etiquetas de todos los grupos
            for (int i = 0; i < MAX_GROUPS; i++)
            {
                client->printf("label\\group_%d_name\\%s", i, measure_get_group_name(i));
            }
        }

        /**
         * Comando: "channel_group"
         * Propósito: Asignar un grupo a cada canal activo basado en el valor recibido.
         */
        else if (!strcmp("channel_group", cmd))
        {
            char *group = value;   // Puntero al valor recibido
            int channel_count = 0; // Contador de canales procesados

            // Procesar cada carácter del valor recibido
            while (*group)
            {
                if (*group >= '0' && *group <= '5')
                {                                                          // Validar que el grupo esté en el rango permitido
                    int group_id = *group - '0';                           // Convertir el carácter a un entero
                    measure_set_channel_group_id(channel_count, group_id); // Asignar el grupo al canal actual
                }
                channel_count++;
                group++;
            }
        }

        /**
         * Comando: "OSC"
         * Propósito: Obtener los buffers de muestras y FFT de los canales activos y enviarlos al cliente.
         */
        else if (!strcmp("OSC", cmd))
        {
            // Declaración del buffer para construir la respuesta
            char request[numbersOfSamples * VIRTUAL_CHANNELS + numbersOfFFTSamples * VIRTUAL_CHANNELS + 96] = "";
            char tmp[64] = "";             // Buffer temporal para datos individuales
            uint16_t *mybuffer;            // Puntero al buffer de datos de muestras
            char *active_channels = value; // Lista de canales activos
            int active_channel_count = 0;  // Contador de canales activos
            int SampleScale = 4;           // Factor de escala para muestras
            int FFTScale = 1;              // Factor de escala para FFT

            /**
             * Contar los canales activos.
             */
            while (*active_channels)
            {
                if (*active_channels == '1')
                {
                    active_channel_count++;
                }
                active_channels++;
            }

            // Ajustar escala en función del número de canales activos
            if (active_channel_count < 8)
                SampleScale = 2;
            if (active_channel_count < 5)
                SampleScale = 1;

            /**
             * Enviar información inicial al cliente.
             */
            snprintf(tmp, sizeof(tmp), "OScopeProbe\\%d\\%d\\%d\\%f\\",
                     active_channel_count,
                     numbersOfSamples / SampleScale,
                     numbersOfFFTSamples / FFTScale,
                     0.01); // Resolución temporal
            strncat(request, tmp, sizeof(request));

            /**
             * Obtener el buffer de muestras y construir el bloque de datos inicial.
             */
            mybuffer = measure_get_buffer();
            for (int channel = 0; channel < VIRTUAL_CHANNELS; channel++)
            {
                if (*(value + channel) == '0')
                    continue; // Saltar canales inactivos

                for (int i = 0; i < numbersOfSamples; i += SampleScale)
                {
                    snprintf(tmp, sizeof(tmp), "%03x",
                             mybuffer[numbersOfSamples * channel + i] > 0x0fff ? 0x0fff : mybuffer[numbersOfSamples * channel + i]);
                    strncat(request, tmp, sizeof(request));
                }
            }
            strncat(request, "\\", sizeof(request));

            /**
             * Obtener el buffer de FFT y construir el siguiente bloque de datos.
             */
            mybuffer = measure_get_fft();
            for (int channel = 0; channel < VIRTUAL_CHANNELS; channel++)
            {
                if (*(value + channel) == '0')
                    continue; // Saltar canales inactivos

                for (int i = 0; i < numbersOfFFTSamples; i += FFTScale)
                {
                    snprintf(tmp, sizeof(tmp), "%03x",
                             mybuffer[numbersOfFFTSamples * channel + i] > 0x0fff ? 0x0fff : mybuffer[numbersOfFFTSamples * channel + i]);
                    strncat(request, tmp, sizeof(request));
                }
            }
            strncat(request, "\\", sizeof(request));

            /**
             * Representar los canales activos en formato binario y luego convertir a hexadecimal.
             */
            char activeChannelsBinary[14]; // Binario para canales activos
            for (int channel = 0; channel < VIRTUAL_CHANNELS; channel++)
            {
                activeChannelsBinary[channel] = (*(value + channel) == '0') ? '0' : '1';
            }
            activeChannelsBinary[VIRTUAL_CHANNELS] = '\0';

            char hexChannels[8]; // Representación hexadecimal
            int binaryValue = strtol(activeChannelsBinary, NULL, 2);
            snprintf(hexChannels, sizeof(hexChannels), "%03X", binaryValue);
            strncat(request, hexChannels, sizeof(request));

            // Enviar el bloque de datos al cliente
            client->text(request);
        }

        /**
         * Comando: "STS"
         * Propósito: Enviar la línea de estado al cliente.
         */
        else if (!strcmp("STS", cmd))
        {
            char request[1024] = ""; // Buffer para la respuesta
            char tmp[128] = "";      // Buffer temporal

            // Encabezado de la línea de estado
            snprintf(request, sizeof(request), "status\\online (");

            /**
             * Iterar sobre los grupos y construir los datos de estado.
             */
            for (int group_id = 0; group_id < MAX_GROUPS; group_id++)
            {
                int power_channel = -1;
                int reactive_power_channel = -1;
                float cos_phi = 1.0f;

                // Saltar grupos inactivos o sin entradas
                if (!measure_get_group_active(group_id))
                    continue;
                if (!measure_get_channel_group_id_entrys(group_id))
                    continue;

                snprintf(tmp, sizeof(tmp), " %s:[ ", measure_get_group_name(group_id));
                strncat(request, tmp, sizeof(request));

                for (int channel = 0; channel < VIRTUAL_CHANNELS; channel++)
                {
                    if (measure_get_channel_group_id(channel) != group_id)
                        continue;

                    // Identificar canales de potencia activa y reactiva
                    if (measure_get_channel_type(channel) == AC_POWER)
                        power_channel = channel;
                    if (measure_get_channel_type(channel) == AC_REACTIVE_POWER)
                        reactive_power_channel = channel;

                    // Calcular el factor de potencia (cosφ) si los canales están disponibles
                    if (power_channel != -1 && reactive_power_channel != -1)
                    {
                        float active_power = measure_get_channel_rms(power_channel) + measure_get_channel_rms(reactive_power_channel);
                        cos_phi = active_power / measure_get_channel_rms(power_channel);
                    }

                    // Procesar el tipo de canal y agregar datos al buffer temporal
                    switch (measure_get_channel_type(channel))
                    {
                    case AC_VOLTAGE:
                    case DC_VOLTAGE:
                        snprintf(tmp, sizeof(tmp), "U=%.1f%s ",
                                 measure_get_channel_rms(channel),
                                 measure_get_channel_report_unit(channel));
                        break;
                    case AC_CURRENT:
                    case DC_CURRENT:
                        snprintf(tmp, sizeof(tmp), "I=%.2f%s ",
                                 measure_get_channel_rms(channel),
                                 measure_get_channel_report_unit(channel));
                        break;
                    case AC_POWER:
                    case DC_POWER:
                        snprintf(tmp, sizeof(tmp), "P=%.3f%s ",
                                 measure_get_channel_rms(channel),
                                 measure_get_channel_report_unit(channel));
                        break;
                    case AC_REACTIVE_POWER:
                        snprintf(tmp, sizeof(tmp), "Pvar=%.3f%s ",
                                 measure_get_channel_rms(channel),
                                 measure_get_channel_report_unit(channel));
                        break;
                    default:
                        tmp[0] = '\0'; // No se agrega nada si no hay tipo válido
                    }
                    strncat(request, tmp, sizeof(request));
                }

                // Agregar cosφ si está disponible
                if (power_channel != -1 && reactive_power_channel != -1)
                {
                    snprintf(tmp, sizeof(tmp), "Cos=%.3f ", cos_phi);
                    strncat(request, tmp, sizeof(request));
                }
                strncat(request, "]", sizeof(request));
            }

            // Agregar información de frecuencia, si está disponible
            tmp[0] = '\0';
            for (int i = 0; i < VIRTUAL_CHANNELS; i++)
            {
                if (measure_get_channel_type(i) == AC_VOLTAGE)
                {
                    snprintf(tmp, sizeof(tmp), " ; f = %.3fHz", measure_get_max_freq());
                    break;
                }
            }
            strncat(request, tmp, sizeof(request));
            strncat(request, " )", sizeof(request));

            // Enviar el estado al cliente
            client->printf(request);
        }
        /**
         * Comandos relacionados con la configuración de WiFi y el SoftAP.
         */
        else if (!strcmp("hostname", cmd))
        {
            // Establecer el nombre de host (hostname)
            wificlient_set_hostname(value);
        }
        else if (!strcmp("softap_ssid", cmd))
        {
            // Configurar el SSID del punto de acceso (SoftAP)
            wificlient_set_softap_ssid(value);
        }
        else if (!strcmp("softap_password", cmd))
        {
            // Configurar la contraseña del SoftAP, evitando sobrescribir si es "********"
            if (strcmp("********", value))
            {
                wificlient_set_softap_password(value);
            }
        }
        else if (!strcmp("ssid", cmd))
        {
            // Configurar el SSID de la red WiFi
            wificlient_set_ssid(value);
        }
        else if (!strcmp("password", cmd))
        {
            // Configurar la contraseña de la red WiFi, evitando sobrescribir si es "********"
            if (strcmp("********", value))
            {
                wificlient_set_password(value);
            }
        }
        else if (!strcmp("enable_softap", cmd))
        {
            // Activar o desactivar el SoftAP
            wificlient_set_enable_softap(atoi(value) ? true : false);
        }

        /**
         * Comandos relacionados con la configuración de energía y ancho de banda.
         */
        else if (!strcmp("low_power", cmd))
        {
            // Configurar el modo de bajo consumo
            wificlient_set_low_power(atoi(value) ? true : false);
        }
        else if (!strcmp("low_bandwidth", cmd))
        {
            // Configurar el modo de bajo ancho de banda
            wificlient_set_low_bandwidth(atoi(value) ? true : false);
        }

        /**
         * Comandos relacionados con la configuración del cliente MQTT.
         */
        else if (!strcmp("mqtt_server", cmd))
        {
            // Configurar el servidor MQTT
            mqtt_client_set_server(value);
        }
        else if (!strcmp("mqtt_port", cmd))
        {
            // Configurar el puerto del servidor MQTT
            mqtt_client_set_port(atoi(value));
        }
        else if (!strcmp("mqtt_username", cmd))
        {
            // Configurar el usuario MQTT
            mqtt_client_set_username(value);
        }
        else if (!strcmp("mqtt_password", cmd))
        {
            // Configurar la contraseña MQTT, evitando sobrescribir si es "********"
            if (strcmp("********", value))
            {
                mqtt_client_set_password(value);
            }
        }
        else if (!strcmp("mqtt_topic", cmd))
        {
            // Configurar el tópico MQTT
            mqtt_client_set_topic(value);
        }
        else if (!strcmp("mqtt_interval", cmd))
        {
            // Configurar el intervalo de envío de datos MQTT
            mqtt_client_set_interval(atoi(value));
        }
        else if (!strcmp("mqtt_realtimestats", cmd))
        {
            // Activar o desactivar las estadísticas en tiempo real para MQTT
            mqtt_client_set_realtimestats(atoi(value) ? true : false);
        }

        /**
         * Comandos relacionados con la configuración de mediciones.
         */
        else if (!strcmp("samplerate_corr", cmd))
        {
            // Establecer la corrección de la frecuencia de muestreo
            measure_set_samplerate_corr(atoi(value));
        }
        else if (!strcmp("network_frequency", cmd))
        {
            // Configurar la frecuencia de red (en Hz)
            measure_set_network_frequency(atof(value));
        }
        else if (!strcmp("FQ+", cmd))
        {
            // Incrementar la frecuencia de muestreo en 1 Hz
            measure_set_samplerate_corr(measure_get_samplerate_corr() + 1);
        }
        else if (!strcmp("FQ-", cmd))
        {
            // Disminuir la frecuencia de muestreo en 1 Hz
            measure_set_samplerate_corr(measure_get_samplerate_corr() - 1);
        }

        /**
         * Comandos relacionados con la configuración de canales.
         */
        else if (!strcmp("PS+", cmd))
        {
            // Incrementar el desplazamiento de fase del canal seleccionado
            measure_set_channel_phaseshift(selectedchannel, measure_get_channel_phaseshift(selectedchannel) + 1);
        }
        else if (!strcmp("PS-", cmd))
        {
            // Disminuir el desplazamiento de fase del canal seleccionado
            measure_set_channel_phaseshift(selectedchannel, measure_get_channel_phaseshift(selectedchannel) - 1);
        }
        else if (!strcmp("channel", cmd))
        {
            // Seleccionar un canal
            selectedchannel = atoi(value);
        }
        else if (!strcmp("channel_type", cmd))
        {
            // Configurar el tipo de canal
            measure_set_channel_type(selectedchannel, (channel_type_t)atoi(value));
        }
        else if (!strcmp("channel_report_exp", cmd))
        {
            // Configurar el exponente del informe del canal
            measure_set_channel_report_exp(selectedchannel, atoi(value));
        }
        else if (!strcmp("channel_phaseshift", cmd))
        {
            // Configurar el desplazamiento de fase del canal
            measure_set_channel_phaseshift(selectedchannel, atoi(value));
        }
        else if (!strcmp("channel_true_rms", cmd))
        {
            // Configurar si el canal usa RMS verdadero
            measure_set_channel_true_rms(selectedchannel, atoi(value) ? true : false);
        }
        else if (!strcmp("channel_opcodeseq_str", cmd))
        {
            // Configurar la secuencia de código de operación del canal
            measure_set_channel_opcodeseq_str(selectedchannel, value);
        }
        else if (!strcmp("channel_offset", cmd))
        {
            // Configurar el desplazamiento del canal
            measure_set_channel_offset(selectedchannel, atof(value));
        }
        else if (!strcmp("channel_ratio", cmd))
        {
            // Configurar la relación del canal
            measure_set_channel_ratio(selectedchannel, atof(value));
        }
        else if (!strcmp("channel_name", cmd))
        {
            // Configurar el nombre del canal
            measure_set_channel_name(selectedchannel, value);
        }
        else if (!strcmp("channel_group_id", cmd))
        {
            // Configurar el ID del grupo al que pertenece el canal
            measure_set_channel_group_id(selectedchannel, atoi(value));
        }

        /**
         * Comandos relacionados con grupos.
         */
        else if (!strcmp("group_name", cmd))
        {
            // Configurar el nombre del grupo seleccionado
            measure_set_group_name(selectedchannel, value);
        }
        else if (!strcmp("group_active", cmd))
        {
            // Configurar si el grupo seleccionado está activo
            measure_set_group_active(selectedchannel, atoi(value) ? true : false);
        }

        // Liberar memoria del comando
        free(cmd);
    }
    }
}

/*
 * based on: https://github.com/lbernstone/asyncUpdate/blob/master/AsyncUpdate.ino
 */
static void handleUpdate(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
{
    mqtt_client_disable();

    if (!index)
    {
        /*
         * if filename includes spiffs, update the spiffs partition
         */
        int cmd = (filename.indexOf("spiffs") > 0) ? U_SPIFFS : U_FLASH;
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd))
            Update.printError(Serial);
    }
    /*
     * Write Data an type message if fail
     */
    if (Update.write(data, len) != len)
        Update.printError(Serial);
    /*
     * After write Update restart
     */
    if (final)
    {
        AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "Please wait while the powermeter reboots");
        response->addHeader("Refresh", "20");
        response->addHeader("Location", "/");
        request->send(response);

        if (!Update.end(true))
            Update.printError(Serial);
        else
        {
            log_i("Update complete");
            //   display_save_settings();
            // ioport_save_settings();
            measure_save_settings();
            mqtt_save_settings();
            wificlient_save_settings();
            ESP.restart();
        }
    }
    void mqtt_client_enable();
}
/**
 * Configuración inicial del servidor web asíncrono.
 */
static void asyncwebserver_setup(void)
{
    // Ruta "/info": Enviar información del firmware, compilador y versión.
    asyncserver.on("/info", HTTP_GET, [](AsyncWebServerRequest *request)
                   { request->send(200, "text/plain",
                                   "Firmwarestand: " __DATE__ " " __TIME__ "\r\n"
                                   "GCC-Version: " __VERSION__ "\r\n"
                                   "Version: " __FIRMWARE__ "\r\n"); });

    // Habilitar el editor de SPIFFS.
    asyncserver.addHandler(new SPIFFSEditor(SPIFFS));

    // Servir archivos estáticos desde SPIFFS con un archivo predeterminado.
    asyncserver.serveStatic("/", SPIFFS, "/").setDefaultFile("index.htm");

    // Manejar rutas no encontradas (404).
    asyncserver.onNotFound([](AsyncWebServerRequest *request)
                           { request->send(404); });

    // Manejo de carga de archivos.
    asyncserver.onFileUpload([](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
                             {
        if (!index) {
            log_i("UploadStart: %s", filename.c_str());
        }
        log_i("%s", (const char*)data);
        if (final) {
            log_i("UploadEnd: %s (%u)", filename.c_str(), index + len);
        } });

    // Manejo de datos en el cuerpo de solicitudes.
    asyncserver.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                              {
        if (!index) {
            log_i("BodyStart: %u", total);
        }
        log_i("%s", (const char*)data);
        if (index + len == total) {
            log_i("BodyEnd: %u\n", total);
        } });

    // Ruta "/default": Restablecer configuraciones predeterminadas y reiniciar el dispositivo.
    asyncserver.on("/default", HTTP_GET, [](AsyncWebServerRequest *request)
                   {
        AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "Reset to default. Please wait while the powermeter reboots\r\n");
        response->addHeader("Refresh", "20");  
        response->addHeader("Location", "/");
        request->send(response);

        // Eliminar archivos de configuración del sistema de archivos.
        SPIFFS.remove("powermeter.json");
        SPIFFS.remove("measure.json");

        delay(3000);
        ESP.restart(); });

    // Ruta "/memory": Mostrar detalles de memoria del sistema.
    asyncserver.on("/memory", HTTP_GET, [](AsyncWebServerRequest *request)
                   {
        String html = (String) "<html><head><meta charset=\"utf-8\"></head><body><h3>Memory Details</h3>" +
                    "<b>Heap size: </b>" + ESP.getHeapSize() + "<br>" +
                    "<b>Heap free: </b>" + ESP.getFreeHeap() + "<br>" +
                    "<b>Heap free min: </b>" + ESP.getMinFreeHeap() + "<br>" +
                    "<b>Psram size: </b>" + ESP.getPsramSize() + "<br>" +
                    "<b>Psram free: </b>" + ESP.getFreePsram() + "<br>" +
                    "<br><b><u>System</u></b><br>" +
                    "<b>Uptime: </b>" + millis() / 1000 + "<br>" +
                    "</body></html>";
        request->send(200, "text/html", html); });

    // Ruta "/reset": Reiniciar el dispositivo.
    asyncserver.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request)
                   {
        AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "Please wait while the powermeter reboots");
        response->addHeader("Refresh", "20");  
        response->addHeader("Location", "/");
        request->send(response);
        delay(3000);
        ESP.restart(); });

    // Ruta "/update" (GET): Mostrar la interfaz de actualización OTA.
    asyncserver.on("/update", HTTP_GET, [](AsyncWebServerRequest *request)
                   { request->send(200, "text/html", serverIndex); });

    // Ruta "/update" (POST): Manejar la carga y actualización del firmware.
    asyncserver.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {}, // No se realiza ninguna acción en la solicitud inicial.
                   [](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
                   {
                       handleUpdate(request, filename, index, data, len, final); // Manejar actualización OTA.
                   });

    // Configurar WebSocket y asignar el evento manejador.
    ws.onEvent(onWsEvent);
    asyncserver.addHandler(&ws);

    // Iniciar el servidor.
    asyncserver.begin();
}

/**
 * Crear una tarea para ejecutar el servidor web asíncrono.
 */
void asyncwebserver_StartTask(void)
{
    xTaskCreatePinnedToCore(
        asyncwebserver_Task, // Función que implementa la tarea.
        "webserver Task",    // Nombre de la tarea.
        10000,               // Tamaño de la pila en palabras.
        NULL,                // Parámetro de entrada para la tarea.
        1,                   // Prioridad de la tarea.
        &_WEBSERVER_Task,    // Handle de la tarea.
        _WEBSERVER_TASKCORE  // Núcleo donde debe ejecutarse la tarea.
    );
}

/**
 * Función que implementa la tarea del servidor web.
 */
static void asyncwebserver_Task(void *pvParameters)
{
    log_i("Start Webserver on Core: %d", xPortGetCoreID());

    // Configurar el servidor web.
    asyncwebserver_setup();

    // Loop principal de la tarea.
    while (true)
    {
        vTaskDelay(10);      // Pequeño retraso para permitir otras tareas.
        ws.cleanupClients(); // Limpiar clientes WebSocket desconectados.
    }
}
