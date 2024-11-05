
#include <WiFi.h>
#include <AsyncMqttClient.h>
#include <ArduinoJson.h>

#include "config/mqtt_config.h"
#include "config.h"
#include "measure.h"
#include "mqttclient.h"
#include "wificlient.h"
#include <HTTPClient.h>


const char* serverName = "https://sa-east-1.aws.data.mongodb-api.com/app/conecthttp-fhbjiiu/endpoint/test"; // Cambia esto por tu URL de endpoint


mqtt_config_t mqtt_config; //objeto mqtt_config
TaskHandle_t _ASYNCMQTT_Task; //Manejador de tareas _ASYNCMQTT_Task
AsyncMqttClient mqttClient;     //objeto mqttClient
bool disable_MQTT = false;  //booleano para deshabilitar mqtt en falso
char reset_state[64] = "";  //char para resetaer el estado comienza vacío

#define MQTT_CMND                  "cmnd/%s/powermeter"            /** @brief Direcciones del cmd del medidor de energía */
#define MQTT_CONFIG                "stat/%s/config"                /** @brief Direcciones del cmd del medidor de energía */
#define MQTT_DATA                  "stat/%s/data"                  /** @brief Direcciones del cmd del medidor de energía */
#define MQTT_STAT_POWER            "stat/%s/power"                 /** @brief Direcciones del cmd del medidor de energía */
#define MQTT_STAT_REALTIMEPOWER    "stat/%s/realtimepower"         /** @brief Direcciones del cmd del medidor de energía */
#define MQTT_STAT_FFT               "stat/%s/fft"         /** @brief Direcciones del cmd del medidor de energía */

char powermeter_cmnd_topic[128] = "";                                   /** @brief nombre del buffer del comando del tópico del powermeter */
char powermeter_config_topic[128] = "";                                 /** @brief nombre del buffer de la config del topic del powermeter */
char powermeter_data_topic[128] = "";                                   /** @brief nombre del buffer de los datos del topic del powermeter */
char powermeter_stat_power_topic[128] = "";                             /** @brief nobmre del buffer de las estadísticas del topic del powermeter*/
char powermeter_stat_realtimepower_topic[128] = "";                     /** @brief nombre del buffer de las estadísticas en tiempo real del topic del powermeter */
char powermeter_stat_fft_topic[128] = "";                               /** @brief nombre del buffer de las estadísticas en tiempo real del topic del powermeter */

static float measure_rms[ VIRTUAL_CHANNELS ];                           /** @brief Buffer de medición de potencia */
static float measure_frequency;                                         /** @brief Actual frecuencia medida */

void mqtt_client_Task( void * pvParameters );       //tarea de inicio de cliente mqtt
void mqtt_client_send_realtimepower( void );        //envío de datos en tiempo real
void mqtt_client_send_fft( void );                 //envío de datos fft en tiempo real
void mqtt_client_send_power( void );                //envío de potencia

/**
 * @brief mqqt on connect call back routine
 * 
 * @param sessionPresent 
 */
void mqtt_client_on_connect( bool sessionPresent ) { //rutina de conexión del cliente al broker mqtt
    log_i( "MQTT-Client: connected to %s\r\n", mqtt_config.server );
    /**
     * setup all mqtt topic
     */
    snprintf( powermeter_cmnd_topic, sizeof( powermeter_cmnd_topic ), MQTT_CMND, mqtt_config.topic );
    snprintf( powermeter_config_topic, sizeof( powermeter_config_topic ), MQTT_CONFIG, mqtt_config.topic );
    snprintf( powermeter_data_topic, sizeof( powermeter_data_topic ), MQTT_DATA, mqtt_config.topic );
    snprintf( powermeter_stat_power_topic, sizeof( powermeter_stat_power_topic ), MQTT_STAT_POWER, mqtt_config.topic );
    snprintf( powermeter_stat_realtimepower_topic, sizeof( powermeter_stat_realtimepower_topic ), MQTT_STAT_REALTIMEPOWER, mqtt_config.topic );
    snprintf( powermeter_stat_fft_topic, sizeof( powermeter_stat_fft_topic ), MQTT_STAT_FFT, mqtt_config.topic );

    /**
     * subscripe cmnd topic for remote cmnd
     */
    mqttClient.subscribe( powermeter_cmnd_topic, 0 ); //suscripción al topic con QoS en 0, es decir que no importa la perdida de datos
}


