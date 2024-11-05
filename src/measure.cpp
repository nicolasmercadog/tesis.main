
#include <FreeRTOS.h>
#include <driver/i2s.h>
#include <arduinoFFT.h>
#include <math.h>
#include "config/measure_config.h"
#include "measure.h"
#include "config.h"

extern "C"
{                           // estos dos include serán interpretados como C y no como C++
#include "soc/syscon_reg.h" //permite controlar registros internos del microcontrolador, en este caso para el ADC
#include "soc/syscon_struct.h"
}

measure_config_t measure_config;
/*
 * Mapea los 8 canalas ADC reales en canales virtuales ADC
 * -1 significa no mapeado
 *
 * Por ejemplo: canal 5 mapea a canal virtual 0, 6 a 1 y 7 a 2
 */
int8_t channelmapping[MAX_ADC_CHANNELS] = {CHANNEL_3, CHANNEL_NOP, CHANNEL_NOP, CHANNEL_4, CHANNEL_5, CHANNEL_0, CHANNEL_1, CHANNEL_2};
/*Mapea los canales de ADC para contar las cantidad de canales y si no se desea incluir
entonces se utiliza el CHANNEL_NOP, por lo cual se usa el canal 3,4,5,0,1 y 2*/
/**
 *
 */
//Estructura para conexión global de armónicos
HarmonicData harmonic_values[VIRTUAL_CHANNELS];  // Definición del array global

/* Estructura que configura L1, L2, L3 según la cantidad máxima de grupos (MAX_GROUPS)*/
struct groupconfig groupconfig[MAX_GROUPS] = {
    {"L1", true}, // Por default inicia con L1 como activo y los demás desactivados
    {"L2", true},
    {"L3", true},
    {"all power", false},
    {"unused", false},
    {"unused", false}};
/*
 * Se define los tipos de canales virtuales, estos serán como canales ADC sólo que físicamente van a haber 6 canales, 3 de tensión y 3 de corriente, lo demás
    (potencia, potencia reactiva) saldrán de cálculos a partir de estos 6 canales físicos.
    El formato es:
        struct channelconfig {
        char            name[32];                            Nombre del canal
        channel_type_t  type;                                Tipo del canal
        int             phaseshift;                         Desplazamiento en fase ADC
        float           ratio;                              Coeficiente de amplitud
        float           offset;                             Desplazamiento en DC (vertical)
        float           rms;                                Si el canal es RMS
        bool            true_rms;                           El cálculo del verdadero RMS
        float           sum;                                Coeficiente de suma del canal
        int             report_exp;                         El exponente que lo conforma
        int             group_id;                           ID del grupo de salida que lo conforma
        float           sign;                               Signo del canal en potencia reactiva
        uint8_t         operation[ MAX_MICROCODE_OPS ];     Secuencia del opcode

    "Nombre del canal"      , tipo del canal    , desfasamiento en fase, factor de amplificación, desplazamiento en DC,
 */

struct channelconfig channelconfig[VIRTUAL_CHANNELS] = {
    {"L1 Corriente", AC_CURRENT, 0, 0.084, 0.0, 0.0, true, 0.0, 0, 0, 0.0, GET_ADC | CHANNEL_0, FILTER | 0, BRK, BRK, BRK, BRK, BRK, BRK, BRK, BRK},
    {"L1 Tensión", AC_VOLTAGE, 30, 0.63, 0.0, 0.0, true, 0.0, 0, 0, 0.0, GET_ADC | CHANNEL_1, FILTER | 0, BRK, BRK, BRK, BRK, BRK, BRK, BRK, BRK},
    {"L1 Potencia", AC_POWER, 0, 1.0, 0.0, 0.0, false, 0.0, 3, 0, 0.0, SET_TO | 1, MUL | CHANNEL_0, MUL | CHANNEL_1, MUL_RATIO | CHANNEL_0, MUL_RATIO | CHANNEL_1, ABS, BRK, BRK, BRK, BRK},
    {"L1 Potencia Reactiva", AC_REACTIVE_POWER, 0, 1.0, 0.0, 0.0, false, 0.0, 3, 0, 0.0, SET_TO | 1, MUL | CHANNEL_0, MUL | CHANNEL_1, MUL_RATIO | CHANNEL_0, MUL_RATIO | CHANNEL_1, PASS_NEGATIVE, MUL_REACTIVE | CHANNEL_1, ABS, NEG, BRK},
    {"L2 Corriente", AC_CURRENT, 0, 0.115, 0.0, 0.0, true, 0.0, 0, 1, 0.0, GET_ADC | CHANNEL_5, FILTER | 0, BRK, BRK, BRK, BRK, BRK, BRK, BRK, BRK},
    {"L2 Tensión", AC_VOLTAGE, 30, 0.63, 0.0, 0.0, true, 0.0, 0, 1, 0.0, GET_ADC | CHANNEL_4, FILTER | 0, BRK, BRK, BRK, BRK, BRK, BRK, BRK, BRK},
    {"L2 Potencia", AC_POWER, 0, 1.0, 0.0, 0.0, false, 0.0, 3, 1, 0.0, SET_TO | 1, MUL | CHANNEL_5, MUL | CHANNEL_4, MUL_RATIO | CHANNEL_5, MUL_RATIO | CHANNEL_4, ABS, BRK, BRK, BRK, BRK},
    {"L2 Potencia Reactiva", AC_REACTIVE_POWER, 0, 1.0, 0.0, 0.0, false, 0.0, 3, 1, 0.0, SET_TO | 1, MUL | CHANNEL_5, MUL | CHANNEL_4, MUL_RATIO | CHANNEL_5, MUL_RATIO | CHANNEL_4, PASS_NEGATIVE, MUL_REACTIVE | CHANNEL_4, ABS, NEG, BRK},
    {"L3 Corriente", AC_CURRENT, 0, 0.082, 0.0, 0.0, true, 0.0, 0, 2, 0.0, GET_ADC | CHANNEL_2, FILTER | 0, BRK, BRK, BRK, BRK, BRK, BRK, BRK, BRK},
    {"L3 Tensión", AC_VOLTAGE, 30, 0.63, 0.0, 0.0, true, 0.0, 0, 2, 0.0, GET_ADC | CHANNEL_3, FILTER | 0, BRK, BRK, BRK, BRK, BRK, BRK, BRK, BRK},
    {"L3 Potencia", AC_POWER, 0, 1.0, 0.0, 0.0, false, 0.0, 3, 2, 0.0, SET_TO | 1, MUL | CHANNEL_2, MUL | CHANNEL_3, MUL_RATIO | CHANNEL_2, MUL_RATIO | CHANNEL_3, ABS, BRK, BRK, BRK, BRK},
    {"L3 Potencia Reactiva", AC_REACTIVE_POWER, 0, 1.0, 0.0, 0.0, false, 0.0, 3, 2, 0.0, SET_TO | 1, MUL | CHANNEL_2, MUL | CHANNEL_3, MUL_RATIO | CHANNEL_2, MUL_RATIO | CHANNEL_3, PASS_NEGATIVE, MUL_REACTIVE | CHANNEL_3, ABS, NEG, BRK},
    {"Todas las Potencias", AC_POWER, 0, 1.0, 0.0, 0.0, false, 0.0, 3, 3, 0.0, SET_TO | 0, ADD | CHANNEL_0, ADD | CHANNEL_4, ADD | CHANNEL_2, BRK, BRK, BRK, BRK, BRK, BRK}

