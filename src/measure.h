#ifndef _MEASURE_H
    #define _MEASURE_H

    #define MAX_ADC_CHANNELS        8               /** @brief maximum channels from adc */
    #define VIRTUAL_ADC_CHANNELS    6               /** @brief maximum virtual adc channels after mapping */
    #define VIRTUAL_CHANNELS        13              /** @brief maximum virtual channels */

    #define MAX_GROUPS              6               /** @brief max groups */
    #define MAX_MICROCODE_OPS       10              /** @brief max channel opcodes size */

    #define numbersOfSamples        256             /** @brief number of samples per time domain */
    #define numbersOfFFTSamples     32              /** @brief number of sampled for fft per time domain */
    #define samplingFrequency       numbersOfSamples*VIRTUAL_ADC_CHANNELS //número de muestras (256*6)
    #define DELAY                   1000
    #define I2S_PORT                I2S_NUM_0
    // Macros relacionados con el módulo de medición
    #define HIGH_PASS_FILTER(current, last, sample) (high_pass_coef * ((last) + (sample) - (current)))



    #define OPMASK 0xf0

    #include <ArduinoJson.h>  // Asegúrate de incluir la librería ArduinoJson

    /**
     * @brief channel type enum
     */
    typedef enum {
        AC_CURRENT = 0,                     /** @brief measured AC current */
        AC_VOLTAGE,                         /** @brief measured AC voltage */
        DC_CURRENT,                         /** @brief measured DC current */
        DC_VOLTAGE,                         /** @brief measured DC voltage */
        AC_POWER,                           /** @brief measured AC power */
        AC_REACTIVE_POWER,                  /** @brief measured AC reactive power */
        DC_POWER,                           /** @brief measured DC power */
        NO_CHANNEL_TYPE                     /** @brief set channel to zero */
    } channel_type_t;
    /**
     * @brief opcode enum
     */
    typedef enum {
        BRK = 0x00,                         /** @brief no operation */
        ADD = 0x10,                         /** @brief add value from channel */
        SUB = 0x20,                         /** @brief subtract value from channel */
        MUL = 0x30,                         /** @brief multiply value from channel */
        NOP = 0x40,
        GET_ADC = 0x50,                     /** @brief get value from adc channel */
        SET_TO = 0x60,                      /** @brief set value to zero */
        FILTER = 0x70,                      /** @brief filter value */
        MUL_RATIO = 0x80,                   /** @brief multiply with ratio from channel */
        MUL_SIGN = 0x90,                    /** @brief store value into buffer */
        ABS = 0xa0,                         /** @brief abs */
        MUL_REACTIVE = 0xb0,                /** @brief multiply with reactive sign from channel */
        NEG = 0xd0,                         /** @brief change sign of a value */
        PASS_NEGATIVE = 0xe0,               /** @brief only pass negative values, otherwise set to zero */
        PASS_POSITIVE = 0xf0,               /** @brief only pass negative values, otherwise set to zero */
        OPCODE_END
    } opcode_t;
    /**
     * @brief channel enum
     */
    typedef enum {
        CHANNEL_NOP = -1,
        CHANNEL_0,
        CHANNEL_1,
        CHANNEL_2,
        CHANNEL_3,
        CHANNEL_4,
        CHANNEL_5,
        CHANNEL_6,
        CHANNEL_7,
        CHANNEL_8,
        CHANNEL_9,
        CHANNEL_10,
        CHANNEL_11,
        CHANNEL_12,
        CHANNEL_13,
        CHANNEL_14,
        CHANNEL_15,
        CHANNEL_END
    } channel_t;


    // Estructura para armónicos
struct HarmonicData {
    float fundamental;
    float thirdHarmonic;
    float fifthHarmonic;
    float seventhHarmonic;
    float ninthHarmonic;
    float thd;  // Distorsión Armónica Total en porcentaje
    float ffund;
    float fthird;
    float ffifth;
    float fsevent;
    float fninth;
    char uF[3] = "Hz";
    char porcentaje[3] = "%";
};