void sendDataToMongoDB(const char* topic, const char* payload) {
    if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi conectado, comenzando el envío a MongoDB...");

    WiFiClientSecure client;
    client.setInsecure();  // Solo para pruebas, asegúrate de considerar seguridad en producción

    HTTPClient http;
    http.begin(client, serverName);  // Asegúrate de que serverName esté bien definido y sea correcto
    http.addHeader("Content-Type", "application/json");

    // Formatear el JSON adecuadamente
    String jsonPayload = "{\"topic\": \"" + String(topic) + "\", \"payload\": " + String(payload) + "}";
    Serial.printf("Payload a enviar: %s\n", jsonPayload.c_str());

    int httpResponseCode = http.POST(jsonPayload);
    Serial.printf("Bandera 1");
if (httpResponseCode > 0) {
    Serial.printf("Datos enviados a MongoDB, respuesta HTTP: %d\n", httpResponseCode);
    // Eliminar la línea `getString()` si no hay contenido esperado
    // String response = http.getString();
    // Serial.println("Respuesta del servidor: " + response);
} else {
    Serial.printf("Error en la solicitud HTTP: %d\n", httpResponseCode);
}

    Serial.printf("Bandera 2");

    http.end();
} else {
    Serial.println("Error: WiFi no está conectado, no se pueden enviar datos a MongoDB");
}
}

/**
 * @brief mqtt on disconnect call back routine
 * 
 * @param reason 
 */
void mqtt_client_on_disconnect(AsyncMqttClientDisconnectReason reason) {
    log_i( "MQTT-Client: Disconnected from MQTT\r\n" );
    /**
     * check connection state and reconnect if possible/allowed
     */
    if ( WiFi.isConnected() ) {
        if ( strlen( mqtt_config.server ) >= 1 && disable_MQTT == false ) {
            mqtt_client_enable();
        }
    }
}

/**
 * @brief mqtt on message call back routine
 * 
 * @param topic             mqtt topic
 * @param payload           payload buffer, not zero terminated
 * @param properties        
 * @param len               size of the payload buffer
 * @param index             
 * @param total
 */
void mqtt_client_on_message(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
    /**
     * copy no zero terminate payload buffer into a terminated buffer
     */    
    char *msg = NULL;
    if( !(msg = (char *)calloc( 1, len + 1 ) ) ) {
        log_e("error while allocate payload buffer");
        while( true );
    }
    memcpy( (void*)msg, payload, len );
    log_d("MQTT-Client: Publish received, topic: [%s] , payload: [%s]", topic, msg );
    /**
     * check if the msg come from cmnd topc
     */
    if( !strcmp( topic, powermeter_cmnd_topic ) ) {
        /**
         * serialize json document
         */
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson( doc, msg );
        if( !error ) {

        }
        else {
            /**
             * if failed
             */
            log_e("json deserialiation Error");
        }
    }
    /**
     * free msg buffer
     */
    free( msg );
}

/**
 * @brief publish a mqqt msg to a given topic
 * 
 * @param topic     topic
 * @param payload   msg to send
 */
void mqtt_client_publish( char * topic, char * payload ) {
    mqttClient.publish( topic , 1, false, payload, strlen( payload ), false, 0 );
}

/**
 * @brief start the mqqt client background task
 */
void mqtt_client_StartTask( void ) {
    xTaskCreatePinnedToCore(
                                mqtt_client_Task,   /* Function to implement the task */
                                "mqttclient Task",  /* Name of the task */
                                10000,              /* Stack size in words */
                                NULL,               /* Task input parameter */
                                1,                  /* Priority of the task */
                                &_ASYNCMQTT_Task,   /* Task handle. */
                                _MQTT_TASKCORE );   /* Core where the task should run */
}

/**
 * @brief mqtt client task function
 * 
 * @param pvParameters      pvParameters or NULL
 */