    // Cambio L1 current de 0,025989 a 0,06 al igual que L2 y L3.
    // La relación de voltaje se deja igual hasta que se encuentre el valor de calibración correcto.
    // Potencia Reactivo Multiplica V1*I1*(ratioV1)*(ratioI1)*(deja pasar lo negativo)*(obtiene el signo del reactivo de V1)*(módulo)*(lo cambia de signo)
    // Potencia Activa V1*I1*(ratioV1)*(ratioI1)*(módulo)
};

/* taske handle */
TaskHandle_t _MEASURE_Task; // Creación de la tarea para medir

volatile int TX_buffer = -1;                                // variable volátil para el buffer de transmisión iniciado vacío
uint16_t buffer[VIRTUAL_CHANNELS][numbersOfSamples];        // buffer para las variables donde especifica [canal][número de muestras (256)]
uint16_t buffer_fft[VIRTUAL_CHANNELS][numbersOfFFTSamples]; // buffer para la transf de fourier espeficando [canal][número de muestras para fft (32)]
uint16_t buffer_probe[VIRTUAL_CHANNELS][numbersOfSamples];  // buffer para punteros para banderas

double HerzvReal[numbersOfSamples * 4];                     /*variable de frecuencia real [n° de muestras *4] el 4 es porque se
                                                              toma una muestra cada 4 muestras verdaderas, es decir que se está                             muestreando cada 4 muestras*/
double HerzvImag[numbersOfSamples * 4];                     // lo mismo pasa con la variable de frecuencia imaginaria
double netfrequency_phaseshift, netfrequency_oldphaseshift; // estas variables definen el desfaje de la frecuencia de linea, la actual y la anterior
double netfrequency;                                        // esta variable guarda el valor de la frecuencia de línea
double vReal[numbersOfSamples]; // Buffer para los valores reales
double vImag[numbersOfSamples]; // Buffer para los valores imaginarios (en FFT se inicializan a cero)
arduinoFFT FFT2 = arduinoFFT();
static int measurement_valid = 3; /** empieza a contar en 3 para evitar basura */

void measure_Task(void *pvParameters)
{
    log_i("Se empieza a medir en nucleo: %d", xPortGetCoreID()); // log para ver cuando empieza la tarea de medición
    measure_init(); // Se inicia la función para medir
    while (true){measure_mes(); /* se repite hasta que no se termine la medida */}
}

void measure_StartTask(void)
{
    xTaskCreatePinnedToCore(
        measure_Task,               /* Función para implementar la tarea */
        "measure measurement Task", /* Nombre de la tarea */
        10000,                      /* Tamaño de la pila en palabras */
        NULL,                       /* Parámetro de entrada, no se asigna nada (NULL) */
        2,                          /* Prioridad de la tarea (mientras más altas, mayor prioridad) */
        &_MEASURE_Task,             /* Puntero al manejador de la tarea. */
        _MEASURE_TASKCORE);         /* Núcleo donde la tarea correrá (1 en este caso) */
}

void measure_init(void)
{ // función que prepara todo para la medición
    /*Se carga config desde json, este es creado para que se guarde la configuración inicial o modificada en la memoria interna del esp 
    utilizando SPIFFs */
    measure_config.load();                           // se carga la configuracion desde el archivo config/measure_config.cpp
    netfrequency = measure_config.network_frequency; // se carga la frecuencia de la linea (50hz de referencia)
    // en el ratio de muestreo se toma la frecuencia de muestreo (256*6=1536 hz) y se la multiplica por la frecuencia de la linea/2 + una corrección.
    int sample_rate = (samplingFrequency * measure_config.network_frequency / 2) + measure_config.samplerate_corr;
    // sample_rate = 1536 * 50/2 + 0 = 38400
    /**
     * se configura el i2s para obtener los datos desde el adc interno
     */
    const i2s_config_t i2s_config = {
        .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN), // modo del i2c
        // inicio modo maestro (proporciona señales del reloj), config para recibir datos, usa el ADC del microcontrolador
        .sample_rate = sample_rate,                   // Ratio de muestras (38400)
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, // Bits por muestras --> 16 bits
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT, // Formato del canal, sólo canal derecho (como no es audio, sólo sirve un canal)
        //.communication_format = i2s_comm_format_t( I2S_COMM_FORMAT_I2S ),           //Formato de la comunicación (i2s en este caso)
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,  // Banderas de interrupción de nivel 1
        .dma_buf_count = VIRTUAL_ADC_CHANNELS * 4, // Buffer es 6*4=24
        .dma_buf_len = numbersOfSamples,           // Tamaño del buffer 256
        .use_apll = false,                         // Evalúa si es necesario usar el APLL
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0};
    /**
     * Se setea la configuración del i2s
     */
    esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL); // instalación del driver i2c sin cola (queu)
    if (err != ESP_OK)
    {
        log_e("Failed installing driver: %d", err); // si no puede instalar el driver manda un error por log
        while (true)
            ;
    }
    /**
     * set adc mode
     */
    err = i2s_set_adc_mode(ADC_UNIT_1, (adc1_channel_t)6); // configura el modo del adc en i2s y se utilizará canal N°6 para conversión
    if (err != ESP_OK)
    {
        log_e("Failed installing driver: %d", err);
        while (true);
    }

    i2s_stop(I2S_PORT); // se para la comunicación de i2s en los puertos

    vTaskDelay(100); // delay de 100ms

    // En este caso se utiliza SYSCON SAR1(Successive Approximation Register ADC 1) para definir la cantidad de canales
    // virtuales que el ADC va a utilizar (empezando desde el 0 hasta el max-1)
    SYSCON.saradc_ctrl.sar1_patt_len = VIRTUAL_ADC_CHANNELS - 1;
    /**
     * [7:4] Channel
     * [3:2] Bit Width; 3=12bit, 2=11bit, 1=10bit, 0=9bit
     * [1:0] Attenuation; 3=11dB, 2=6dB, 1=2.5dB, 0=0dB
     * Acá se tiene SYSCON.saradc_sar1_patt_tab que es una matriz que contiene los patrones de configuración para
     * cada canal del ADC SAR1.
     * El patrón está en hexadecimal, configura cuatro canales con 0f (canal 0), 3f (canal1), 4f (canal2), 5f (canal3)
     * Luego dos canales más con 6f (canal 4) y 7f (canal 5), los demás los configura con 0.
     * Al final tengo SYSCON.saradc_ctrl.sar_clk_div = 5 donde sar_clk_div es un registro que define el divisor de reloj
     * para el ADC, en este caso divide por 5.
     */
    SYSCON.saradc_sar1_patt_tab[0] = 0x0f3f4f5f;
    SYSCON.saradc_sar1_patt_tab[1] = 0x6f7f0000;
    SYSCON.saradc_ctrl.sar_clk_div = 6;
    log_i("Measurement: I2S driver ready");
    i2s_start(I2S_PORT); // Inicia la medición por puertos I2C
}
/*
    Se toma la cantidad del buffer adc como es posible por un segundo y se calcula algunas cosas
 */