extern HarmonicData harmonic_values[VIRTUAL_CHANNELS];  // Declaración como extern

    /**
     * @brief group config structure
     */
    struct groupconfig {
        char        name[32];                           /** @brief group name */
        bool        active;                             /** @brief group output active/inactive */
    };



    /**
     * @brief channel config structure
     */
    struct channelconfig {
        char            name[32];                           /** @brief channel name */
        channel_type_t  type;                               /** @brief channel type */
        int             phaseshift;                         /** @brief channel adc phaseshift */
        float           ratio;                              /** @brief channel ratio */
        float           offset;                             /** @brief channel offset */
        float           rms;                                /** @brief channel rms */
        bool            true_rms;                           /** @brief channel rms calculated with square rms flag */
        float           sum;                                /** @brief channel sum */
        int             report_exp;                         /** @brief channel report exponent */
        int             group_id;                           /** @brief channel group ID for output groups */
        float           sign;                               /** @brief channel reactive power sign */
        uint8_t         operation[ MAX_MICROCODE_OPS ];     /** @brief opcode sequence */
    };

    // Declara la función en measure.h
    int calculate_phaseshift(int base_shift, int n, int samples);
    /**
     * @brief measurement init function
     */
    void measure_init(void);
    /**
     * @brief
     */
    void measure_mes(void);
    void measure_save_settings( void );
    /**
     *@brief establece el valor de corrección de phasshift para todos los canales de voltaje en números de muestras
    * @Param Corr Corrección Valor en la muestra
    * @return 0 si está bien o fallido
     */
    int measure_set_phaseshift( int corr );
/**
     * @Brief Establecer el valor de corrección de muestreo en números de muestra 
     * @param Corr el valor de corrección en números de muestras
     * @note es muy importante para la frecuencia de red precisa para calibrar
     * El muestreador con este valor
     */
    void measure_set_samplerate_corr( int samplerate_corr );
    /**
     * @brief Obtener valor central de la frecuencia de muestra
     * 
     * @return int 
     */
    int measure_get_samplerate_corr( void );
    /**
     * @brief Obtener frecuencia de voltaje
     * 
     * @return int 
     */
    float measure_get_network_frequency( void );
    /**
     * @brief establecer la frecuencia de voltaje entre 50 y 60Hz
     * 
     * @param voltage_frequency 
     */
    void measure_set_network_frequency( float voltage_frequency );