void mqtt_client_Task( void * pvParameters ) {
    /*
     * set the timerevent for sending MQTT and reconnect
     */
    static uint64_t NextMeasureMillis = millis();
    static uint64_t NextMillis = millis() + 15000;
    static int data_counter = 0;
    /**
     * clear measure buffers
     */
    memset( measure_rms, 0, sizeof( measure_rms ) );
    measure_frequency = 0.0;
    /**
     * get reset state and set reset state string
     */
    esp_reset_reason_t why = esp_reset_reason();
    switch ( why ) {
        case (ESP_RST_POWERON):
            snprintf( reset_state, sizeof( reset_state ), "ESP_RST_POWERON" );
            break;
        case (ESP_RST_UNKNOWN):
            snprintf( reset_state, sizeof( reset_state ), "ESP_RST_UNKNOWN" );
            break;
        case (ESP_RST_EXT):
            snprintf( reset_state, sizeof( reset_state ), "ESP_RST_EXT" );
            break;
        case (ESP_RST_SW):
            snprintf( reset_state, sizeof( reset_state ), "ESP_RST_SW" );
            break;
        case (ESP_RST_PANIC):
            snprintf( reset_state, sizeof( reset_state ), "ESP_RST_PANIC" );
            break;
        case (ESP_RST_INT_WDT):
            snprintf( reset_state, sizeof( reset_state ), "ESP_RST_INT_WDT" );
            break;
        case (ESP_RST_TASK_WDT):
            snprintf( reset_state, sizeof( reset_state ), "ESP_RST_TASK_WDT" );
            break;
        case (ESP_RST_WDT):
            snprintf( reset_state, sizeof( reset_state ), "ESP_RST_WDT" );
            break;
        case (ESP_RST_DEEPSLEEP):
            snprintf( reset_state, sizeof( reset_state ), "ESP_RST_DEEPSLEEP" );
            break;
        case (ESP_RST_BROWNOUT):
            snprintf( reset_state, sizeof( reset_state ), "ESP_RST_BROWNOUT" );
            break;
        case (ESP_RST_SDIO):
            snprintf( reset_state, sizeof( reset_state ), "ESP_RST_SDIO" );
            break;
        default:
            break;
    }
    mqtt_config.load();
    /**
     * set call back functions
     */
    mqttClient.onConnect( mqtt_client_on_connect );
    mqttClient.onDisconnect( mqtt_client_on_disconnect );
    mqttClient.onMessage( mqtt_client_on_message );    
    log_i( "Start MQTT-Client on Core: %d", xPortGetCoreID() );
    /**
     * delay task
     */
    vTaskDelay( 1000 );
    /**
     * wait for wifi
     */
    while( !WiFi.isConnected() ){};
    mqtt_client_enable();

    while( true ) {
        vTaskDelay( 250 );

        if ( WiFi.isConnected() ) {
            if ( !mqttClient.connected() ) {
                if ( strlen( mqtt_config.server ) >= 1 && disable_MQTT == false ) {
                    mqttClient.connect();
                }
            }
            else {
                /**
                 *  send every second an json msg to MQTT
                 */
                if ( NextMeasureMillis < millis() ) {
                    NextMeasureMillis += 1000l;
                    /*
                     * add measure values to each measure buffer every second
                     */
                    for ( int channel = 0 ; channel < VIRTUAL_CHANNELS ; channel++ ) {
                        measure_rms[ channel ] += measure_get_channel_rms( channel );
                        measure_frequency += measure_get_max_freq();
                    }
                    data_counter++;
                    if( mqtt_config.realtimestats )
                        mqtt_client_send_realtimepower();
                        mqtt_client_send_fft();
                }
                /**
                 *  send every MQTTInterval seconds a json msg to MQTT
                 */
                if ( NextMillis < millis() ) {
                    NextMillis += mqtt_config.interval* 1000l;
                    /**
                     * calculate measure buffers
                     */
                    for ( int channel = 0 ; channel < VIRTUAL_CHANNELS ; channel++ )
                        measure_rms[ channel ] /= data_counter;
                    measure_frequency /= data_counter;
                    /**
                     * send data
                     */
                    mqtt_client_send_power();
                    /**
                     * clear measure buffers
                     */
                    for ( int channel = 0 ; channel < VIRTUAL_CHANNELS ; channel++ )
                        measure_rms[ channel ] = 0;
                    measure_frequency = 0;
                    data_counter = 0;
                }
            }
        }
    }
}

/**
 * @brief send a realtime power json msg over mqtt
 */
