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

    switch (type)
    {
    case WS_EVT_CONNECT:
    {
        break;
    }
    case WS_EVT_ERROR:
    {
        break;
    }
    case WS_EVT_PONG:
    {
        break;
    }
    case WS_EVT_DISCONNECT:
    {
        break;
    }
    case WS_EVT_DATA:
    {
        /*
         * copy data into an separate allocated buffer and terminate it with \0
         */
        char *cmd = (char *)calloc(len + 1, sizeof(uint8_t));
        memcpy(cmd, data, len);
        /*
         * separate command and his correspondening value
         */
        char *value = cmd;
        while (*value)
        {
            if (*value == '\\')
            {
                *value = '\0';
                value++;
                break;
            }
            value++;
        }
        /*
         * queue commands
         */
        /* store all values into SPIFFFS */
        if (!strcmp("SAV", cmd))
        {
            //  display_save_settings();
            // ioport_save_settings();
            measure_save_settings();
            // mqtt_save_settings();
            wificlient_save_settings();
            client->printf("status\\Save");
        }
        /**
         * send channel name list
         */
        if (!strcmp("get_channel_list", cmd))
        {
            // Iterar sobre todos los canales virtuales
            for (int i = 0; i < VIRTUAL_CHANNELS; i++)
            {
                // Obtener el nombre del canal una sola vez
                const char *channelName = measure_get_channel_name(i);

                // Enviar el nombre del canal al cliente
                client->printf("get_channel_list\\channel_%d_name\\%s", i, channelName);

                // Determinar si el canal está activo
                bool isChannelActive = (measure_get_channel_type(i) != NO_CHANNEL_TYPE) &&
                                       measure_get_group_active(measure_get_channel_group_id(i));

                // Enviar el estado del canal al cliente
                client->printf("get_channel_use_list\\channel%d\\%s\\channel\\%d\\%s",
                               i,
                               isChannelActive ? "true" : "false",
                               i,
                               channelName);
            }
        }
        else if (!strcmp("get_channel_config", cmd))
        {
            char tmp[64] = ""; // Buffer temporal para datos de configuración

            // Validar el canal seleccionado
            if (selectedchannel >= VIRTUAL_CHANNELS)
            {
                selectedchannel = 0; // Restablecer al canal 0 si es inválido
            }

            // Enviar la configuración básica del canal seleccionado
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

            // Opciones de canales disponibles
            for (int i = 0; i < VIRTUAL_CHANNELS; i++)
            {
                const char *channelName = measure_get_channel_name(i); // Obtener el nombre del canal
                if (strlen(channelName))
                { // Verificar si tiene un nombre válido
                    client->printf("option\\channel\\%d\\%s", i, channelName);
                }
            }

            // Opciones de grupos disponibles
            for (int i = 0; i < MAX_GROUPS; i++)
            {
                const char *groupName = measure_get_group_name(i); // Obtener el nombre del grupo
                if (strlen(groupName))
                { // Verificar si tiene un nombre válido
                    client->printf("option\\channel_group_id\\%d\\%s", i, groupName);
                }
            }
        }

        else if (!strcmp("get_wlan_settings", cmd))
        {
            // Obtener los valores necesarios en variables temporales
            const char *ssid = wificlient_get_ssid();
            const char *softap_ssid = wificlient_get_softap_ssid();
            bool enable_softap = wificlient_get_enable_softap();
            bool low_power = wificlient_get_low_power();
            bool low_bandwidth = wificlient_get_low_bandwidth();

            // Enviar las configuraciones al cliente
            client->printf("ssid\\%s", ssid);                                                // SSID de la red WiFi
            client->printf("password\\%s", "********");                                      // Marcador para la contraseña
            client->printf("checkbox\\enable_softap\\%s", enable_softap ? "true" : "false"); // Estado del SoftAP
            client->printf("softap_ssid\\%s", softap_ssid);                                  // SSID del SoftAP
            client->printf("softap_password\\%s", "********");                               // Marcador para la contraseña del SoftAP
            client->printf("checkbox\\low_power\\%s", low_power ? "true" : "false");         // Estado del modo de bajo consumo
            client->printf("checkbox\\low_bandwidth\\%s", low_bandwidth ? "true" : "false"); // Estado del modo de bajo ancho de banda
        }
        /*else if (!strcmp("get_mqtt_settings", cmd))
        {
            // Obtener valores de configuración de MQTT en variables temporales
            const char *server = mqtt_client_get_server();
            int port = mqtt_client_get_port();
            const char *username = mqtt_client_get_username();
            const char *topic = mqtt_client_get_topic();
            int interval = mqtt_client_get_interval();
            bool realtimeStats = mqtt_client_get_realtimestats();

            // Enviar las configuraciones de MQTT al cliente
            client->printf("mqtt_server\\%s", server);                                            // Dirección del servidor
            client->printf("mqtt_port\\%d", port);                                                // Puerto del servidor
            client->printf("mqtt_username\\%s", username);                                        // Nombre de usuario
            client->printf("mqtt_password\\%s", "********");                                      // Contraseña oculta
            client->printf("mqtt_topic\\%s", topic);                                              // Tópico MQTT
            client->printf("mqtt_interval\\%d", interval);                                        // Intervalo de envío
            client->printf("checkbox\\mqtt_realtimestats\\%s", realtimeStats ? "true" : "false"); // Estadísticas en tiempo real
        }*/

        else if (!strcmp("get_measurement_settings", cmd))
        {
            // Obtener valores de configuración de medición
            float networkFrequency = measure_get_network_frequency(); // Frecuencia de red
            //int samplerateCorrection = measure_get_samplerate_corr(); // Corrección de muestreo

            // Enviar los valores al cliente
            client->printf("network_frequency\\%f", networkFrequency);   // Frecuencia de red
            //client->printf("samplerate_corr\\%d", samplerateCorrection); // Corrección de la frecuencia de muestreo
        }

        else if (!strcmp("get_hostname_settings", cmd))
        {
            // Obtener el nombre de host configurado
            const char *hostname = wificlient_get_hostname();

            // Enviar el nombre de host al cliente
            client->printf("hostname\\%s", hostname);
        }

        else if (!strcmp("get_group_settings", cmd))
        {
            // Validar el grupo seleccionado
            if (selectedchannel >= MAX_GROUPS)
            {
                selectedchannel = 0; // Restablecer al grupo 0 si es inválido
            }

            // Obtener información básica del grupo seleccionado
            const char *groupName = measure_get_group_name(selectedchannel);
            bool groupActive = measure_get_group_active(selectedchannel);

            // Enviar información básica del grupo seleccionado
            client->printf("channel\\%d", selectedchannel);          // ID del canal seleccionado
            client->printf("group_name\\%s", groupName);             // Nombre del grupo
            client->printf("group_active\\%d", groupActive ? 1 : 0); // Estado del grupo (activo/inactivo)

            // Enviar opciones de grupos disponibles
            for (int i = 0; i < MAX_GROUPS; i++)
            {
                const char *optionGroupName = measure_get_group_name(i); // Nombre del grupo actual
                if (strlen(optionGroupName) > 0)
                { // Verificar si el nombre es válido
                    client->printf("option\\channel\\%d\\%s", i, optionGroupName);
                }
            }

            // Enviar lista de uso de grupos y nombres de canales asociados
            for (int i = 0; i < VIRTUAL_CHANNELS; i++)
            {
                const char *channelName = measure_get_channel_name(i); // Nombre del canal actual
                client->printf("get_group_use_list\\channel%d_%d\\true\\channel_%d_name\\%s",
                               i,
                               measure_get_channel_group_id(i),
                               i,
                               channelName);
            }

            // Enviar etiquetas de nombres de todos los grupos
            for (int i = 0; i < MAX_GROUPS; i++)
            {
                const char *labelGroupName = measure_get_group_name(i); // Nombre del grupo actual
                client->printf("label\\group_%d_name\\%s", i, labelGroupName);
            }
        }

        else if (!strcmp("channel_group", cmd))
        {
            if (value)
            {                          // Validar que el valor no sea NULL
                char *group = value;   // Puntero al valor recibido
                int channel_count = 0; // Contador de canales procesados

                /**
                 * Iterar sobre los caracteres en el valor recibido para asignar grupos a canales.
                 */
                while (*group && channel_count < VIRTUAL_CHANNELS)
                { // Limitar a los canales válidos
                    if (*group >= '0' && *group <= '5')
                    {                                                          // Validar que el carácter representa un ID de grupo válido (0-5)
                        int group_id = *group - '0';                           // Convertir el carácter a un entero (ID de grupo)
                        measure_set_channel_group_id(channel_count, group_id); // Asignar el grupo al canal actual
                    }
                    channel_count++; // Incrementar el contador de canales
                    group++;         // Avanzar al siguiente carácter
                }
            }
        }

        /* Procesar comando "OSC" */
        else if (!strcmp("OSC", cmd))
        {
            if (value)
            { // Validar que la entrada no sea NULL
                char request[numbersOfSamples * VIRTUAL_CHANNELS + numbersOfFFTSamples * VIRTUAL_CHANNELS + 96] = "";
                char tmp[64] = "";
                uint16_t *mybuffer;
                int active_channel_count = 0;
                int SampleScale = 4;
                int FFTScale = 1;

                // Contar canales activos
                for (char *ptr = value; *ptr; ptr++)
                {
                    if (*ptr == '1')
                        active_channel_count++;
                }

                // Ajustar escala según el número de canales activos
                if (active_channel_count < 8)
                    SampleScale = 2;
                if (active_channel_count < 5)
                    SampleScale = 1;

                // Construir datos iniciales
                snprintf(tmp, sizeof(tmp), "OScopeProbe\\%d\\%d\\",
                         numbersOfSamples / SampleScale,
                         numbersOfFFTSamples / FFTScale);
                strncat(request, tmp, sizeof(request));

                // Procesar muestras
                mybuffer = measure_get_buffer();
                for (int channel = 0; channel < VIRTUAL_CHANNELS; channel++)
                {
                    if (*(value + channel) == '0')
                        continue;

                    for (int i = 0; i < numbersOfSamples; i += SampleScale)
                    {
                                uint16_t sample_value = mybuffer[numbersOfSamples * channel + i];

                        snprintf(tmp, sizeof(tmp), "%03x",
                                 mybuffer[numbersOfSamples * channel + i] > 0x0fff ? 0x0fff : mybuffer[numbersOfSamples * channel + i]);
                        strncat(request, tmp, sizeof(request));
                    }
                }
                strncat(request, "\\", sizeof(request));

                // Obtener datos FFT y construir el bloque de datos solo para los canales seleccionados
                mybuffer = measure_get_fft();

                // Canales específicos que queremos procesar
                for (int channel = 0; channel < VIRTUAL_CHANNELS; channel++)
                {   
                    if (*(value + channel) == '0')
                        continue;
                    // Procesar únicamente los canales seleccionados
                    for (int i = 0; i < numbersOfFFTSamples; i += FFTScale)
                    {
                        snprintf(tmp, sizeof(tmp), "%03x",
                                 mybuffer[numbersOfFFTSamples * channel + i] > 0x0fff ? 0x0fff : mybuffer[numbersOfFFTSamples * channel + i]);
                        strncat(request, tmp, sizeof(request));
                    }
                }

                strncat(request, "\\", sizeof(request));

                // Convertir canales activos a hexadecimal
                char activeChannelsBinary[VIRTUAL_CHANNELS + 1] = {0};
                for (int channel = 0; channel < VIRTUAL_CHANNELS; channel++)
                {
                    activeChannelsBinary[channel] = (*(value + channel) == '0') ? '0' : '1';
                }
                char hexChannels[8];
                snprintf(hexChannels, sizeof(hexChannels), "%03X", strtol(activeChannelsBinary, NULL, 2));
                strncat(request, hexChannels, sizeof(request));

                // Enviar respuesta
                client->text(request);
            }
        }
        /* Obtener la línea de estado (STS) */
        else if (!strcmp("STS", cmd))
        {
            char request[1024] = ""; // Buffer para construir la respuesta final
            char tmp[128] = "";      // Buffer temporal para datos intermedios

            // Iniciar la respuesta con el estado general
            snprintf(request, sizeof(request), "status\\online (");

            // Iterar sobre los grupos para incluir su estado
            for (int group_id = 0; group_id < MAX_GROUPS; group_id++)
            {
                int power_channel = -1;          // Canal de potencia activa
                int reactive_power_channel = -1; // Canal de potencia reactiva
                float cos_phi = 1.0f;            // Factor de potencia inicializado

                // Verificar si el grupo está activo
                if (!measure_get_group_active(group_id))
                    continue;

                // Verificar si el grupo tiene canales asignados
                if (!measure_get_channel_group_id_entrys(group_id))
                    continue;

                // Incluir el nombre del grupo en la respuesta
                snprintf(tmp, sizeof(tmp), "\r\n %s:[ ", measure_get_group_name(group_id));
                strncat(request, tmp, sizeof(request));

                // Iterar sobre los canales para incluir información por grupo
                for (int channel = 0; channel < VIRTUAL_CHANNELS; channel++)
                {
                    if (measure_get_channel_group_id(channel) != group_id)
                        continue;

                    // Identificar los canales de potencia activa y reactiva
                    if (measure_get_channel_type(channel) == AC_POWER)
                    {
                        power_channel = channel;
                    }
                    if (measure_get_channel_type(channel) == AC_REACTIVE_POWER)
                    {
                        reactive_power_channel = channel;
                    }

                    // Calcular el factor de potencia si ambos canales están disponibles
                    if (power_channel != -1 && reactive_power_channel != -1)
                    {
                        float active_power = measure_get_channel_rms(power_channel) +
                                             measure_get_channel_rms(reactive_power_channel);
                        cos_phi = active_power / measure_get_channel_rms(power_channel);
                    }

                    // Construir información específica del canal
                    tmp[0] = '\0'; // Limpiar el buffer temporal
                    switch (measure_get_channel_type(channel))
                    {
                    case AC_VOLTAGE:
                    case DC_VOLTAGE:
                        snprintf(tmp, sizeof(tmp),
                                 " U=%.1f%s | U3=%.1f%s | U5=%.1f%s | U7=%.1f%s | U9=%.1f%s | THDV=%.1f%s |",
                                 measure_get_channel_rms(channel), measure_get_channel_report_unit(channel),
                                 harmonic_values[channel].thirdHarmonic, measure_get_channel_report_unit(channel),
                                 harmonic_values[channel].fifthHarmonic, measure_get_channel_report_unit(channel),
                                 harmonic_values[channel].seventhHarmonic, measure_get_channel_report_unit(channel),
                                 harmonic_values[channel].ninthHarmonic, measure_get_channel_report_unit(channel),
                                 harmonic_values[channel].thd, harmonic_values[channel].porcentaje);
                        break;

                    case AC_CURRENT:
                    case DC_CURRENT:
                        snprintf(tmp, sizeof(tmp),
                                 " I=%.2f%s | I3=%.2f%s | I5=%.2f%s | I7=%.2f%s | I9=%.2f%s | THDI=%.1f%s |",
                                 measure_get_channel_rms(channel), measure_get_channel_report_unit(channel),
                                 harmonic_values[channel].thirdHarmonic, measure_get_channel_report_unit(channel),
                                 harmonic_values[channel].fifthHarmonic, measure_get_channel_report_unit(channel),
                                 harmonic_values[channel].seventhHarmonic, measure_get_channel_report_unit(channel),
                                 harmonic_values[channel].ninthHarmonic, measure_get_channel_report_unit(channel),
                                 harmonic_values[channel].thd, harmonic_values[channel].porcentaje);
                        break;

                    case AC_POWER:
                    case DC_POWER:
                        snprintf(tmp, sizeof(tmp), " P=%.2f%s |",
                                 measure_get_channel_rms(channel), measure_get_channel_report_unit(channel));
                        break;

                    case AC_REACTIVE_POWER:
                        snprintf(tmp, sizeof(tmp), " Pvar=%.2f%s |",
                                 measure_get_channel_rms(channel), measure_get_channel_report_unit(channel));
                        break;

                    default:
                        tmp[0] = '\0'; // Si no es un tipo reconocido, dejar vacío
                    }
                    strncat(request, tmp, sizeof(request)); // Agregar información al buffer principal
                }

                // Agregar el factor de potencia (cos_phi) si se calcula
                if (power_channel != -1 && reactive_power_channel != -1)
                {
                    snprintf(tmp, sizeof(tmp), " Cos=%.2f ", cos_phi);
                    strncat(request, tmp, sizeof(request));
                }

                // Finalizar el bloque del grupo
                strncat(request, "]", sizeof(request));
            }

            // Agregar información de frecuencia si hay canales de voltaje presentes
            for (int i = 0; i < VIRTUAL_CHANNELS; i++)
            {
                if (measure_get_channel_type(i) == AC_VOLTAGE)
                {
                    snprintf(tmp, sizeof(tmp), " ; f = %.1f Hz ", measure_get_max_freq());
                    strncat(request, tmp, sizeof(request));
                    break;
                }
            }

            // Finalizar la respuesta y enviarla al cliente
            strncat(request, " )", sizeof(request));
            client->printf(request);
        }

        /* Configurar valores relacionados con WLAN y MQTT */
        if (value)
        { // Validar que el valor no sea NULL
            if (!strcmp("hostname", cmd))
            {
                // Configurar el nombre de host para la red WiFi
                wificlient_set_hostname(value);
            }
            else if (!strcmp("softap_ssid", cmd))
            {
                // Configurar el SSID del SoftAP
                wificlient_set_softap_ssid(value);
            }
            else if (!strcmp("softap_password", cmd))
            {
                // Configurar la contraseña del SoftAP (solo si no es un marcador genérico)
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
                // Configurar la contraseña de la red WiFi (solo si no es un marcador genérico)
                if (strcmp("********", value))
                {
                    wificlient_set_password(value);
                }
            }
            else if (!strcmp("enable_softap", cmd))
            {
                // Activar o desactivar el modo SoftAP
                bool enableSoftAP = atoi(value) ? true : false;
                wificlient_set_enable_softap(enableSoftAP);
            }
            else if (!strcmp("low_power", cmd))
            {
                // Activar o desactivar el modo de bajo consumo
                bool lowPower = atoi(value) ? true : false;
                wificlient_set_low_power(lowPower);
            }
            else if (!strcmp("low_bandwidth", cmd))
            {
                // Activar o desactivar el modo de bajo ancho de banda
                bool lowBandwidth = atoi(value) ? true : false;
                wificlient_set_low_bandwidth(lowBandwidth);
            }
        }

        /* Procesar comandos relacionados con MQTT y mediciones */
        if (value)
        { // Validar que el valor no sea NULL
            /* Configuraciones del servidor MQTT */
            /* if (!strcmp("mqtt_server", cmd))
             {
                 // Establecer la dirección del servidor MQTT
                 mqtt_client_set_server(value);
             }
             else if (!strcmp("mqtt_port", cmd))
             {
                 // Establecer el puerto del servidor MQTT
                 mqtt_client_set_port(atoi(value));
             }
             else if (!strcmp("mqtt_username", cmd))
             {
                 // Establecer el nombre de usuario para MQTT
                 mqtt_client_set_username(value);
             }
             else if (!strcmp("mqtt_password", cmd))
             {
                 // Establecer la contraseña MQTT (solo si no es un marcador genérico)
                 if (strcmp("********", value))
                 {
                     mqtt_client_set_password(value);
                 }
             }
             else if (!strcmp("mqtt_topic", cmd))
             {
                 // Establecer el tópico MQTT
                 mqtt_client_set_topic(value);
             }
             else if (!strcmp("mqtt_interval", cmd))
             {
                 // Establecer el intervalo de envío de datos MQTT
                 mqtt_client_set_interval(atoi(value));
             }
             else if (!strcmp("mqtt_realtimestats", cmd))
             {
                 // Habilitar o deshabilitar el envío de estadísticas en tiempo real por MQTT
                 mqtt_client_set_realtimestats(atoi(value) ? true : false);
             }*/
            /* Configuraciones de frecuencia de muestreo */
            if (!strcmp("samplerate_corr", cmd))
            {
                // Establecer la corrección de frecuencia de muestreo
                measure_set_samplerate_corr(atoi(value));
            }
            else if (!strcmp("network_frequency", cmd))
            {
                // Establecer la frecuencia de red (en Hz)
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

            /* Configuraciones de canales */
            else if (!strcmp("PS+", cmd))
            {
                // Incrementar el desfasaje de fase del canal seleccionado
                measure_set_channel_phaseshift(selectedchannel, measure_get_channel_phaseshift(selectedchannel) + 1);
            }
            else if (!strcmp("PS-", cmd))
            {
                // Disminuir el desfasaje de fase del canal seleccionado
                measure_set_channel_phaseshift(selectedchannel, measure_get_channel_phaseshift(selectedchannel) - 1);
            }
            else if (!strcmp("channel", cmd))
            {
                // Seleccionar un canal
                selectedchannel = atoi(value);
            }
            else if (!strcmp("channel_type", cmd))
            {
                // Establecer el tipo de canal
                measure_set_channel_type(selectedchannel, (channel_type_t)atoi(value));
            }
            else if (!strcmp("channel_report_exp", cmd))
            {
                // Establecer el exponente de reporte para el canal
                measure_set_channel_report_exp(selectedchannel, atoi(value));
            }
            else if (!strcmp("channel_phaseshift", cmd))
            {
                // Establecer el desfasaje de fase del canal
                measure_set_channel_phaseshift(selectedchannel, atoi(value));
            }
            else if (!strcmp("channel_true_rms", cmd))
            {
                // Establecer si el canal reporta valores RMS verdaderos
                measure_set_channel_true_rms(selectedchannel, atoi(value) ? true : false);
            }
            else if (!strcmp("channel_opcodeseq_str", cmd))
            {
                // Establecer la secuencia de operación del canal
                measure_set_channel_opcodeseq_str(selectedchannel, value);
            }
            else if (!strcmp("channel_offset", cmd))
            {
                // Establecer el offset del canal
                measure_set_channel_offset(selectedchannel, atof(value));
            }
            else if (!strcmp("channel_ratio", cmd))
            {
                // Establecer el ratio del canal
                measure_set_channel_ratio(selectedchannel, atof(value));
            }
            else if (!strcmp("channel_name", cmd))
            {
                // Establecer el nombre del canal
                measure_set_channel_name(selectedchannel, value);
            }
            else if (!strcmp("channel_group_id", cmd))
            {
                // Asignar el canal a un grupo
                measure_set_channel_group_id(selectedchannel, atoi(value));
            }

            /* Configuraciones de grupos */
            else if (!strcmp("group_name", cmd))
            {
                // Establecer el nombre del grupo
                measure_set_group_name(selectedchannel, value);
            }
            else if (!strcmp("group_active", cmd))
            {
                // Activar o desactivar un grupo
                measure_set_group_active(selectedchannel, atoi(value) ? true : false);
            }
        }
        free(cmd);
    }
    }
}