void measure_mes(void)
{
    int round = 0; /** contador ciclico */
    /**Limpia la suma para cada canal*/
    for (int i = 0; i < VIRTUAL_CHANNELS; i++) // empiezo a recorrer canal por canal
        channelconfig[i].sum = 0.0;            // hago que la suma en cada canal inicie en 0
    /**Obtiene el contador actual y calcula el tiempo de salida para capturar el buffer ADC*/
    uint64_t NextMillis = millis() + 1000l; // tomo el tiempo actual y le sumo un segundo estilo estar en el futuro y poder comparar en el futuro
    /**Obtiene numbersOfSamples * VIRTUAL_ADC_CHANNELS por ronda
     * Una ronda es 40/33.3ms (dependiendo de la frecuencia de línea) o dos periodos de la frecuencia de línea
     * despues de 950 ms calcula cada valor del canal como Vrms/Irms o frecuencia de linea/paseshift*/
    while (millis() < NextMillis)
    {

        static float adc_sample[VIRTUAL_CHANNELS], last_adc_sample[VIRTUAL_CHANNELS];   /** tomo las muestras del adc y tambien guardo la muestra anterior */
        static float temp_adc_sample[VIRTUAL_CHANNELS];                                 // variable temporanea para las muestras adc
        static float ac_filtered[VIRTUAL_CHANNELS], last_ac_filtered[VIRTUAL_CHANNELS]; /** variable para almacenar el ac filtrado por pasabajos y guardar el valor anterior al actual*/
        static float dc_filtered[VIRTUAL_CHANNELS][64];                                 /** esta variable guarda la señal filtrada dc, una matriz en donde cada canal tiene un array de 64 espacios */
        uint16_t *channel[VIRTUAL_ADC_CHANNELS];                                        /** puntero de cada canal a los canales virtuales */
        uint16_t adc_tempsamples[numbersOfSamples];                                     /** variable temporarea para guardar los valores del n° de muestras del adc */
        uint16_t adc_samples[VIRTUAL_ADC_CHANNELS][numbersOfSamples];                   /** se guardan las muestras según el n° de canal virtual y el n° de muestra */
        int phaseshift_0_degree;                                                        // guardo el valor del desfasaje despues de 0°
        int phaseshift_90_degree;                                                       // guardo el valor del desfasaje despues de 90°, me sirve para saber si la carga es capacitiva o inductiva

        for (int i = 0; i < VIRTUAL_ADC_CHANNELS; i++)
        {channel[i] = &adc_samples[i][0]; // se crea una lista de punteros a los canales para usar despues y que sea más rápido su uso.
        }
        /**
         * Obtiene una parte de datos del ADC y los ordena
         */
        for (int adc_chunck = 0; adc_chunck < VIRTUAL_ADC_CHANNELS; adc_chunck++)
        {
            /**
             * Obtiene un buffer ADC
             */
            size_t num_bytes_read = 0;
            esp_err_t err;
            err = i2s_read(I2S_PORT,
                           (char *)adc_tempsamples, /* puntero char hacia adc_tempsample */
                           sizeof(adc_tempsamples), /* tomo el tamaño de la adc_tempsable en bytes */
                           &num_bytes_read,         /* puntero a variable size_t donde se guarda el n° de bytes leidos */
                           100);                    /* sin timeout */
            /**
             * handle error
             */
            if (err != ESP_OK)
            {
                log_e("Error while reading DMA Buffer: %d", err); // si no se pudiera guardar o el buffer se quedó sin espacio entonces larga error
                while (true);
            } /**
               * check blocksize
               */
            num_bytes_read /= 2; // parto a la mitad la cantidad de bytes leidos
            if (num_bytes_read != numbersOfSamples)
            {
                log_e("block size != numberOfSamples, num_bytes_read = %d", num_bytes_read); // verifica que el n° de bytes leidos es igual al n° de muestras
                while (true);
            }
            /**
             * Ordena el buffer ADC con los canales virtuales
             * El buffer de muestras ADC contiene el siguiente esquema de datos con palabras de 16 bits
             *
             * [CH6][CH6][CH7][CH7][CH5][CH5][CH6][CH6][CH7].....
             *
             * Cada palabra de 16bits contiene el siguiente esquma de bits
             *  [15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0]
             *  [  canal    ][ muestra de 12 bits     ]
             * [15..12]     Canal
             * [11..0]      muestra de 12 bits
             */
            for (int i = 0; i < num_bytes_read; i++)
            { // voy desde 0 hasta el n° de bytes leidos
                /**
                 * Tomo las muestras del n° de canal correcto y las guardo
                 */
                int8_t chan = (adc_tempsamples[i] >> 12) & 0xf; // tomo los 12 bits de muestra, lo desplazo y para aislar los primeros 4 bits (0xf) del canal
                /**
                 * Verifico si el canal está asignado y si no es el último canal ADC, luego le asigno la muestra al canal virtual correcto usando la lista de punteros hacia ese canal
                    Además al asignarle la muestra me aseguro que le estoy pasando los 12 bits y no el canal con el & 0x0fff*/
                if (channelmapping[chan] != CHANNEL_NOP && chan < MAX_ADC_CHANNELS)
                {
                    *channel[channelmapping[chan]] = adc_tempsamples[i] & 0x0fff;
                    channel[channelmapping[chan]]++; // una vez terminado paso al canal siguiente
                }
            }
        }
        /**
         * Computo cada muestra para cada canal virtual
         */
        for (int n = 0; n < numbersOfSamples; n++)
        {
            for (int i = 0; i < VIRTUAL_CHANNELS; i++)
            {
                /**
                 * abort if group not active or channel not used
                 */
                if (!groupconfig[channelconfig[i].group_id].active || channelconfig[i].type == NO_CHANNEL_TYPE)
                {
                    // verifico si el canal está con un id y si está activo
                    buffer[i][n] = 2048;        // asigno el tamaño del buffer
                    channelconfig[i].sum = 0.0; // pongo la suma en cero de la conf del canal
                    adc_sample[i] = 0.0;        // pongo todas las muestras en 0 para que no haya basura
                    continue;
                }
                /**
                 * Obtengo la muestra justa
                 */
                for (int operation = 0; operation < MAX_MICROCODE_OPS; operation++)
                {
                    /**
                     * precalc the phaseshift from 360 degree to number of samples
                     * and prevent a channel clipping issue
                     */
                    // para el cálculo del desfasaje inicial (en fase) tomo el número de muestras/2 y la divido por el ang° total, esto se mult por el módulo entre la fase de conf inicial y 360
                    //  es decir, como la señal es impar divido la cantidad de muestras en 2 pero en el total del °, y si el mod del desfaje conf es menor a 1 entonces está en fase
                    phaseshift_0_degree = ((numbersOfSamples / 2) / 360.0) * ((channelconfig[i].phaseshift) % 360);
                    phaseshift_0_degree = (n + phaseshift_0_degree) % numbersOfSamples; // el desfasaje en 0 va a ser el mod entre el valor recien calculado + n y el número de muestras
                    if (phaseshift_0_degree == numbersOfSamples - 1)                    // si se llega al final entonces vuelve a 0.
                        phaseshift_0_degree = 0;
                    phaseshift_90_degree = ((numbersOfSamples / 2) / 360.0) * ((channelconfig[i].phaseshift + 90) % 360); // lo mismo para 90° sólo que se suma ese desfasaje de 90°
                    phaseshift_90_degree = (n + phaseshift_90_degree) % numbersOfSamples;
                    if (phaseshift_90_degree == numbersOfSamples - 1)
                        phaseshift_90_degree = 0;
                    /**
                     * mask the channel out and store it in a separate variable for later use
                     * and check if op_channel valid
                     */
                    int op_channel = channelconfig[i].operation[operation] & ~OPMASK; // guardo el código op del canal según su posición y la operación a realizar mas el desplazamiento que me dice que canal es (OPMASK).
                    if (op_channel >= VIRTUAL_CHANNELS)
                        continue; // si está dentro del rango de canales virtuales entonces sigo
                    /**
                     * oparate the current channel operateion
                     */
                    switch (channelconfig[i].operation[operation] & OPMASK)
                    { // acá elijo la operación según el canal seleccionado
                    case ADD:
                        adc_sample[i] += adc_sample[op_channel]; // tomo la muestra de entrada y la sumo con la muestra del canal seleccionado
                        break;
                    case SUB:
                        adc_sample[i] -= adc_sample[op_channel]; // tomo la muestra de entrada y la resto con la muestra del canal seleccionado
                        break;
                    case MUL:
                        adc_sample[i] = adc_sample[i] * adc_sample[op_channel]; // tomo la muestra de entrada y la multiplico con la muestra del canal seleccionado
                        break;
                    case MUL_RATIO:
                        adc_sample[i] *= channelconfig[op_channel].ratio; // tomo la muestra de entrada y la multiplico por el factor ingresado por pantalla
                        break;
                    case MUL_SIGN:
                        if (adc_sample[op_channel] > 0.0) // corroboro si la muestra es mayor a cero entonces es positivo, sino negativo para obtener el signo
                            adc_sample[i] *= 1.0;
                        else if (adc_sample[op_channel] < 0.0)
                            adc_sample[i] *= -1.0;

                        break;
                    case MUL_REACTIVE:
                        temp_adc_sample[i] = buffer[op_channel][phaseshift_90_degree] - 2048.0; // para multplicar por la el signo del reactivo (capacitivo -1, inductivo +1 o resistiva cero)
                                                                                                // lo que hago es buscar la muestra desfasada y ver el signo
                        if (adc_sample[op_channel] > 0.0)
                            temp_adc_sample[i] = temp_adc_sample[i] * 1.0;
                        else if (adc_sample[op_channel] < 0.0)
                            temp_adc_sample[i] = temp_adc_sample[i] * -1.0;

                        if (temp_adc_sample[i] > 0.0)
                            channelconfig[i].sign = 1.0;
                        else if (temp_adc_sample[i] < 0.0)
                            channelconfig[i].sign = -1.0;

                        adc_sample[i] *= channelconfig[i].sign;

                        break;
                    case ABS:
                        adc_sample[i] = fabs(adc_sample[i]); // tomo la muestra de entrada y le aplico el valor absoluto con fabs
                        break;
                    case NEG:
                        adc_sample[i] = adc_sample[i] * -1.0; // tomo la muestra de entrada y la multiplico por -1 para tener el negativo
                        break;
                    case PASS_NEGATIVE:
                        if (adc_sample[i] > 0.0) // para pasar los negativos sólo hago cero los positivos
                            adc_sample[i] = 0.0;
                        break;
                    case PASS_POSITIVE:
                        if (adc_sample[i] < 0.0) // para pasar los positivos sólo hago cero los negativos
                            adc_sample[i] = 0.0;
                        break;
                    case GET_ADC:
                        if (op_channel >= 0 && op_channel < VIRTUAL_ADC_CHANNELS) // verifico si el canal está activo y dentro del rango
                            {adc_sample[i] = adc_samples[op_channel][phaseshift_0_degree] + channelconfig[i].offset;
                            }
                        // tomo la muestra del canal seleccionado con su desfasaje y le sumo el desplazamiento en vertical (generalmente es cero)
                        else
                            continue;
                        break;
                    case SET_TO:
                        adc_sample[i] = op_channel; // asigno el canal en donde se guarda la muestra
                        break;
                    case FILTER:
                        switch (channelconfig[i].type)
                        { // en el caso del filtro se trata de quitar las componentes reactivas que pueden ensuciar la señal
                        case AC_CURRENT:
                        case AC_VOLTAGE:
                        case AC_POWER:
                        case AC_REACTIVE_POWER:
                            /**
                             * primero filtro la señal dc
                             */
                            ac_filtered[i] = (0.9989) * (last_ac_filtered[i] + adc_sample[i] - last_adc_sample[i]);
                            last_ac_filtered[i] = ac_filtered[i];
                            last_adc_sample[i] = adc_sample[i];
                            adc_sample[i] = ac_filtered[i];
                            /**
                             * luego se filtra el valor medio con un filtro pasa bajo
                             */
                            if (op_channel)
                            {
                                int mul = 1;
                                if (op_channel <= 6)
                                    mul = 1 << op_channel;
                                else
                                    mul = 1 << 6;
                                dc_filtered[i][n % mul] = adc_sample[i];
                                adc_sample[i] = 0.0;
                                for (int a = 0; a < mul; a++)
                                    adc_sample[i] += dc_filtered[i][a];
                                adc_sample[i] /= mul;
                            }
                            else
                            {
                                adc_sample[i] = adc_sample[i];
                            }
                            break;
                        case DC_CURRENT:
                        case DC_VOLTAGE: // en este caso sólo se deja la señal en dc, filtrando la ac con un pasa bajos
                            if (op_channel)
                            {
                                int mul = 1;
                                if (op_channel <= 6)
                                    mul = 1 << op_channel;
                                else
                                    mul = 1 << 6;

                                dc_filtered[i][n % mul] = adc_sample[i];
                                adc_sample[i] = 0.0;
                                for (int a = 0; a < mul; a++)
                                    adc_sample[i] += dc_filtered[i][a];
                                adc_sample[i] /= mul;
                            }
                            else
                            {
                                adc_sample[i] = adc_sample[i];
                            }
                            break;
                        default:
                            adc_sample[i] = adc_sample[i];
                            break;
                        }
                        break;
                    case NOP:
                        break;
                    default:
                        operation = MAX_MICROCODE_OPS;
                        break;
                    }
                }
                /**
                 * Suma y almacena datos si ChannelGroup está activo
                 */
                if (channelconfig[i].type == AC_VOLTAGE && measure_get_channel_ratio(i) > 5) // verifico si tomo datos de tenión y si el ratio es menor a 5
                    buffer[i][n] = 2048;                                                     // si es TRUE le asigno 2048
                else
                    // si no se cumple entonces veo el valor de la muestra es menor a 0 en esa posición, si es mayor entonces le asigno el valor de la muestra + 2048
                    buffer[i][n] = (adc_sample[i] + 2048) < 0.0 ? 0 : adc_sample[i] + 2048;

                switch (channelconfig[i].type)
                {
                case AC_CURRENT:
                case AC_VOLTAGE: // Si true_rms es verdadero, se añade el cuadrado del valor de la muestra del ADC (adc_sample[i]) a channelconfig[i].sum.
                    if (channelconfig[i].true_rms)
                        channelconfig[i].sum += adc_sample[i] * adc_sample[i];
                    else
                        channelconfig[i].sum += fabs(adc_sample[i]); // si no está seleccionado solo se obtiene el módulo
                    break;
                case AC_POWER:
                case AC_REACTIVE_POWER:
                case DC_CURRENT:
                case DC_VOLTAGE:
                case DC_POWER: // Lo mismo que AC
                    if (channelconfig[i].true_rms)
                        channelconfig[i].sum += adc_sample[i] * adc_sample[i];
                    else
                        channelconfig[i].sum += adc_sample[i];
                    break;
                case NO_CHANNEL_TYPE:
                    break;
                }
            }
        }
        /**
         * Se verifica si se tienen datos para copiar, luego se llena el buffer de prueba y y se vuelve a -1 para marcar que terminó para esperar nuevos datos
         */
        if (TX_buffer != -1)
        {
            memcpy(&buffer_probe[0][0], &buffer[0][0], sizeof(buffer));
            TX_buffer = -1;
        }
        /**
         * Hacer 4 rondas y busca el primer canal de Tensión AC que encuentre, una vez que lo encuentra toma la muestra de frecuencia real e imaginaria, guarda las reales luego sale del bucle
           Acá es donde guardan las cosas para luego pasarlo a hacer la FFT*/
        if (round < 4)
        {
            for (int i = 0; i < VIRTUAL_CHANNELS; i++)
            {
                if (channelconfig[i].type == AC_VOLTAGE)
                {
                    for (int sample = 0; sample < numbersOfSamples; sample++)
                    {
                        HerzvReal[(round * numbersOfSamples) + sample] = buffer[i][sample];
                        HerzvImag[(round * numbersOfSamples) + sample] = 0;
                    }
                    break;
                }
            }
        }
        /**
         * Sale del bucle y aumenta la ronda
         */
        round++;
    }
    /**
     * De nuevo veo si el canal es de voltaje AC y si el ratio es menor a 5
     */
    if (channelconfig[1].type == AC_VOLTAGE && measure_get_channel_ratio(1) <= 5.0)
    {
        /**
         * Calculo el cambio de fase entre el desplazamiento de la última fase
         */
        // genero una variable FFT con la frecuencia real, imaginaria y le asigno la cantidad de muestras (256*4 para evitar aliasing) y la frecuencia de red (256*50)
        arduinoFFT FFT = arduinoFFT(HerzvReal, HerzvImag, numbersOfSamples * 4, (numbersOfSamples)*measure_get_network_frequency());
        // aplico una ventana a FFT tipo hamming para que no tenga discontinuidad en los valores de los extremos y tomar sólo una porción de la señal muestreada
        for (int i = 0; i < numbersOfSamples * 4; i++)
    {
        HerzvReal[i] *= 0.54 - 0.46 * cos(2 * PI * i / (numbersOfSamples * 4));
    }
        //FFT.Windowing(FFT_WIN_TYP_HAMMING, FFT_REVERSE);      // guardo el resultado en FFT_REVERSE
        FFT.Compute(FFT_REVERSE);                             // luego realizo la FFT del ese valor
        netfrequency_oldphaseshift = netfrequency_phaseshift; // guardo el valor de frecuencia actual
        // calculo el nuevo valor de la frecuencia viendo si el desplazamiento de fase está dentro de +-180°
        netfrequency_phaseshift = atan2(HerzvReal[8], HerzvImag[8]) * (180.0 / PI) + 180;
        /**
         * si está entre +-180° entonces
         */
        if ((netfrequency_phaseshift - netfrequency_oldphaseshift) < 180 && (netfrequency_phaseshift - netfrequency_oldphaseshift) > -180)
        {
            static float netfrequency_filter[16];     // genero un array para filtrar la frecuencia
            static int index = 0;                     // genero un indice en 0
            static bool netfrequency_firstrun = true; // genero un true para saber si es la primera vez que leo la frecuencia

            if (netfrequency_firstrun)
            {
                for (int i = 0; i < 16; i++)
                    netfrequency_filter[i] = measure_get_network_frequency(); // si es la primera vez entonces guardo ese valor, sino devuelve un falso
                netfrequency_firstrun = false;
            }
            // voy en el índice y guardo la dif entre frec actual y anterior multiplicado por un valor para pasarlo a radianes y normalizado en 90° mas la frecuencia de red actual
            netfrequency_filter[index] = (netfrequency_phaseshift - netfrequency_oldphaseshift) * ((1.0f / PI) / 90) + measure_get_network_frequency();
            // pongo en 0 la frecuencia de la red
            netfrequency = 0.0;
            for (int i = 0; i < 16; i++)
                netfrequency += netfrequency_filter[i]; // sumo a la frecuencia de la red los valores filtrados (16 uds)
            netfrequency /= 16.0;                       // una vez sumado lo divido por 16 para tener el promedio

            if (index < 16)
                index++;
            else
                index = 0;
        }
    }
    else
    {
        netfrequency = measure_get_network_frequency(); // si no está entre +-180° entonces devuelvo la frecuencia de red medida
    }
    // voy por todos los canales virtuales
    for (int i = 0; i < VIRTUAL_CHANNELS; i++)
    {
        /*
         *
         */
            log_i("Channel %d, measure %f",i,measure_get_channel_rms(i));
        switch (channelconfig[i].type)
        {
        case DC_CURRENT:
            if (channelconfig[i].ratio > 5)
            {
                channelconfig[i].rms = channelconfig[i].ratio;
            }
            else
            { // si es mayor a 5 el ratio obtengo el valor rms por el ratio y ala raiz cuadrada de la suma configurada y el n° de muestras
                if (channelconfig[i].true_rms)
                    channelconfig[i].rms = channelconfig[i].ratio * sqrt(channelconfig[i].sum / (numbersOfSamples * round));
                else
                    channelconfig[i].rms = channelconfig[i].ratio * (channelconfig[i].sum / (numbersOfSamples * round));
            }
            break;
        case AC_CURRENT:
        case DC_POWER:
        case AC_POWER:
        case AC_REACTIVE_POWER: // para todos los casos veo si el ratio es menor a 5 y le asigno a cada valor rms el valor del ratio
            if (channelconfig[i].ratio > 5)
            {
                channelconfig[i].rms = channelconfig[i].ratio;
            }
            else
            { // si es mayor a 5 el ratio obtengo el valor rms por el ratio y ala raiz cuadrada de la suma configurada y el n° de muestras
                if (channelconfig[i].true_rms)
                    channelconfig[i].rms = channelconfig[i].ratio * sqrt(channelconfig[i].sum / (numbersOfSamples * round));
                else
                    channelconfig[i].rms = channelconfig[i].ratio * (channelconfig[i].sum / (numbersOfSamples * round));
            }
            break;
        case AC_VOLTAGE:
        case DC_VOLTAGE:
            if (channelconfig[i].ratio > 5)
            {
                channelconfig[i].rms = channelconfig[i].ratio;
                log_i("Ratio mayor 5V en %d : %f", i, channelconfig[i].rms);
                vTaskDelay(200);
            }
            else
            { // si es mayor a 5 el ratio obtengo el valor rms por el ratio y ala raiz cuadrada de la suma configurada y el n° de muestras
                if (channelconfig[i].true_rms)
                {

                    if ((channelconfig[i].rms = channelconfig[i].ratio * sqrt(channelconfig[i].sum / (numbersOfSamples * round))) > 0)
                    {
                        channelconfig[i].rms = channelconfig[i].ratio * sqrt(channelconfig[i].sum / (numbersOfSamples * round));
                        // log_i("Ratio menor 5 y truerms V en %d : %f",i,channelconfig[i].rms);
                    }
                    else
                    {
                        channelconfig[i].rms = 0;
                        //  log_i("Ratio menor 5 y truerms V en %d : %f",i,channelconfig[i].rms);
                    }
                }
                else
                {
                    if ((channelconfig[i].rms = channelconfig[i].ratio * (channelconfig[i].sum / (numbersOfSamples * round))) > 40)
                    {
                        channelconfig[i].rms = channelconfig[i].ratio * (channelconfig[i].sum / (numbersOfSamples * round));
                        //    log_i("Ratio menor 5 y rms V en %d : %f",i,channelconfig[i].rms);
                        // vTaskDelay(200);
                    }
                    else
                    {
                        channelconfig[i].rms = 0;
                        //  log_i("Ratio menor 5 y rms V en %d : %f",i,channelconfig[i].rms);
                        // vTaskDelay(200);
                    }
                }
            }
            break;
        case NO_CHANNEL_TYPE:
            break;
        }

        if (measurement_valid > 0)
            channelconfig[i].rms = 0.0; // si la medición no es valida entonces asigno al rms 0

        if (measurement_valid > 0)
            measurement_valid--;


    
    /* for (int j = 0; j < numbersOfSamples; j++) {
                vReal[j] = buffer[i][j]; // Copia las muestras para realizar la FFT
                vImag[j] = 0.0; // Inicia el buffer de valores imaginarios en cero
            }

            // Realizar la FFT
            FFT2.Windowing(vReal, numbersOfSamples, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
            FFT2.Compute(vReal, vImag, numbersOfSamples, FFT_FORWARD);
            FFT2.ComplexToMagnitude(vReal, vImag, numbersOfSamples);

            // Calcular la THD
            double fundamental = vReal[1]; // Bin 1 es la frecuencia fundamental
            double harmonic_sum = 0.0;
            //Serial.println("Canal: ");
            //Serial.println(i);
            //Serial.println("Fundamental 50Hz: ");
            //Serial.println(vReal[1]);
            //Serial.println("3er armónico 150Hz: ");
            //Serial.println(vReal[3]);
            //Serial.println("5to armónico 300Hz: ");
            //Serial.println(vReal[5]);
            vTaskDelay(500);
            for (int k = 2; k < (numbersOfSamples / 2); k++) {
                harmonic_sum += vReal[k] * vReal[k];
            }
            harmonic_sum = sqrt(harmonic_sum);
            double thd = harmonic_sum / fundamental;
            //Serial.print("THD del Canal ");
            //Serial.print(i);
            //Serial.print(": ");
            //Serial.println(thd);
            */
    
    }


     // FFT y THD por canal
           

     /*double vReal[numbersOfFFTSamples * 2];
    double vImag[numbersOfFFTSamples * 2];
    arduinoFFT FFT = arduinoFFT(vReal, vImag, numbersOfFFTSamples * 2, samplingFrequency * measure_get_network_frequency() / 4);
    log_i("Bandera 1");
    for (int channel = 0; channel < VIRTUAL_CHANNELS; channel++)
    {
        for (int i = 0; i < numbersOfFFTSamples * 2; i++)
        {
            vReal[i] = buffer_probe[channel][(i * 6) % numbersOfSamples];
            vImag[i] = 0;
        }
        
        FFT.Windowing(FFT_WIN_TYP_RECTANGLE, FFT_REVERSE);
        FFT.Compute(FFT_REVERSE);
        FFT.ComplexToMagnitude();

       // log_i("Canal %d:", channel);

        for (int i = 1; i < numbersOfFFTSamples; i++)
        {
            double frequency = (i * measure_get_network_frequency());
            
            // Identificar los armónicos de la frecuencia fundamental de 50Hz (50Hz, 150Hz, 250Hz, etc.)
            if (frequency == 50.0 || frequency == 150.0 || frequency == 250.0 ||
                frequency == 350.0 || frequency == 450.0)
            {
                buffer_fft[channel][i] = static_cast<uint16_t>(vReal[i]);
                
                // Mostrar el valor del armónico y su frecuencia
             //   log_i("Armónico %d: Frecuencia: %f Hz, Magnitud: %f", i, frequency, vReal[i]);
                vTaskDelay(200);
            }
        }

        //log_i("");  // Línea en blanco para separación entre canales
    }*/

}