void mqtt_client_send_realtimepower( void ) {
    time_t now;
    struct tm info;
    char time_str[64] = "";
    String ip = WiFi.localIP().toString();
    String json;
    /**
     * generate json
     */
    time( &now );
    localtime_r( &now, &info );
    strftime( time_str, sizeof( time_str ), "%Y-%m-%d %H:%M.%S", &info );
    StaticJsonDocument<4096> doc;
    /**
     * check if measurement are valid
     */
    if( !measure_get_measurement_valid() )
        return;
    /**
     * fill the json with generic data
     */
    doc["id"] = wificlient_get_hostname();
    doc["ip"] = ip.c_str();
    doc["time"] = time_str;
    doc["uptime"] = millis() / 1000;
    doc["reset_state"] = reset_state;
    doc["measurement_valid"] = measure_get_measurement_valid();
    doc["interval"] = 1;
    doc["frequency"] = measure_get_max_freq();
   // log_i("frequency (%f)", measure_get_max_freq());
    
    /**
     * write out measurment data
     */

    for ( int group_id = 0 ; group_id < 6 ; group_id++ ) {
        int power_channel = -1;
        int reactive_power_channel = -1;
        float cos_phi = 1.0f;
        /**
         * check if group is active
         */
        if( !measure_get_group_active( group_id ) )
            continue;
        /**
         * check if this group has members
         */
        if( !measure_get_channel_group_id_entrys( group_id ) )
            continue;

        for( int channel = 0 ; channel < VIRTUAL_CHANNELS; channel++ ) {
            if( measure_get_channel_group_id( channel ) == group_id ) {
                /**
                 * continue if channel not used
                 */
                if( measure_get_channel_type( channel ) == NO_CHANNEL_TYPE )
                    continue;

                if( measure_get_channel_type( channel ) == AC_POWER )
                    power_channel = channel; 
                if( measure_get_channel_type( channel ) == AC_REACTIVE_POWER )
                    reactive_power_channel = channel;

                if( power_channel != -1 && reactive_power_channel != -1 ) {
                    float active_power = measure_get_channel_rms( power_channel ) + measure_get_channel_rms( reactive_power_channel );
                    cos_phi = active_power / measure_get_channel_rms( power_channel );
                    //log_i("Cos phi (%f)", cos_phi);
                }
                /**
                 * check if this channel has the right group ID and
                 * push out group info
                 */
                if( measure_get_channel_group_id( channel ) == group_id ) {

                    /**
                     * build quantity and type string
                     */
                    char quantity[ 32 ] = "";
                    char type[ 32 ] = "DC";
                    switch( measure_get_channel_type( channel ) ) {
                        case AC_CURRENT:
                            snprintf( type, sizeof( type ), "AC" );
                        case DC_CURRENT:
                            snprintf( quantity, sizeof( quantity ), "current" );
                            break;
                        case AC_VOLTAGE:
                            snprintf( type, sizeof( type ), "AC" );
                        case DC_VOLTAGE:
                            snprintf( quantity, sizeof( quantity ), "voltage" );
                            break;
                        case AC_POWER:
                            snprintf( type, sizeof( type ), "AC" );
                        case DC_POWER:
                            snprintf( quantity, sizeof( quantity ), "power" );
                            break;
                        case AC_REACTIVE_POWER:
                            snprintf( type, sizeof( type ), "AC" );
                            snprintf( quantity, sizeof( quantity ), "reactive power" );
                            break;
                        default:
                            snprintf( type, sizeof( type ), "n/a" );
                            snprintf( quantity, sizeof( quantity ), "n/a" );
                            break;
                    }                
                    /**
                     * push out channel info
                     */
                    doc["group"][ measure_get_group_name( group_id ) ][ quantity ][ "value" ] = measure_get_channel_rms( channel );
                    doc["group"][ measure_get_group_name( group_id ) ][ quantity ][ "unit" ] = measure_get_channel_report_unit( channel );
                    doc["group"][ measure_get_group_name( group_id ) ][ quantity ][ "type" ] = type;
                    doc["group"][ measure_get_group_name( group_id ) ][ quantity ][ "name" ] = measure_get_channel_name( channel );
                }
            }
        }
        if( power_channel != -1 && reactive_power_channel != -1 )
            doc["group"][ measure_get_group_name( group_id ) ][ "cos_phi" ] = cos_phi;
    }
    /**
     * write out io port states
     */
    /**
     * serialize json and send via MQTT
     */
    serializeJson( doc, json );
    mqtt_client_publish( powermeter_stat_realtimepower_topic , (char*)json.c_str() );
        //sendDataToMongoDB(powermeter_stat_realtimepower_topic, (char*)json.c_str());


    return;
}

