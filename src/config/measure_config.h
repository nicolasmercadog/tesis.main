
#ifndef _MEASURE_CONFIG_H
    #define _MEASURE_CONFIG_H

    #include "utils/basejsonconfig.h"
    
    #define     MEASURE_JSON_CONFIG_FILE     "/measure.json"    /** @brief defines json config file name */
    /**
     * @brief 
     */

    /**
     * @brief ioport config structure
     */
    class measure_config_t : public BaseJsonConfig {
        public:
            measure_config_t();
            float network_frequency = 50;
            int samplerate_corr = 146;
            bool true_rms = false;
            
        protected:
            ////////////// Available for overloading: //////////////
            virtual bool onLoad(JsonDocument& document);
            virtual bool onSave(JsonDocument& document);
            virtual bool onDefault( void );
            virtual size_t getJsonBufferSize() { return 8192; }
    };
#endif // _MEASURE_CONFIG_H