int measure_get_samplerate_corr(void)
{
    return (measure_config.samplerate_corr);
}

void measure_set_samplerate_corr(int samplerate_corr)
{ // esta función corrige la tasa de muestreo
    measure_config.samplerate_corr = samplerate_corr;

    esp_err_t err; // crea una variable de error de muestreo (dif)
    // ajusta la tasa de muestreo de error primero, llamando a i2s, luego haciendo un promedio entre la frecuencia de linea, la frecuencia muestreada y la corrección
    err = i2s_set_sample_rates(I2S_PORT, (samplingFrequency * measure_config.network_frequency / 2) + measure_config.samplerate_corr);
    if (err != ESP_OK)
    { // si no devuelve un ok entonces marca un log con error
        log_e("Failed set samplerate: %d", err);
        while (true)
            ;
    }
}

float measure_get_network_frequency(void)
{
    return (measure_config.network_frequency);
}

void measure_set_network_frequency(float network_frequency)
{
    if (network_frequency >= 16.0 && network_frequency <= 120.0)
    {
        measure_config.network_frequency = network_frequency;
        esp_err_t err;
        err = i2s_set_sample_rates(I2S_PORT, (samplingFrequency * measure_config.network_frequency / 2) + measure_config.samplerate_corr);
        if (err != ESP_OK)
        {
            log_e("Failed set samplerate: %d", err);
            while (true)
                ;
        }
    }
}