/**
 * @brief send a power json msg over mqtt
 */
void mqtt_client_send_power( void ) {
    time_t now;
    struct tm info;
    char time_str[64] = "";
    String ip = WiFi.localIP().toString();
    String json;
    /**
     * generate json
     */
    time( &now );
    localtime_r( &now, &info );
    strftime( time_str, sizeof( time_str ), "%Y-%m-%d %H:%M.%S", &info );
    StaticJsonDocument<4096> doc;
    /**
     * check if measurement are valid
     */
    if( !measure_get_measurement_valid() )
        return;
    /**
     * fill the json with data
     */
    doc["id"] = wificlient_get_hostname();
    doc["ip"] = ip.c_str();
    doc["time"] = time_str;
    doc["uptime"] = millis() / 1000;
    doc["reset_state"] = reset_state;
    doc["measurement_valid"] = measure_get_measurement_valid();
    doc["interval"] = mqtt_config.interval;
    for( int i = 0 ; i < VIRTUAL_CHANNELS ; i++ ) {
        if( measure_get_channel_type( i ) == AC_VOLTAGE ) {
            doc["frequency"] = measure_get_max_freq();
            break;
        }
    }
    /**
     * write out measurment data
     */
    for ( int group_id = 0 ; group_id < 6 ; group_id++ ) {
        int power_channel = -1;
        int reactive_power_channel = -1;
        float cos_phi = 1.0f;
        /**
         * check if group is active
         */
        if( !measure_get_group_active( group_id ) )
            continue;
        /**
         * check if this group has members
         */
        if( !measure_get_channel_group_id_entrys( group_id ) )
            continue;
        /**
         * crawl all channels
         */
        for( int channel = 0 ; channel < VIRTUAL_CHANNELS; channel++ ) {
            if( measure_get_channel_group_id( channel ) == group_id ) {
                /**
                 * continue if channel not used
                 */
                if( measure_get_channel_type( channel ) == NO_CHANNEL_TYPE )
                    continue;

                if( measure_get_channel_type( channel ) == AC_POWER )
                    power_channel = channel;
                if( measure_get_channel_type( channel ) == AC_REACTIVE_POWER )
                    reactive_power_channel = channel;

                if( power_channel != -1 && reactive_power_channel != -1 ) {
                    float active_power = measure_rms[ power_channel ] + measure_rms[ reactive_power_channel ];
                    cos_phi = active_power / measure_rms[ power_channel ];
                }
                /**
                 * check if this channel has the right group ID and
                 * push out group info
                 */
                if( measure_get_channel_group_id( channel ) == group_id ) {
                    /**
                     * build quantity and type string
                     */
                    char quantity[ 32 ] = "";
                    char type[ 32 ] = "DC";
                    switch( measure_get_channel_type( channel ) ) {
                        case AC_CURRENT:
                            snprintf( type, sizeof( type ), "AC" );
                        case DC_CURRENT:
                            snprintf( quantity, sizeof( quantity ), "current" );
                            break;
                        case AC_VOLTAGE:
                            snprintf( type, sizeof( type ), "AC" );
                        case DC_VOLTAGE:
                            snprintf( quantity, sizeof( quantity ), "voltage" );
                            break;
                        case AC_POWER:
                            snprintf( type, sizeof( type ), "AC" );
                        case DC_POWER:
                            snprintf( quantity, sizeof( quantity ), "power" );
                            break;
                        case AC_REACTIVE_POWER:
                            snprintf( type, sizeof( type ), "AC" );
                            snprintf( quantity, sizeof( quantity ), "reactive power" );
                            break;
                        default:
                            snprintf( type, sizeof( type ), "n/a" );
                            snprintf( quantity, sizeof( quantity ), "n/a" );
                            break;
                    }                
                    /**
                     * push out channel info
                     */
                    doc["group"][ measure_get_group_name( group_id ) ][ quantity ][ "value" ] = measure_rms[ channel ];
                    doc["group"][ measure_get_group_name( group_id ) ][ quantity ][ "unit" ] = measure_get_channel_report_unit( channel );
                    doc["group"][ measure_get_group_name( group_id ) ][ quantity ][ "type" ] = type;
                    doc["group"][ measure_get_group_name( group_id ) ][ quantity ][ "name" ] = measure_get_channel_name( channel );
                }
            }
        }
        if( power_channel != -1 && reactive_power_channel != -1 )
            doc["group"][ measure_get_group_name( group_id ) ][ "cos_phi" ] = cos_phi;
    }
    /**
     * write out io port states
    
    }    
     * serialize json and send via MQTT
     */
    serializeJson( doc, json );
    mqtt_client_publish( powermeter_stat_power_topic , (char*)json.c_str() );

    return;
}

