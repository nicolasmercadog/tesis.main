#ifndef _MQTTCLIENT_H
    #define _MQTTCLIENT_H

    /**
     * @brief Enviar datos al servidor MongoDB
     * 
     * @param topic     Identificador del tipo de datos
     * @param payload   Datos en formato JSON
     */
    void sendDataToMongoDB(const char* topic, const char* payload);

    /**
     * @brief Envía datos de potencia en tiempo real a MongoDB
     */
    void sendPowerDataToMongoDB(void);

    /**
     * @brief Inicia la tarea en segundo plano para enviar datos a MongoDB
     */
    void startMongoDBTask(void);

    void sendDataToMongoDBTask(void* pvParameters);

#endif // _MQTTCLIENT_H