uint16_t *measure_get_buffer(void)
{
    TX_buffer = 0;
    while (TX_buffer == 0)
    {
    }
    return (&buffer_probe[0][0]);
}

#include <arduinoFFT.h>
#include <Arduino.h>

uint16_t *measure_get_fft(void)
{
    double vReal[numbersOfFFTSamples * 2];
    double vImag[numbersOfFFTSamples * 2];
    double temp = 0;
    arduinoFFT FFT = arduinoFFT(vReal, vImag, numbersOfFFTSamples * 2, samplingFrequency * measure_get_network_frequency() / 4);

    for (int channel = 0; channel < VIRTUAL_CHANNELS; channel++)
    {
        for (int i = 0; i < numbersOfFFTSamples * 2; i++)
        {
            vReal[i] = buffer_probe[channel][(i * 6) % numbersOfSamples];
            vImag[i] = 0;
        }
        FFT.Windowing(FFT_WIN_TYP_RECTANGLE, FFT_REVERSE);
        FFT.Compute(FFT_REVERSE);
        FFT.ComplexToMagnitude();

        for (int i = 1; i < numbersOfFFTSamples; i++)
        {
            buffer_fft[channel][i] = vReal[i];

                   
        //Calcular y almacenar los valores de los armónicos
        temp=measure_get_channel_rms(channel)/vReal[3];

        harmonic_values[channel].fundamental = vReal[3]*temp;
        harmonic_values[channel].thirdHarmonic = vReal[9]*temp;
        harmonic_values[channel].fifthHarmonic = vReal[15]*temp;
        harmonic_values[channel].seventhHarmonic = vReal[21]*temp;
        harmonic_values[channel].ninthHarmonic = vReal[27]*temp;
        harmonic_values[channel].ffund = measure_get_max_freq();
        harmonic_values[channel].fthird = measure_get_max_freq()*3;
        harmonic_values[channel].ffifth = measure_get_max_freq()*5;
        harmonic_values[channel].fsevent = measure_get_max_freq()*7;
        harmonic_values[channel].fninth = measure_get_max_freq()*9;


        // Calcular la Distorsión Armónica Total (THD)
        double thd_numerator = sqrt(
            pow(harmonic_values[channel].thirdHarmonic, 2) +
            pow(harmonic_values[channel].fifthHarmonic, 2) +
            pow(harmonic_values[channel].seventhHarmonic, 2) +
            pow(harmonic_values[channel].ninthHarmonic, 2)
        );

        harmonic_values[channel].thd = (harmonic_values[channel].fundamental > 0) ?
            (thd_numerator / harmonic_values[channel].fundamental) * 100 : 0;
            temp=0;
            // log_i("V [%d][%d] : %2x",channel,i,buffer_fft[channel][i]);
            // vTaskDelay(50);
        }
        return (&buffer_fft[0][0]);
    }
}


