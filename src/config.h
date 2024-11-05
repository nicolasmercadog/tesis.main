
#ifndef _CONFIG_H
    #define _CONFIG_H

    #if CONFIG_FREERTOS_UNICORE  //corre freertos sólo en el primer núcleo
    
        #define _MEASURE_TASKCORE    1
        #define _MQTT_TASKCORE       1
        #define _WEBSOCK_TASKCORE    1
        #define _OTA_TASKCORE        1
        #define _WEBSERVER_TASKCORE  1
        #define _NTP_TASKCORE        1  

    #else
    
        #define _MEASURE_TASKCORE    0
        #define _MQTT_TASKCORE       1
        #define _WEBSOCK_TASKCORE    1
        #define _OTA_TASKCORE        1
        #define _WEBSERVER_TASKCORE  1
        #define _NTP_TASKCORE        1  
    
    #endif // CONFIG_FREERTOS_UNICORE
    /*
     * firmewareversion string
     */
    #define __FIRMWARE__            "2022110101" //fecha del firmware

#endif // _CONFIG_H
