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

static void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len) {
    static int selectedchannel = 0;

    switch (type) {
        case WS_EVT_CONNECT: { break; }
        case WS_EVT_ERROR: { break; }
        case WS_EVT_PONG: { break; }
        case WS_EVT_DISCONNECT: { break; }
        case WS_EVT_DATA: {
        /*
         * copy data into an separate allocated buffer and terminate it with \0
         */
        char *cmd = (char*)calloc( len+1, sizeof(uint8_t) );
        memcpy( cmd, data, len );
        /*
         * separate command and his correspondening value
         */
        char *value = cmd;
        while( *value ) {
            if ( *value == '\\' ) {
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
        if ( !strcmp("SAV", cmd ) ) {
          //  display_save_settings();
            //ioport_save_settings();
            measure_save_settings();
            mqtt_save_settings();
            wificlient_save_settings();
            client->printf("status\\Save" );
        }
        /**
         * send channel name list
         */
        if ( !strcmp("get_channel_list", cmd ) ) {
            for( int i = 0 ; i < VIRTUAL_CHANNELS ; i++ ) {
                client->printf("get_channel_list\\channel_%d_name\\%s", i, measure_get_channel_name( i ) );
                client->printf("get_channel_use_list\\channel%d\\%s\\channel\\%d\\%s", i, ( measure_get_channel_type( i ) != NO_CHANNEL_TYPE ) && measure_get_group_active( measure_get_channel_group_id( i ) ) ? "true" : "false", i, measure_get_channel_name( i ) );
            }
        }
        else if ( !strcmp("get_channel_config", cmd ) ) {
            char tmp[64]="";
            if( selectedchannel >= VIRTUAL_CHANNELS )
                selectedchannel = 0;
            client->printf("channel\\%d", selectedchannel );
            client->printf("channel_type\\%01x", measure_get_channel_type( selectedchannel ) );
            client->printf("checkbox\\channel_true_rms\\%s", measure_get_channel_true_rms( selectedchannel ) ? "true" : "false" );
            client->printf("channel_report_exp\\%d", measure_get_channel_report_exp( selectedchannel ) );
            client->printf("channel_phaseshift\\%d", measure_get_channel_phaseshift( selectedchannel ) );
            client->printf("channel_opcodeseq_str\\%s", measure_get_channel_opcodeseq_str( selectedchannel, sizeof( tmp ), tmp ) );
            client->printf("old_channel_opcodeseq_str\\%s", measure_get_channel_opcodeseq_str( selectedchannel, sizeof( tmp ), tmp ) );
            client->printf("channel_offset\\%f", measure_get_channel_offset( selectedchannel ) );
            client->printf("channel_ratio\\%f", measure_get_channel_ratio( selectedchannel ) );
            client->printf("channel_name\\%s", measure_get_channel_name( selectedchannel ) );
            client->printf("channel_group_id\\%d", measure_get_channel_group_id( selectedchannel ) );
            for( int i = 0 ; i < VIRTUAL_CHANNELS ; i++ ) {
                if( strlen( measure_get_channel_name( i ) ) )
                    client->printf("option\\channel\\%d\\%s", i, measure_get_channel_name( i ) );                    
            }
            for( int i = 0 ; i < MAX_GROUPS ; i++ ) {
                if( strlen( measure_get_group_name( i ) ) )
                    client->printf("option\\channel_group_id\\%d\\%s", i, measure_get_group_name( i ) );                    
            }

        }
        else if ( !strcmp("get_wlan_settings", cmd ) ) {
            client->printf("ssid\\%s", wificlient_get_ssid() );
            client->printf("password\\%s", "********" );
            client->printf("checkbox\\enable_softap\\%s", wificlient_get_enable_softap() ? "true" : "false " );
            client->printf("softap_ssid\\%s", wificlient_get_softap_ssid() );
            client->printf("softap_password\\%s", "********" );
            client->printf("checkbox\\low_power\\%s", wificlient_get_low_power() ? "true" : "false ");
            client->printf("checkbox\\low_bandwidth\\%s", wificlient_get_low_bandwidth() ? "true" : "false ");
        }
        else if ( !strcmp("get_mqtt_settings", cmd ) ) {
            client->printf("mqtt_server\\%s", mqtt_client_get_server() );
            client->printf("mqtt_port\\%d", mqtt_client_get_port() );
            client->printf("mqtt_username\\%s", mqtt_client_get_username() );
            client->printf("mqtt_password\\%s", "********" );
            client->printf("mqtt_topic\\%s", mqtt_client_get_topic() );
            client->printf("mqtt_interval\\%d", mqtt_client_get_interval() );
            client->printf("checkbox\\mqtt_realtimestats\\%s", mqtt_client_get_realtimestats()? "true" : "false" );
        }
        else if ( !strcmp("get_measurement_settings", cmd ) ) {
            client->printf("network_frequency\\%f", measure_get_network_frequency() );
            client->printf("samplerate_corr\\%d", measure_get_samplerate_corr() );
        }
        else if ( !strcmp("get_hostname_settings", cmd ) ) {
            client->printf("hostname\\%s", wificlient_get_hostname() );
        }
        else if ( !strcmp("get_group_settings", cmd ) ) {
            if( selectedchannel >= MAX_GROUPS )
                selectedchannel = 0;
            client->printf("channel\\%d", selectedchannel );
            client->printf("group_name\\%s", measure_get_group_name( selectedchannel ) );
            client->printf("group_active\\%d", measure_get_group_active( selectedchannel ) ? 1 : 0 );
            for( int i = 0 ; i < MAX_GROUPS ; i++ ) {
                if( strlen( measure_get_group_name( i ) ) )
                    client->printf("option\\channel\\%d\\%s", i, measure_get_group_name( i ) );
            }
            for( int i = 0 ; i < VIRTUAL_CHANNELS ; i++ )
                client->printf("get_group_use_list\\channel%d_%d\\true\\channel_%d_name\\%s", i, measure_get_channel_group_id( i ), i, measure_get_channel_name( i ) );
            for( int i = 0 ; i < MAX_GROUPS ; i++ )
                client->printf("label\\group_%d_name\\%s", i, measure_get_group_name( i ) );
        }
        else if ( !strcmp("channel_group", cmd ) ) {
            char * group = value;
            int channel_count = 0;
            /**
             * count active channels
             */
            while( *group ) {
                if( *group >= '0' && *group <= '5' ) {
                    int group_id = *group - '0';
                    measure_set_channel_group_id( channel_count, group_id );
                }
                channel_count++;
                group++;
            }       
        }
        /* get samplebuffer */
        else if ( !strcmp("OSC", cmd ) ) {
            char request[ numbersOfSamples * VIRTUAL_CHANNELS + numbersOfFFTSamples * VIRTUAL_CHANNELS + 96 ]="";
            char tmp[64]="";
            //char tmp[128]="";
            uint16_t * mybuffer;
            char * active_channels = value;
            int active_channel_count = 0;
            int SampleScale=4;
            int FFTScale=1;
            /**
             * count active channels
             */
            
            while( *active_channels ) {
                if( *active_channels == '1' )
                    active_channel_count++;
                active_channels++;
            }
           if( active_channel_count < 8 )
                SampleScale = 2;
           if( active_channel_count < 5 )
                SampleScale = 1;
            /**
             * send first info data
             */
            snprintf( tmp, sizeof( tmp ), "OScopeProbe\\%d\\%d\\%d\\%f\\", active_channel_count , numbersOfSamples / SampleScale, numbersOfFFTSamples / FFTScale, 0.01 );
            strncat( request, tmp, sizeof( request ) );
            /**
             * get sample buffers and build first data block
             */
            //mybuffer = measure_get_channel_rms();
            mybuffer = measure_get_buffer();
            //printf("%u\n", measure_get_buffer());
            for( int channel = 0 ; channel < VIRTUAL_CHANNELS ; channel++ ) {
                if( *( value + channel ) == '0' )
                    continue;

                for( int i = 0 ; i < numbersOfSamples ; i = i + SampleScale ) {    
                
                    //snprintf(tmp, sizeof(tmp), "U=%.1f%s ", measure_get_channel_rms(channel), measure_get_channel_report_unit(channel));              
                    snprintf( tmp, sizeof( tmp ), "%03x", mybuffer[ numbersOfSamples * channel + i ] > 0x0fff ? 0x0fff : mybuffer[ numbersOfSamples * channel + i ] );
                    strncat( request, tmp, sizeof( request ) );
                    //log_e("Temp: %s \n ", mybuffer[numbersOfSamples * channel + i]);
                    //printf("%u\n", mybuffer[numbersOfSamples * channel + i]);
                    //printf("Temp canal %d: %s\n",channel, tmp);

                }
            }
            strncat( request, "\\", sizeof( request ) );
            mybuffer = measure_get_fft();
            /**
             * get fft buffers and build next data block
             */
            for( int channel = 0 ; channel < VIRTUAL_CHANNELS ; channel++ ) {
                if( *( value + channel ) == '0' )
                    continue;

                for( int i = 0 ; i < numbersOfFFTSamples ; i = i + FFTScale ) {
                    snprintf( tmp, sizeof( tmp ), "%03x", mybuffer[ numbersOfFFTSamples * channel + i ] > 0x0fff ? 0x0fff : mybuffer[ numbersOfFFTSamples * channel + i ] );
                    strncat( request, tmp, sizeof( request ) );
                }
            }
            strncat( request, "\\", sizeof( request ) );
            /**
             * get channel data types and build data data block
             */
char activeChannelsBinary[14]; // Almacenar los valores de los canales activos
for (int channel = 0; channel < VIRTUAL_CHANNELS; channel++) {
    if (*(value + channel) == '0') {
        activeChannelsBinary[channel] = '0';
    } else {
        activeChannelsBinary[channel] = '1';
    }
}
activeChannelsBinary[VIRTUAL_CHANNELS] = '\0'; // Finalizar la cadena

// Ahora convierte la representación binaria a hexadecimal para reducir la longitud del mensaje.
char hexChannels[8]; // Necesitamos una representación hexadecimal de los 13 canales
int binaryValue = strtol(activeChannelsBinary, NULL, 2); // Convertir de binario a un entero
snprintf(hexChannels, sizeof(hexChannels), "%03X", binaryValue); // Convertir el entero en hexadecimal

strncat(request, hexChannels, sizeof(request));

            
            client->text( request );
        }
        /* get status-line */
        else if ( !strcmp("STS", cmd ) ) {
            
            char request[1024]="";
            char tmp[128]="";

            snprintf( request, sizeof( request ), "status\\online (" );
            
            for( int group_id = 0 ; group_id < MAX_GROUPS ; group_id++ ) {
                int power_channel = -1;
                int reactive_power_channel = -1;
                float cos_phi = 1.0f;

                if( !measure_get_group_active( group_id ) )
                    continue;

                if( !measure_get_channel_group_id_entrys( group_id ) )
                    continue;
                        
                strncat( tmp, "\r\n", sizeof( tmp ) - strlen( tmp ) - 1 );  // Salto de línea
                snprintf( tmp, sizeof( tmp ), " %s:[ ", measure_get_group_name( group_id ) );
                strncat( request, tmp, sizeof( request ) );

                for( int channel = 0 ; channel < VIRTUAL_CHANNELS ; channel++ ) {
                    if( measure_get_channel_group_id( channel ) == group_id ) {

                        if( measure_get_channel_type( channel ) == AC_POWER )
                            power_channel = channel;
                        if( measure_get_channel_type( channel ) == AC_REACTIVE_POWER )
                            reactive_power_channel = channel;

                        if( power_channel != -1 && reactive_power_channel != -1 ) {
                            float active_power = measure_get_channel_rms( power_channel ) + measure_get_channel_rms( reactive_power_channel );
                            cos_phi = active_power / measure_get_channel_rms( power_channel );
                        }
                    
                        switch( measure_get_channel_type( channel ) ) {
                            case AC_VOLTAGE:
                            case DC_VOLTAGE:
                               // Primero, limpiar el buffer tmp para evitar problemas de contenido previo.
                            tmp[0] = '\0'; // Reinicia el contenido de tmp

                            // Concatenar el valor RMS del canal
                            snprintf(tmp, sizeof(tmp), "U=%.1f%s ", measure_get_channel_rms(channel), measure_get_channel_report_unit(channel));
                            //printf("%f \n",measure_get_channel_rms(channel));
                            // Concatenar el valor del tercer armónico
                            strncat(tmp, "| U3=", sizeof(tmp) - strlen(tmp) - 1);
                            char thirdHarmonicStr[32];
                            snprintf(thirdHarmonicStr, sizeof(thirdHarmonicStr), "%.1f%s ", harmonic_values[channel].thirdHarmonic, measure_get_channel_report_unit(channel));
                            strncat(tmp, thirdHarmonicStr, sizeof(tmp) - strlen(tmp) - 1);
                                                        
                            // strncat(tmp, " - ", sizeof(tmp) - strlen(tmp) - 1);
                            // char cf3[32];
                            // snprintf(cf3, sizeof(cf3), "%.1f%s ", harmonic_values[channel].fthird, harmonic_values[channel].uF);
                            // strncat(tmp, cf3, sizeof(tmp) - strlen(tmp) - 1);

                            // Concatenar el valor del quinto armónico
                            strncat(tmp, "| U5=", sizeof(tmp) - strlen(tmp) - 1);
                            char fifthHarmonicStr[32];
                            snprintf(fifthHarmonicStr, sizeof(fifthHarmonicStr), "%.1f%s ", harmonic_values[channel].fifthHarmonic, measure_get_channel_report_unit(channel));
                            strncat(tmp, fifthHarmonicStr, sizeof(tmp) - strlen(tmp) - 1);
                            
                            // strncat(tmp, " - ", sizeof(tmp) - strlen(tmp) - 1);
                            // char cf5[32];
                            // snprintf(cf5, sizeof(cf5), "%.1f%s ", harmonic_values[channel].ffifth, harmonic_values[channel].uF);
                            // strncat(tmp, cf5, sizeof(tmp) - strlen(tmp) - 1);

                            strncat(tmp, "| U7=", sizeof(tmp) - strlen(tmp) - 1);
                            char seventhHarmonicStr[32];
                            snprintf(seventhHarmonicStr, sizeof(seventhHarmonicStr), "%.1f%s ", harmonic_values[channel].seventhHarmonic, measure_get_channel_report_unit(channel));
                            strncat(tmp, seventhHarmonicStr, sizeof(tmp) - strlen(tmp) - 1);

                            // strncat(tmp, " - ", sizeof(tmp) - strlen(tmp) - 1);
                            // char cf7[32];
                            // snprintf(cf7, sizeof(cf7), "%.1f%s ", harmonic_values[channel].fsevent, harmonic_values[channel].uF);
                            // strncat(tmp, cf7, sizeof(tmp) - strlen(tmp) - 1);

                            strncat(tmp, "| U9=", sizeof(tmp) - strlen(tmp) - 1);
                            char ninethHarmonicStr[32];
                            snprintf(ninethHarmonicStr, sizeof(ninethHarmonicStr), "%.1f%s ", harmonic_values[channel].ninthHarmonic, measure_get_channel_report_unit(channel));
                            strncat(tmp, ninethHarmonicStr, sizeof(tmp) - strlen(tmp) - 1);

                           /* strncat(tmp, " - ", sizeof(tmp) - strlen(tmp) - 1);
                            char cf9[32];
                            snprintf(cf9, sizeof(cf9), "%.2f%s ", harmonic_values[channel].fninth, harmonic_values[channel].uF);
                            strncat(tmp, cf9, sizeof(tmp) - strlen(tmp) - 1);*/

                            strncat(tmp, "| THDV=", sizeof(tmp) - strlen(tmp) - 1);
                            char tdhv[32];
                            snprintf(tdhv, sizeof(tdhv), "%.1f%s ", harmonic_values[channel].thd, harmonic_values[channel].porcentaje);
                            strncat(tmp, tdhv, sizeof(tmp) - strlen(tmp) - 1);


                            break;
                        case AC_CURRENT:
                        case DC_CURRENT:
                            snprintf(tmp, sizeof(tmp), " | I=%.2f%s ", measure_get_channel_rms(channel), measure_get_channel_report_unit(channel));
                            
                            // Concatenar el valor del tercer armónico
                            strncat(tmp, "| I3=", sizeof(tmp) - strlen(tmp) - 1);
                            char thirdHarmonicStr2[32];
                            snprintf(thirdHarmonicStr2, sizeof(thirdHarmonicStr2), "%.2f%s ", harmonic_values[channel].thirdHarmonic, measure_get_channel_report_unit(channel));
                            strncat(tmp, thirdHarmonicStr2, sizeof(tmp) - strlen(tmp) - 1);
                                                        
                            // strncat(tmp, " - ", sizeof(tmp) - strlen(tmp) - 1);
                            // char cf32[32];
                            // snprintf(cf32, sizeof(cf32), "%.1f%s ", harmonic_values[channel].fthird, harmonic_values[channel].uF);
                            // strncat(tmp, cf32, sizeof(tmp) - strlen(tmp) - 1);

                            // Concatenar el valor del quinto armónico
                            strncat(tmp, "| I5=", sizeof(tmp) - strlen(tmp) - 1);
                            char fifthHarmonicStr2[32];
                            snprintf(fifthHarmonicStr2, sizeof(fifthHarmonicStr2), "%.2f%s ", harmonic_values[channel].fifthHarmonic, measure_get_channel_report_unit(channel));
                            strncat(tmp, fifthHarmonicStr2, sizeof(tmp) - strlen(tmp) - 1);
                            
                            // strncat(tmp, " - ", sizeof(tmp) - strlen(tmp) - 1);
                            // char cf52[32];
                            // snprintf(cf52, sizeof(cf52), "%.1f%s ", harmonic_values[channel].ffifth, harmonic_values[channel].uF);
                            // strncat(tmp, cf52, sizeof(tmp) - strlen(tmp) - 1);

                            strncat(tmp, "| I7=", sizeof(tmp) - strlen(tmp) - 1);
                            char seventhHarmonicStr2[32];
                            snprintf(seventhHarmonicStr2, sizeof(seventhHarmonicStr2), "%.2f%s ", harmonic_values[channel].seventhHarmonic, measure_get_channel_report_unit(channel));
                            strncat(tmp, seventhHarmonicStr2, sizeof(tmp) - strlen(tmp) - 1);

                            // strncat(tmp, " - ", sizeof(tmp) - strlen(tmp) - 1);
                            // char cf72[32];
                            // snprintf(cf72, sizeof(cf72), "%.1f%s ", harmonic_values[channel].fsevent, harmonic_values[channel].uF);
                            // strncat(tmp, cf72, sizeof(tmp) - strlen(tmp) - 1);

                            strncat(tmp, "| I9=", sizeof(tmp) - strlen(tmp) - 1);
                            char ninethHarmonicStr2[32];
                            snprintf(ninethHarmonicStr2, sizeof(ninethHarmonicStr2), "%.2f%s ", harmonic_values[channel].ninthHarmonic, measure_get_channel_report_unit(channel));
                            strncat(tmp, ninethHarmonicStr2, sizeof(tmp) - strlen(tmp) - 1);

                            // strncat(tmp, " - ", sizeof(tmp) - strlen(tmp) - 1);
                            // char cf92[32];
                            // snprintf(cf92, sizeof(cf92), "%.1f%s ", harmonic_values[channel].fninth, harmonic_values[channel].uF);
                            // strncat(tmp, cf92, sizeof(tmp) - strlen(tmp) - 1);

                            strncat(tmp, "| THDI=", sizeof(tmp) - strlen(tmp) - 1);
                            char tdhi[32];
                            snprintf(tdhi, sizeof(tdhi), "%.1f%s ", harmonic_values[channel].thd, harmonic_values[channel].porcentaje);
                            strncat(tmp, tdhi, sizeof(tmp) - strlen(tmp) - 1);
                            strncat( tmp, "\r\n", sizeof( tmp ) - strlen( tmp ) - 1 );  // Salto de línea
                            break;
                            case AC_POWER:
                            case DC_POWER:
                                snprintf( tmp, sizeof( tmp ), "P=%.3f%s ", measure_get_channel_rms( channel ), measure_get_channel_report_unit( channel ) );
                                break;
                            case AC_REACTIVE_POWER:
                                snprintf( tmp, sizeof( tmp ), "Pvar=%.3f%s ", measure_get_channel_rms( channel ), measure_get_channel_report_unit( channel ) );
                                break;
                            default:
                                tmp[ 0 ] = '\0';
                        }
                        strncat( request, tmp, sizeof( request ) );
                    }
                }
                if( power_channel != -1 && reactive_power_channel != -1 ) {
                    snprintf( tmp, sizeof( tmp ), "Cos=%.3f ",cos_phi );
                    strncat( request, tmp, sizeof( request ) );
                }
                snprintf( tmp, sizeof( tmp ), "]" );
                strncat( request, tmp, sizeof( request ) );
            }
            tmp[ 0 ] = '\0';
            for( int i = 0 ; i < VIRTUAL_CHANNELS ; i++ ) {
                if( measure_get_channel_type( i ) == AC_VOLTAGE ) {
                    snprintf( tmp, sizeof( tmp ), " ; f = %.3fHz", measure_get_max_freq() );
                    break;
                }
            }
            strncat( request, tmp, sizeof( request ) );
            strncat( request, " )", sizeof( request ));

            client->printf( request );
        }
        /* Wlan SSID */
        else if ( !strcmp("hostname", cmd ) ) {
            wificlient_set_hostname( value );
        }
        /* WlanAP SSID */
        else if ( !strcmp("softap_ssid", cmd ) ) {
            wificlient_set_softap_ssid( value );
        }
        /* WlanAP Passwort */
        else if ( !strcmp("softap_password", cmd ) ) {
            if ( strcmp( "********", value ) )
            wificlient_set_softap_password( value );
        }
        /* Wlan SSID */
        else if ( !strcmp("ssid", cmd ) ) {
            wificlient_set_ssid( value );
        }
        /* Wlan Passwort */
        else if ( !strcmp("password", cmd ) ) {
            if ( strcmp( "********", value ) )
            wificlient_set_password( value );
        }
        /* Wlan Passwort */
        else if ( !strcmp("enable_softap", cmd ) ) {
            wificlient_set_enable_softap( atoi( value ) ? true : false );
        }
        /* MQTT Server */
        else if ( !strcmp("low_power", cmd ) ) {
            wificlient_set_low_power( atoi( value ) ? true : false );
        }
        /* MQTT Server */
        else if ( !strcmp("low_bandwidth", cmd ) ) {
            wificlient_set_low_bandwidth( atoi( value ) ? true : false );
        }
        /* MQTT Server */
        else if ( !strcmp("mqtt_server", cmd ) ) {
            mqtt_client_set_server( value );
        }
        /* MQTT Interval */
        else if ( !strcmp("mqtt_port", cmd ) ) {
            mqtt_client_set_port( atoi( value ) );
        }
        /* MQTT User */
        else if ( !strcmp("mqtt_username", cmd ) ) {
            mqtt_client_set_username( value );
        }
        /* MQTT Pass */
        else if ( !strcmp("mqtt_password", cmd ) ) {
            if ( strcmp( "********", value ) )
                mqtt_client_set_password( value );
        }
        /* MQTT Topic */
        else if ( !strcmp("mqtt_topic", cmd ) ) {
            mqtt_client_set_topic( value );
        }
        /* MQTT Interval */
        else if ( !strcmp("mqtt_interval", cmd ) ) {
            mqtt_client_set_interval( atoi( value ) );
        }
        /* MQTT Interval */
        else if ( !strcmp("mqtt_realtimestats", cmd ) ) {
            mqtt_client_set_realtimestats( atoi( value ) ? true : false );
        }
        /* store AC-main voltage frequency */
        else if ( !strcmp("samplerate_corr", cmd ) ) {
            measure_set_samplerate_corr( atoi( value ) );
        }
        /* store smaple-rate */
        else if ( !strcmp("network_frequency", cmd ) ) {
            measure_set_network_frequency( atof( value ) );
        }
        /* sample-rate +1Hz */
        else if ( !strcmp("FQ+", cmd ) ) {
            measure_set_samplerate_corr( measure_get_samplerate_corr() + 1 );
        }
        /* sample-rate -1Hz */
        else if ( !strcmp("FQ-", cmd ) ) {
            measure_set_samplerate_corr( measure_get_samplerate_corr() - 1 );
        }
        else if ( !strcmp("PS+", cmd ) )
            measure_set_channel_phaseshift( selectedchannel, measure_get_channel_phaseshift( selectedchannel ) + 1 );
        else if ( !strcmp("PS-", cmd ) )
            measure_set_channel_phaseshift( selectedchannel, measure_get_channel_phaseshift( selectedchannel ) - 1 );
        /**
         * channel group
         */
        else if ( !strcmp("channel", cmd ) )
            selectedchannel = atoi( value );
        else if ( !strcmp("channel_type", cmd ) )
            measure_set_channel_type( selectedchannel , (channel_type_t)atoi( value ) );
        else if ( !strcmp("channel_report_exp", cmd ) )
            measure_set_channel_report_exp( selectedchannel , atoi( value ) );
        else if ( !strcmp("channel_phaseshift", cmd ) )
            measure_set_channel_phaseshift( selectedchannel , atoi( value ) );
        else if ( !strcmp("channel_true_rms", cmd ) )
            measure_set_channel_true_rms( selectedchannel, atoi( value ) ? true : false );
        else if ( !strcmp("channel_opcodeseq_str", cmd ) )
            measure_set_channel_opcodeseq_str( selectedchannel ,value );
        else if ( !strcmp("channel_offset", cmd ) )
            measure_set_channel_offset( selectedchannel , atof( value ) );
        else if ( !strcmp("channel_ratio", cmd ) )
            measure_set_channel_ratio( selectedchannel , atof( value ) );
        else if ( !strcmp("channel_name", cmd ) )
            measure_set_channel_name( selectedchannel , value );
        else if ( !strcmp("channel_group_id", cmd ) )
            measure_set_channel_group_id( selectedchannel , atoi( value ) );
        /**
         * groups
         */
        else if ( !strcmp("group_name", cmd ) )
            measure_set_group_name( selectedchannel,  value );
        else if ( !strcmp("group_active", cmd ) )
            measure_set_group_active( selectedchannel, atoi( value ) ? true : false );
  

        free( cmd );
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