double measure_get_max_freq(void)
{
    return (netfrequency);
}

float measure_get_channel_rms(int channel)
{
    return (channelconfig[channel].rms * measure_get_channel_report_exp_mul(channel));
}

char *measure_get_channel_name(uint16_t channel)
{
    if (channel >= VIRTUAL_CHANNELS)
        return (0);

    return (channelconfig[channel].name);
}

void measure_set_channel_name(uint16_t channel, char *name)
{
    if (channel >= VIRTUAL_CHANNELS)
        return;

    strlcpy(channelconfig[channel].name, name, sizeof(channelconfig[channel].name));
}

bool measure_get_channel_true_rms(int channel)
{
    return (channelconfig[channel].true_rms);
}

void measure_set_channel_true_rms(int channel, bool true_rms)
{
    channelconfig[channel].true_rms = true_rms;
}

channel_type_t measure_get_channel_type(uint16_t channel)
{
    if (channel >= VIRTUAL_CHANNELS)
        return (NO_CHANNEL_TYPE);

    return (channelconfig[channel].type);
}

void measure_set_channel_type(uint16_t channel, channel_type_t type)
{
    if (channel >= VIRTUAL_CHANNELS)
        return;

    channelconfig[channel].type = type;
}

double measure_get_channel_offset(uint16_t channel)
{
    if (channel >= VIRTUAL_CHANNELS)
        return (0.0);

    return (channelconfig[channel].offset);
}