/**
 * @brief disable all mqtt connections
 */
void mqtt_client_disable( void ) {
    disable_MQTT = true;
    mqttClient.disconnect();
}

/**
 * @brief enable all mqtt connections and reload all connection settings
 */
void mqtt_client_enable( void ) {
    disable_MQTT = false;
    /**
     * reload all connection settings
     */
    mqttClient.setServer( mqtt_config.server , mqtt_config.port );
    mqttClient.setClientId( wificlient_get_hostname() );
    // No establecer credenciales ya que HiveMQ public broker no las requiere
    /**
     * start connection
     */
    mqttClient.connect();
}

const char *mqtt_client_get_server( void ) {
    return( (const char*)mqtt_config.server );
}

void mqtt_client_set_server( const char *server ) {
    strlcpy( mqtt_config.server, server, sizeof( mqtt_config.server) );
}

const char *mqtt_client_get_username( void ) {
    return( (const char*)mqtt_config.username );
}

void mqtt_client_set_username( const char *username ) {
    strlcpy( mqtt_config.username, username, sizeof( mqtt_config.username) );
}

const char *mqtt_client_get_password( void ) {
    return( (const char*)mqtt_config.password );
}

void mqtt_client_set_password( const char *password ) {
    strlcpy( mqtt_config.password, password, sizeof( mqtt_config.password) );
}

const char *mqtt_client_get_topic( void ) {
    return( (const char*)mqtt_config.topic );
}

void mqtt_client_set_topic( const char *topic ) {
    strlcpy( mqtt_config.topic, topic, sizeof( mqtt_config.topic) );
}

int mqtt_client_get_port( void ) {
    return( mqtt_config.port );
}

void mqtt_client_set_port( int port ) {
    mqtt_config.port = port;
}

int mqtt_client_get_interval( void ) {
    return( mqtt_config.interval );
}

void mqtt_client_set_interval( int interval ) {
    mqtt_config.interval = interval;
}

bool mqtt_client_get_realtimestats( void ) {
    return( mqtt_config.realtimestats );
}

void mqtt_client_set_realtimestats( bool realtimestats ) {
    mqtt_config.realtimestats = realtimestats;
}

void mqtt_save_settings( void ) {
    mqtt_config.save();

    mqtt_client_disable();
    mqtt_client_enable();
}

void mqtt_client_send_fft(void) {
    time_t now;
    struct tm info;
    char time_str[64] = "";
    String ip = WiFi.localIP().toString();
    String json;

    // Obtener el tiempo actual
    time(&now);
    localtime_r(&now, &info);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M.%S", &info);

    // Crear documento JSON
    StaticJsonDocument<4096> doc;

    // Validar si la medición es válida
    if (!measure_get_measurement_valid()) return;

    // Rellenar datos genéricos
    doc["id"] = wificlient_get_hostname();
    doc["ip"] = ip.c_str();
    doc["time"] = time_str;
    doc["uptime"] = millis() / 1000;
    doc["reset_state"] = reset_state;
    doc["measurement_valid"] = measure_get_measurement_valid();
    doc["interval"] = 1;
    doc["frequency"] = measure_get_max_freq();

    // Obtener y añadir datos FFT y armónicos al JSON
    //measure_get_fft_for_mqtt(doc);

    // Serializar y enviar por MQTT
    //serializeJson(doc, json);
    //mqtt_client_publish(powermeter_stat_fft_topic, (char*)json.c_str());
}