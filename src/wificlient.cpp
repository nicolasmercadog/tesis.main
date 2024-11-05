/**
 * @file wificlient.cpp
 * @author Dirk Broßwick (dirk.brosswick@googlemail.com)
 * @brief 
 * @version 1.0
 * @date 2022-10-03
 * 
 * @copyright Copyright (c) 2022
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */ 
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include "config/wifi_config.h"
#include "wificlient.h"

wificlient_config_t wificlient_config;
TaskHandle_t _wificlient_Task;

static void wificlient_Task( void * pvParameters );

void wificlient_init( void ) {
    static bool wifi_client_init = false;

    if( wifi_client_init ) {
        log_e("wifi client is running");
        return;
    }
    /**
     * load wifi config from json 
     */
    wificlient_config.load();
    /**
     * set wifi sta mode
     */
    WiFi.mode( WIFI_STA );
    /**set low power wifi*/
    if (!WiFi.mode(WIFI_STA)) {
        log_e("Failed to set WiFi mode to STA");
        return;
    }

    /**
     * set low power and low bandwidth
     */
    if(  wificlient_config.low_bandwidth )
        esp_wifi_set_bandwidth( ESP_IF_WIFI_STA, WIFI_BW_HT20 );
    else
        esp_wifi_set_bandwidth( ESP_IF_WIFI_STA, WIFI_BW_HT40 );

    if( wificlient_config.low_power )
        esp_wifi_set_max_tx_power( 44 );
    else
        esp_wifi_set_max_tx_power( 80 );
    /**
     * set hostname
     */
    WiFi.setHostname( wificlient_config.hostname );
    /**
     * start wifi client task
     */
    xTaskCreatePinnedToCore( wificlient_Task, "wificlient Task", 5000, NULL, 1, &_wificlient_Task, 1 );
    wifi_client_init = true;
}
/**
 * @brief check WiFi connect and reconnect if network available or start a OTA softAP
 */
static void wificlient_Task( void * pvParameters ) {
    static bool wifi_client_task_init = false;
    static bool APMODE = false;

    if( wifi_client_task_init ) {
        log_e("wifi client task is running");
        return;
    }
    

    wifi_client_task_init = true;
    
    while( true ) {
        vTaskDelay( 1000 );
        /*
         * Check if WiFi connected
         */
        if ( WiFi.status() != WL_CONNECTED ) {
            /**
             * load timeout from config
             */
            int wlan_timeout = wificlient_config.timeout;
            log_i("WiFi connection lost, restart ... ");
            /**
             * set ssid and password from config
             */
            WiFi.begin( wificlient_config.ssid , wificlient_config.password );
            /**
             * check connection in a loop
             */
            while ( WiFi.status() != WL_CONNECTED ){
                delay(1000);
                /**
                 * when we run into timeout
                 */
                if ( wlan_timeout <= 0 ) {
                    /**
                     * enable softAP when enabled in config
                     */
                    if( !APMODE && wificlient_config.enable_softap ) {
                        WiFi.softAP( wificlient_config.softap_ssid, wificlient_config.softap_password );
                        log_i("failed and starting Wifi-AP with SSID \"%s\"", wificlient_config.softap_ssid );
                        log_i("AP IP address: %s", WiFi.softAPIP().toString().c_str() );
                        APMODE = true;
                    }
                    /**
                     * exit the loop
                     */
                    break;
                }
                /**
                 * count timeout
                 */
                wlan_timeout--;
            }
            if ( WiFi.status() == WL_CONNECTED ) {
                log_i("connected, IP address: %s", WiFi.localIP().toString().c_str() );
            }
        }
    }
}

const char *wificlient_get_hostname( void ) {
    return( (const char*)wificlient_config.hostname );
}

void wificlient_set_hostname(const char* hostname) {
    if (hostname == nullptr) {
        log_e("Hostname cannot be null");
        return;
    }

    if (strlen(hostname) >= sizeof(wificlient_config.hostname)) {
        log_e("Hostname is too long");
        return;
    }

    strlcpy(wificlient_config.hostname, hostname, sizeof(wificlient_config.hostname));
}

const char *wificlient_get_ssid( void ) {
    return( (const char*)wificlient_config.ssid );
}

void wificlient_set_ssid( const char * ssid ) {
    strlcpy( wificlient_config.ssid, ssid, sizeof( wificlient_config.ssid ) );
}

const char *wificlient_get_password( void ) {
    return( (const char*)wificlient_config.password );
}

void wificlient_set_password( const char * password ) {
    strlcpy( wificlient_config.password, password, sizeof( wificlient_config.password ) );
}

const char *wificlient_get_softap_ssid( void ) {
    return( (const char*)wificlient_config.softap_ssid );
}

void wificlient_set_softap_ssid( const char * softap_ssid ) {
    strlcpy( wificlient_config.softap_ssid, softap_ssid, sizeof( wificlient_config.softap_ssid ) );
}

const char *wificlient_get_softap_password( void ) {
    return( (const char*)wificlient_config.softap_password );
}

void wificlient_set_softap_password( const char * softap_password ) {
    strlcpy( wificlient_config.softap_password, softap_password, sizeof( wificlient_config.softap_password ) );
}

bool wificlient_get_enable_softap( void ) {
    return( wificlient_config.enable_softap );
}

void wificlient_set_enable_softap( bool enable_softap ) {
    wificlient_config.enable_softap = enable_softap;
}

int wificlient_get_timeout( void ) {
    return( wificlient_config.timeout );
}

void wificlient_set_timeout( int timeout ) {
    wificlient_config.timeout = timeout;
}

bool wificlient_get_low_bandwidth( void ) {
    return( wificlient_config.low_bandwidth );
}

void wificlient_set_low_bandwidth( bool low_bandwidth ) {
    wificlient_config.low_bandwidth = low_bandwidth;

    if(  wificlient_config.low_bandwidth )
        esp_wifi_set_bandwidth( ESP_IF_WIFI_STA, WIFI_BW_HT20 );
    else
        esp_wifi_set_bandwidth( ESP_IF_WIFI_STA, WIFI_BW_HT40 );
}

bool wificlient_get_low_power( void ) {
    return( wificlient_config.low_power );
}

void wificlient_set_low_power( bool low_power ) {
    wificlient_config.low_power = low_power;

    if( wificlient_config.low_power )
        esp_wifi_set_max_tx_power( 44 );
    else
        esp_wifi_set_max_tx_power( 80 );
}

void wificlient_save_settings( void ) {
    wificlient_config.save();
}