void measure_set_channel_offset(uint16_t channel, double channel_offset)
{
    if (channel >= VIRTUAL_CHANNELS)
        return;

    channelconfig[channel].offset = channel_offset;
}

double measure_get_channel_ratio(uint16_t channel)
{
    if (channel >= VIRTUAL_CHANNELS)
        return (0.0);

    return (channelconfig[channel].ratio);
}

void measure_set_channel_ratio(uint16_t channel, double channel_ratio)
{
    if (channel >= VIRTUAL_CHANNELS)
        return;

    channelconfig[channel].ratio = channel_ratio;
}

int measure_get_channel_phaseshift(uint16_t channel)
{
    if (channel >= VIRTUAL_CHANNELS)
        return (0);

    return (channelconfig[channel].phaseshift % 360);
}

void measure_set_channel_phaseshift(uint16_t channel, int value)
{
    if (channel >= VIRTUAL_CHANNELS)
        return;

    channelconfig[channel].phaseshift = (value % 360);
}

const char *measure_get_group_name(uint16_t group)
{
    if (group >= MAX_GROUPS)
        return ("");

    return ((const char *)groupconfig[group].name);
}

void measure_set_group_name(uint16_t group, const char *name)
{
    if (group >= MAX_GROUPS)
        return;

    strlcpy(groupconfig[group].name, name, sizeof(groupconfig[group].name));
}

