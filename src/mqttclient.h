
#ifndef _MQTTCLIENT_H
    #define _MQTTCLIENT_H

    /**
     * Inicio de tarea de cliente mqtt
     */
    void sendDataToMongoDB(const char* topic, const char* payload);

    void mqtt_client_StartTask( void );
    /**
     * @brief Publica un msj (payload) mqtt en cierto tema (topic)
     * 
     * @param topic     topic
     * @param payload   msg to send
     */
    void mqtt_client_publish( char * topic, char * payload );
    /**
     * @brief Desabilita todas las conexiones con el cliente mqtt
     */
    void mqtt_client_disable( void );
    /**
     * @brief Activa todas las conexiones con el cliente mqtt
     */
    void mqtt_client_enable( void );
    /**
     * @brief Toma la dirección del servidor mqtt
     *      
     * @return  Dirección del servidor como array de caracteres 
     */
    const char *mqtt_client_get_server( void );
    /**
     * @brief Setea el servidor mqtt
     * 
     * @param   server  La dirección del servidor como array de caracteres 
     */
    void mqtt_client_set_server( const char *server );
    /**
     * @brief Obtiene el nombre de usuario mqtt
     * 
     * @return  Nombre de usuario como cadena de caracteres 
     */
    const char *mqtt_client_get_username( void );
    /**
     * @brief Setea el usuario mqtt
     *  
     * @param   username   Nombre de usuario como cadena de caracteres
     */
    void mqtt_client_set_username( const char *username );
    /**
     * @brief Obtiene la contraseña de usuario mqtt
     * 
     * @return  Contraseña como cadena de caracteres
     */
    const char *mqtt_client_get_password( void );
    /**
     * @brief Setea la contraseña de usuario mqtt
     * 
     * @param   password    Contraseña como cadena de caracteres
     */
    void mqtt_client_set_password( const char *password );
    /**
     * @brief get mqtt topic prefix
     * 
     * @return  mqtt topix prefix as char array
     */
    const char *mqtt_client_get_topic( void );
    /**
     * @brief set mqtt topix prefix
     * 
     * @param topic topix as char array
     */
    void mqtt_client_set_topic( const char *topic );
    /**
     * @brief get mqtt server port
     * 
     * @return  serverport
     */
    int mqtt_client_get_port( void );
    /**
     * @brief set server port
     * 
     * @param   port    mqtt server port
     */
    void mqtt_client_set_port( int port );
    /**
     * @brief get mqtt msg interval
     *
     * @return  mqtt msg interval in sec
     */
    int mqtt_client_get_interval( void );
    /**
     * @brief set mqtt msg interval
     * 
     * @param   interval    mqtt msg interval in sec
     */
    void mqtt_client_set_interval( int interval );
    bool mqtt_client_get_realtimestats( void );
    void mqtt_client_set_realtimestats( bool realtimestats );
    /**
     * @brief store mqtt settings as .json
     * 
     * @note all settings has a direct effect but was not stored, only here the a new json is written
     */
    void mqtt_save_settings( void );
#endif // _MQTTCLIENT_H