/**
     * @brief Obtenga el búfer de muestra actual con un size_of virtual_channels * NumbersOfsamples
     * 
     * @return uint16_t* puntero a una matriz uint16_t [virtual_channels] [NumbersOfSamples]
     */
    uint16_t * measure_get_buffer( void );
    /**
* @Brief Obtenga el búfer FFT actual con un size_of de virtual_channels * NumbersOffftSamples;
     * 
     * @return uint16_t* puntero a una matriz uint16_t [virtual_channels] [NumbersOffftSamples]
     */
    uint16_t * measure_get_fft( void );

    /**
     * @brief Inicio de tarea de medición
     */
    void measure_StartTask( void );
    /**
    * @Brief Obtiene la frecuencia neta actual
     * 
     * @return doble 50.0/60.0 si no se mide o el valor actual en HZ
     */
    double measure_get_max_freq( void );
    /**
     * @brief Obtenga el RMS de un canal dado
     * 
     * @param channel 
     * @return float 
     */
    float measure_get_channel_rms( int channel );
    /**
     * @brief Obtiene el cálculo de RMS con el método cuadrado RMS
     * 
     * @return true 
     * @return false 
     */
    bool measure_get_channel_true_rms( int channel );
    /**
     * @brief Calcula rms cuadrado
     * 
     * @param true_rms    true means enabled, false means disabled
     */
    void measure_set_channel_true_rms( int channel, bool true_rms );
    /**
     * @brief Obtiene el nombre del canal dado
     * 
     * @param channel   channel
     * @return char* 
     */
    char *measure_get_channel_name( uint16_t channel );
    /**
     * @brief  Setea el nombre del canal dado
     * 
     * @param channel   channel
     * @param name      pointer to a channel name string
     */
    void measure_set_channel_name( uint16_t channel, char *name );
    /**
     * @brief Obtiene el tipo del canal
     * 
     * @return uint8_t 
     */
    channel_type_t measure_get_channel_type( uint16_t channel );
    /**
     * @brief Setea el tipo de canal
     * 
     * @param channel 
     * @return channel_type_t 
     */
    void measure_set_channel_type( uint16_t channel, channel_type_t value );
    uint16_t* measure_get_fft_for_mqtt(StaticJsonDocument<4096>& doc);
    /**
     * @brief Obtiene el offset del canal
     * 
     * @param channel 
     * @return double 
     */
    double measure_get_channel_offset( uint16_t channel );
    /**
     * @brief Setea el offset del canal
     * 
     * @param channel 
     * @param channel_offset
     */    
    void measure_set_channel_offset( uint16_t channel, double channel_offset );
    /**
     * @brief Obtiene el ratio del canal
     * 
     * @param channel 
     * @return double 
     */
    double measure_get_channel_ratio( uint16_t channel );
    /**
     * @brief Setea el ratio del canal
     * 
     * @param channel 
     * @param channel_ratio 
     */
    void measure_set_channel_ratio( uint16_t channel, double channel_ratio );
    /**
     * @brief Obtiene el actual desplazamiento del canal dado
     * 
     * @param channel 
     * @return int16_t 
     */
    int measure_get_channel_phaseshift( uint16_t channel );
    /**
     * @brief Setea el actual desplazamiento del canal dado
     * 
     * @param channel       channel
     * @param value         phaseshift in sample
     */
    void measure_set_channel_phaseshift( uint16_t channel, int value );
    /**
     * @brief Obtiene el nombre del grupo
     * 
     * @param group 
     * @return const char* 
     */
    const char *measure_get_group_name( uint16_t group );
    /**
     * @brief Setea el nombre del grupo
     * 
     * @param group 
     * @param name 
     */
    void measure_set_group_name( uint16_t group, const char *name );
    /**
     * @brief Obtiene si el grupo está activo/inactivo
     * 
     * @param group 
     * @return true 
     * @return false 
     */
    bool measure_get_group_active( uint16_t group );
    /**
     * @brief Setea si el grupo está activo/inactivo
     * 
     * @param group 
     * @param active 
     */
    void measure_set_group_active( uint16_t group, bool active );
    /**
     * @brief Obtiene el id del grupo del canal dado
     * 
     * @param channel       channel
     * @return uint16_t 
     */
    int measure_get_channel_group_id( uint16_t channel );
    /**
     * @brief Setea el id del grupo del canal dado
     * 
     * @param channel       channel
     * @param groupID       group id
     */
    void measure_set_channel_group_id( uint16_t channel, int group_id );
    /**
     * @brief Obtiene los números de canales para un id del grupo dado
     * 
     * @param group_id      group id
     * @return int          number of channel with group id
     */
    int measure_get_channel_group_id_entrys( int group_id );
    /**
     * @brief Setea los números de canales para un id del grupo dado
     * 
     * @param group_id      group id
     * @param type          channel type
     * @return int          number of channel with group id
     */
    int measure_get_channel_group_id_entrys_with_type( int group_id, int type );
    /**
     * @brief Obtiene los números de canales dados a un id de grupo y tipo 
     * 
     * @param group_id      group id
     * @param type          channel type
     * @return int          number of channel with group id and type or -1
     */
    int measure_get_channel_with_group_id_and_type( uint16_t group_id, int type );
    /**
     * @brief Obtiene el exponente reportado en un canal
     * 
     * @param channel       channel number
     * @return int          -3, 0 or 3 -> milli, normal or kilo
     */
    int measure_get_channel_report_exp( uint16_t channel );
    /**
     * @brief Obtiene el exponente reportado al multiplicador del exponente
     * 
     * @param channel       channel number
     * @return float        0.001, 1 or 1000
     */
    float measure_get_channel_report_exp_mul( uint16_t channel );
    /**
     * @brief Setea el exponente dado al canal
     * 
     * @param channel       channel number
     * @param report_in     -3,0 or 3
     */
    void measure_set_channel_report_exp( uint16_t channel, int report_in );
    /**
     * @brief Obtiene la unidad dada al canal
     * 
     * @param channel       channel number
     * @return const char* 
     */
    const char *measure_get_channel_report_unit( uint16_t channel );
    /**
     * @brief Obtiene el estado válido de la medida
     * 
     * @return true         measurment are valid
     * @return false 
     */
    bool measure_get_measurement_valid( void );
    /**
     * @brief Setea la medición inválidad dada por un tiempo dado en segundos
     * 
     * @param sec 
     */
    void measure_set_measurement_invalid( int sec );
    /**
     * @brief Obtiene la secuencia del código de opciónes (opcode) para un canal dado
     * 
     * @param channel 
     * @return uint8_t*     pointer to a opcode sequnce with a size of MAX_MICROCODE_OPS
     */
    uint8_t * measure_get_channel_opcodeseq( uint16_t channel );
    /**
     * @brief Obtiene la secuencia del código de opciones (opcode) como un char array terminado por un valor en cero (NULL)
     * 
     * @param channel       channel to get a opcode sequence
     * @param len           max size in len bytes
     * @param dest          pointer to a char array to store the opcode sequence
     * @return char*        NULL if failed or a valid pointer
     */
    char * measure_get_channel_opcodeseq_str( uint16_t channel, uint16_t len, char *dest );
    /**
     * @brief Obtiene la secuencia del código de opciones (opcode) para un canal dado
     * 
     * @param channel       a give channel
     * @param value         pointer to a opcode sequence as byte array with a size of MAX_MICROCODE_OPS
     */
    void measure_set_channel_opcodeseq( uint16_t channel, uint8_t *value );
    /**
     * @brief Setea la secuencia del código de opciones (opcode) para un canal dado en forma de string
     * 
     * @param channel       a given channel
     * @param value         pointer to a opcode sequence as char array terminate with zero
     */
    void measure_set_channel_opcodeseq_str( uint16_t channel, const char *value );

#endif // _MEASURE_H