bool measure_get_group_active(uint16_t group)
{
    if (group >= MAX_GROUPS)
        return (false);

    return (groupconfig[group].active);
}

void measure_set_group_active(uint16_t group, bool active)
{
    if (group >= MAX_GROUPS)
        return;

    groupconfig[group].active = active;
}

int measure_get_channel_group_id(uint16_t channel)
{
    if (channel >= VIRTUAL_CHANNELS)
        return (0);

    return (channelconfig[channel].group_id);
}

void measure_set_channel_group_id(uint16_t channel, int group_id)
{
    if (channel >= VIRTUAL_CHANNELS)
        return;

    channelconfig[channel].group_id = group_id;
}

int measure_get_channel_group_id_entrys(int group_id)
{
    int groupd_id_entrys = 0;

    for (int i = 0; i < VIRTUAL_CHANNELS; i++)
        if (channelconfig[i].group_id == group_id && channelconfig[i].type != NO_CHANNEL_TYPE)
            groupd_id_entrys++;

    return (groupd_id_entrys);
}

int measure_get_channel_group_id_entrys_with_type(int group_id, int type)
{
    int groupd_id_entrys = 0;

    for (int i = 0; i < VIRTUAL_CHANNELS; i++)
        if (channelconfig[i].group_id == group_id && channelconfig[i].type == type)
            groupd_id_entrys++;

    return (groupd_id_entrys);
}

int measure_get_channel_with_group_id_and_type(int group_id, int type)
{
    for (int i = 0; i < VIRTUAL_CHANNELS; i++)
        if (channelconfig[i].group_id == group_id && channelconfig[i].type == type)
            return (i);

    return (-1);
}

int measure_get_channel_report_exp(uint16_t channel)
{
    if (channel >= VIRTUAL_CHANNELS)
        return (false);

    return (channelconfig[channel].report_exp);
}

float measure_get_channel_report_exp_mul(uint16_t channel)
{
    if (channel >= VIRTUAL_CHANNELS)
        return (1.0);

    return (1.0 / pow(10, channelconfig[channel].report_exp));
}

void measure_set_channel_report_exp(uint16_t channel, int report_exp)
{
    if (channel >= VIRTUAL_CHANNELS)
        return;

    channelconfig[channel].report_exp = report_exp;
}

const char *measure_get_channel_report_unit(uint16_t channel)
{ // para reportar la unidad sólo son if según el valor elegido
    switch (channelconfig[channel].type)
    {
    case AC_CURRENT:
    case DC_CURRENT:
        if (channelconfig[channel].report_exp == -3)
            return ("mA");
        else if (channelconfig[channel].report_exp == 0)
            return ("A");
        else if (channelconfig[channel].report_exp == 3)
            return ("kA");
        return ("-");
    case AC_VOLTAGE:
    case DC_VOLTAGE:
        if (channelconfig[channel].report_exp == -3)
            return ("mV");
        else if (channelconfig[channel].report_exp == 0)
            return ("V");
        else if (channelconfig[channel].report_exp == 3)
            return ("kV");
        return ("-");
    case AC_POWER:
    case DC_POWER:
        if (channelconfig[channel].report_exp == -3)
            return ("mW");
        else if (channelconfig[channel].report_exp == 0)
            return ("W");
        else if (channelconfig[channel].report_exp == 3)
            return ("kW");
        return ("-");
    case AC_REACTIVE_POWER:
        if (channelconfig[channel].report_exp == -3)
            return ("mVAr");
        else if (channelconfig[channel].report_exp == 0)
            return ("VAr");
        else if (channelconfig[channel].report_exp == 3)
            return ("kVAr");
        return ("-");
    default:
        return ("-");
    }
}

bool measure_get_measurement_valid(void)
{
    if (measurement_valid == 0)
        return (true);
    return (false);
}

void measure_set_measurement_invalid(int sec)
{
    measurement_valid = sec;
}

uint8_t *measure_get_channel_opcodeseq(uint16_t channel)
{
    if (channel >= VIRTUAL_CHANNELS)
        return (NULL);
    return (channelconfig[channel].operation);
}

char *measure_get_channel_opcodeseq_str(uint16_t channel, uint16_t len, char *dest)
{
    char microcode_tmp[3] = "";
    uint8_t *opcode = channelconfig[channel].operation;
    *dest = '\0';

    if (channel >= VIRTUAL_CHANNELS)
        return (NULL);

    for (int a = 0; a < MAX_MICROCODE_OPS; a++)
    {
        snprintf(microcode_tmp, sizeof(microcode_tmp), "%02x", *opcode);
        strncat(dest, microcode_tmp, len);
        opcode++;
    }
    return (dest);
}

void measure_save_settings(void)
{
    measure_config.save();
}

void measure_set_channel_opcodeseq(uint16_t channel, uint8_t *value)
{
    char microcode[VIRTUAL_CHANNELS * 3] = "";
    char microcode_tmp[3] = "";
    uint8_t *opcode = value;

    if (channel >= VIRTUAL_CHANNELS)
        return;

    if (channel >= CHANNEL_END)
        return;

    memcpy(channelconfig[channel].operation, value, MAX_MICROCODE_OPS);

    for (int a = 0; a < MAX_MICROCODE_OPS; a++)
    {
        snprintf(microcode_tmp, sizeof(microcode_tmp), "%02x", *opcode);
        strncat(microcode, microcode_tmp, sizeof(microcode));
        opcode++;
    }
}
// esta función analiza los opcode escritos y lo guarda en la variable opcode
void measure_set_channel_opcodeseq_str(uint16_t channel, const char *value)
{
    char spanset[] = "0123456789ABCDEFabcdef";
    char *ptr = (char *)value;
    uint8_t opcode_binary = 0;
    int opcode_pos = 0;

    if (channel >= VIRTUAL_CHANNELS)
        return;

    if (channel >= CHANNEL_END)
        return;
    /**
     * check string for illigal format
     */
    while (*ptr)
    {
        /**
         * get first nibble of opcode
         */
        ptr = strpbrk(ptr, spanset);
        if (!ptr)
        {
            log_e("abort, wrong format");
            break;
        }
        opcode_binary = ((*ptr <= '9') ? *ptr - '0' : (*ptr & 0x7) + 9) << 4;
        ptr++;
        /**
         * get 2'nd nibble of opcode
         */
        ptr = strpbrk(ptr, spanset);
        if (!ptr)
        {
            log_e("abort, wrong format");
            break;
        }
        opcode_binary = opcode_binary + ((*ptr <= '9') ? *ptr - '0' : (*ptr & 0x7) + 9);
        ptr++;
        /**
         * check if we have space for next opcode
         */
        if (opcode_pos < MAX_MICROCODE_OPS)
        {
            channelconfig[channel].operation[opcode_pos] = opcode_binary;
            opcode_pos++;
        }
        else
        {
            log_e("no more space for opcodes");
            break;
        }
    }
    return;
}