/*
 * based on: https://github.com/lbernstone/asyncUpdate/blob/master/AsyncUpdate.ino
 */
static void handleUpdate(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
{
    // mqtt_client_disable();

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
            // mqtt_save_settings();
            wificlient_save_settings();
            ESP.restart();
        }
    }
    // void mqtt_client_enable();
}
/**
 * Configuración inicial del servidor web asíncrono.
 */
static void asyncwebserver_setup(void)
{
    // Ruta "/info": Devuelve información del firmware, versión del compilador y fecha de compilación.
    asyncserver.on("/info", HTTP_GET, [](AsyncWebServerRequest *request)
                   { request->send(200, "text/plain",
                                   "Firmwarestand: " __DATE__ " " __TIME__ "\r\n"
                                   "GCC-Version: " __VERSION__ "\r\n"
                                   "Version: " __FIRMWARE__ "\r\n"); });

    // Habilitar el editor de SPIFFS para gestionar el sistema de archivos.
    asyncserver.addHandler(new SPIFFSEditor(SPIFFS));

    // Configurar la ruta raíz para servir archivos estáticos desde SPIFFS.
    asyncserver.serveStatic("/", SPIFFS, "/").setDefaultFile("index.htm");

    // Manejar solicitudes para rutas no encontradas (404).
    asyncserver.onNotFound([](AsyncWebServerRequest *request)
                           {
                               request->send(404); // Responder con un código de estado HTTP 404.
                           });

    // Manejo de cargas de archivos.
    asyncserver.onFileUpload([](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
                             {
        if (!index) {
            log_i("UploadStart: %s", filename.c_str()); // Registro al inicio de la carga.
        }
        log_i("%s", (const char *)data); // Registro de datos cargados.
        if (final) {
            log_i("UploadEnd: %s (%u)", filename.c_str(), index + len); // Registro al finalizar la carga.
        } });

    // Manejo del cuerpo de las solicitudes HTTP.
    asyncserver.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                              {
        if (!index) {
            log_i("BodyStart: %u", total); // Registro al inicio del cuerpo.
        }
        log_i("%s", (const char *)data); // Registro del contenido del cuerpo.
        if (index + len == total) {
            log_i("BodyEnd: %u\n", total); // Registro al finalizar el cuerpo.
        } });

    // Ruta "/default": Restablece configuraciones predeterminadas y reinicia el dispositivo.
    asyncserver.on("/default", HTTP_GET, [](AsyncWebServerRequest *request)
                   {
                       AsyncWebServerResponse *response = request->beginResponse(302, "text/plain",
                                                                                 "Reset to default. Please wait while the powermeter reboots\r\n");
                       response->addHeader("Refresh", "20"); // Redirección automática después de 20 segundos.
                       response->addHeader("Location", "/");
                       request->send(response);

                       // Eliminar archivos de configuración específicos.
                       SPIFFS.remove("powermeter.json");
                       SPIFFS.remove("measure.json");

                       delay(3000);   // Pausa antes de reiniciar.
                       ESP.restart(); // Reiniciar el dispositivo.
                   });

    // Ruta "/memory": Muestra detalles del uso de memoria del sistema.
    asyncserver.on("/memory", HTTP_GET, [](AsyncWebServerRequest *request)
                   {
        String html = "<html><head><meta charset=\"utf-8\"></head><body><h3>Memory Details</h3>"
                      "<b>Heap size: </b>" +
                      String(ESP.getHeapSize()) + "<br>" +
                      "<b>Heap free: </b>" + String(ESP.getFreeHeap()) + "<br>" +
                      "<b>Heap free min: </b>" + String(ESP.getMinFreeHeap()) + "<br>" +
                      "<b>Psram size: </b>" + String(ESP.getPsramSize()) + "<br>" +
                      "<b>Psram free: </b>" + String(ESP.getFreePsram()) + "<br>" +
                      "<br><b><u>System</u></b><br>" +
                      "<b>Uptime: </b>" + String(millis() / 1000) + " seconds<br>" +
                      "</body></html>";
        request->send(200, "text/html", html); });

    // Ruta "/reset": Reinicia el dispositivo.
    asyncserver.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request)
                   {
                       AsyncWebServerResponse *response = request->beginResponse(302, "text/plain",
                                                                                 "Please wait while the powermeter reboots");
                       response->addHeader("Refresh", "20"); // Redirección automática después de 20 segundos.
                       response->addHeader("Location", "/");
                       request->send(response);

                       delay(3000);   // Pausa antes de reiniciar.
                       ESP.restart(); // Reiniciar el dispositivo.
                   });

    // Ruta "/update" (GET): Muestra la interfaz para la actualización OTA.
    asyncserver.on("/update", HTTP_GET, [](AsyncWebServerRequest *request)
                   { request->send(200, "text/html", serverIndex); });

    // Ruta "/update" (POST): Maneja la carga y actualización del firmware.
    asyncserver.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {}, // Sin acción en la solicitud inicial.
                   [](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
                   {
                       handleUpdate(request, filename, index, data, len, final); // Manejar actualización OTA.
                   });

    // Configuración de WebSocket con su manejador de eventos.
    ws.onEvent(onWsEvent);
    asyncserver.addHandler(&ws);

    // Iniciar el servidor web asíncrono.
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
