#include <Arduino.h>
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
 * @brief arduino setup function
 */

void setup( void ) {
    setCpuFrequencyMhz( 240 );
    Serial.begin(115200);
    /*
     * hardware stuff and file system
     */
    log_i("Start Main Task on Core: %d", xPortGetCoreID() );
    if ( !SPIFFS.begin() ) {
        log_i("format SPIFFS ..." );
        SPIFFS.format();
    }
    //ioport_init();
    wificlient_init();
    /*
     * Setup Tasks
     */
    measure_StartTask();
    mqtt_client_StartTask();
    asyncwebserver_StartTask();
    ntp_StartTask();
        sendDataToMongoDB("test_topic", "Test message desde ESP32");

}

/**
 * @brief arduino main loop
 */
void loop() {
        measure_get_fft();



/*for (int i = 0; i < 3; i++) {
    log_i("A%d: (%1.2f) V%d: (%1.2f), P%d: (%1.2f)", 
          i+1, measure_get_channel_rms(i*3), 
          i+1, measure_get_channel_rms(i*3+1), 
          i+1, measure_get_channel_rms(i*3+2));
    vTaskDelay( 200 );
//}*/

    //display_loop();
    //ioport_loop();
}
