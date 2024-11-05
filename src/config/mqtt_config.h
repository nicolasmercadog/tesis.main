/**
 * @file mqtt_config.h
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
#ifndef _MQTT_CONFIG_H
    #define _MQTT_CONFIG_H

    #include "utils/basejsonconfig.h"
    
    #define     MQTT_JSON_CONFIG_FILE     "/mqtt.json"    /** @brief defines json config file name */
    /**
     * @brief 
     */
    #define     MQTT_MAX_TEXT_SIZE   64
    /**
     * @brief ioport config structure
     */
    class mqtt_config_t : public BaseJsonConfig {
        public:
            mqtt_config_t();
            char server[ MQTT_MAX_TEXT_SIZE ] = "public.mqtthq.com"; // Change to HiveMQ broker
            int port = 1883; // Default port for MQTT
            char username[ MQTT_MAX_TEXT_SIZE ] = ""; // No username for public HiveMQ broker
            char password[ MQTT_MAX_TEXT_SIZE ] = ""; // No password for public HiveMQ broker
            char topic[ MQTT_MAX_TEXT_SIZE ] = "medidorenergia"; // Default topic
            int interval = 15; // Default interval for messages
            bool realtimestats = true; // Default real-time stats flag
            
        protected:
            ////////////// Available for overloading: //////////////
            virtual bool onLoad(JsonDocument& document);
            virtual bool onSave(JsonDocument& document);
            virtual bool onDefault( void );
            virtual size_t getJsonBufferSize() { return 8192; }
    };
#endif // _MQTT_CONFIG_H
