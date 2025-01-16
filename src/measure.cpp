
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
    {"L1 Corriente", AC_CURRENT, 0, 0.084, 0.0, 0.0, false, 0.0, 0, 0, 0.0, GET_ADC | CHANNEL_0, FILTER | 0, BRK, BRK, BRK, BRK, BRK, BRK, BRK, BRK},
    {"L1 Tensión", AC_VOLTAGE, 30, 1.3, 0.0, 0.0, false, 0.0, 0, 0, 0.0, GET_ADC | CHANNEL_1, FILTER | 0, BRK, BRK, BRK, BRK, BRK, BRK, BRK, BRK},
    {"L1 Potencia", AC_POWER, 0, 1.0, 0.0, 0.0, false, 0.0, 3, 0, 0.0, SET_TO | 1, MUL | CHANNEL_0, MUL | CHANNEL_1, MUL_RATIO | CHANNEL_0, MUL_RATIO | CHANNEL_1, ABS, BRK, BRK, BRK, BRK},
    {"L1 Potencia Reactiva", AC_REACTIVE_POWER, 0, 1.0, 0.0, 0.0, false, 0.0, 3, 0, 0.0, SET_TO | 1, MUL | CHANNEL_0, MUL | CHANNEL_1, MUL_RATIO | CHANNEL_0, MUL_RATIO | CHANNEL_1, PASS_NEGATIVE, MUL_REACTIVE | CHANNEL_1, ABS, NEG, BRK},
    {"L2 Corriente", AC_CURRENT, 0, 0.115, 0.0, 0.0, false, 0.0, 0, 1, 0.0, GET_ADC | CHANNEL_5, FILTER | 0, BRK, BRK, BRK, BRK, BRK, BRK, BRK, BRK},
    {"L2 Tensión", AC_VOLTAGE, 30, 0.63, 0.0, 0.0, false, 0.0, 0, 1, 0.0, GET_ADC | CHANNEL_4, FILTER | 0, BRK, BRK, BRK, BRK, BRK, BRK, BRK, BRK},
    {"L2 Potencia", AC_POWER, 0, 1.0, 0.0, 0.0, false, 0.0, 3, 1, 0.0, SET_TO | 1, MUL | CHANNEL_5, MUL | CHANNEL_4, MUL_RATIO | CHANNEL_5, MUL_RATIO | CHANNEL_4, ABS, BRK, BRK, BRK, BRK},
    {"L2 Potencia Reactiva", AC_REACTIVE_POWER, 0, 1.0, 0.0, 0.0, false, 0.0, 3, 1, 0.0, SET_TO | 1, MUL | CHANNEL_5, MUL | CHANNEL_4, MUL_RATIO | CHANNEL_5, MUL_RATIO | CHANNEL_4, PASS_NEGATIVE, MUL_REACTIVE | CHANNEL_4, ABS, NEG, BRK},
    {"L3 Corriente", AC_CURRENT, 0, 0.082, 0.0, 0.0, false, 0.0, 0, 2, 0.0, GET_ADC | CHANNEL_2, FILTER | 0, BRK, BRK, BRK, BRK, BRK, BRK, BRK, BRK},
    {"L3 Tensión", AC_VOLTAGE, 30, 0.63, 0.0, 0.0, false, 0.0, 0, 2, 0.0, GET_ADC | CHANNEL_3, FILTER | 0, BRK, BRK, BRK, BRK, BRK, BRK, BRK, BRK},
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
                                                              toma una muestra cada 4 muestras verdaderas, es decir que se está muestreando cada 4 muestras*/
double HerzvImag[numbersOfSamples * 4];                     // lo mismo pasa con la variable de frecuencia imaginaria
double netfrequency_phaseshift, netfrequency_oldphaseshift; // estas variables definen el desfaje de la frecuencia de linea, la actual y la anterior
double netfrequency;                                        // esta variable guarda el valor de la frecuencia de línea
double vReal[numbersOfSamples]; // Buffer para los valores reales
double vImag[numbersOfSamples]; // Buffer para los valores imaginarios (en FFT se inicializan a cero)
arduinoFFT FFT2 = arduinoFFT();
arduinoFFT FFT = arduinoFFT();
static int measurement_valid = 3; /** empieza a contar en 3 para evitar basura */

const float high_pass_coef = 0.9989;  // Coeficiente de filtro de paso alto, ajustado para reducir el DC residual
const int low_pass_window_size = 12;  // Tamaño de la ventana del filtro de media móvil (pasa bajo)

/**
 * @brief Tarea principal para gestionar las mediciones
 * 
 * @param pvParameters Parámetros de entrada para la tarea (no utilizados en este caso)
 */
void measure_Task(void *pvParameters)
{
    // Muestra en el log el núcleo del ESP32 donde se está ejecutando esta tarea
    log_i("Iniciando tarea de medición en núcleo: %d", xPortGetCoreID());
    
    // Configuración inicial de los sistemas de medición (ADC, I2S, buffers, etc.)
    measure_init();
    
    // Bucle infinito para realizar mediciones continuamente
    while (true)
    {
        // Realiza una medición, incluyendo adquisición de datos, procesamiento y cálculo de parámetros
        measure_mes();
    }
}


/**
 * @brief Crea e inicia la tarea de medición en un núcleo específico del ESP32
 */
void measure_StartTask(void)
{
    xTaskCreatePinnedToCore(
        measure_Task,               // Función que implementa la lógica de la tarea
        "measure measurement Task", // Nombre descriptivo de la tarea (para depuración)
        10000,                      // Tamaño de la pila asignada a la tarea (en palabras)
        NULL,                       // Parámetro de entrada para la tarea (no se utiliza en este caso)
        2,                          // Prioridad de la tarea (a mayor valor, mayor prioridad)
        &_MEASURE_Task,             // Puntero al manejador de la tarea, para control y monitoreo
        _MEASURE_TASKCORE           // Núcleo en el que se ejecutará la tarea (definido como 1)
    );
}

/**
 * @brief Configura el sistema de medición inicializando el ADC, I2S y otros parámetros necesarios.
 */
void measure_init(void)
{
    // Carga la configuración desde un archivo JSON almacenado en SPIFFS
    measure_config.load();
    // Asigna la frecuencia de la red (ej. 50 Hz o 60 Hz) desde la configuración cargada
    netfrequency = measure_config.network_frequency;

    // Calcula la tasa de muestreo del ADC basada en la frecuencia de red y una corrección adicional
    int sample_rate = (samplingFrequency * measure_config.network_frequency / 2) + measure_config.samplerate_corr;
    // Ejemplo de cálculo: 1536 Hz * 50 / 2 + 0 = 38400 Hz

    // Configuración del bus I2S para lectura de datos desde el ADC interno
    const i2s_config_t i2s_config = {
        .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN), // Modo maestro, lectura desde el ADC integrado
        .sample_rate = sample_rate,                   // Frecuencia de muestreo calculada
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, // Resolución de 16 bits por muestra
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT, // Usa solo un canal (derecho)
        .communication_format = I2S_COMM_FORMAT_I2S_MSB, // Formato de comunicación I2S (MSB primero)
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,     // Interrupción de nivel 1
        .dma_buf_count = VIRTUAL_ADC_CHANNELS * 4,    // Tamaño del buffer DMA (24 bloques)
        .dma_buf_len = numbersOfSamples,             // Longitud del buffer (256 muestras por bloque)
        .use_apll = false,                           // No usa el PLL auxiliar
        .tx_desc_auto_clear = true,                  // Limpia automáticamente las descripciones de transmisión
        .fixed_mclk = 0                              // Sin reloj maestro fijo
    };

    // Instala el controlador I2S con la configuración anterior
    esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK)
    {
        log_e("Error al instalar el driver I2S: %d", err);
        while (true); // Si ocurre un error crítico, el sistema se detiene
    }

    // Configura el modo del ADC interno, seleccionando la unidad ADC1 y el canal 6
    err = i2s_set_adc_mode(ADC_UNIT_1, (adc1_channel_t)6);
    if (err != ESP_OK)
    {
        log_e("Error al configurar el modo ADC: %d", err);
        while (true); // Detiene el sistema si ocurre un error crítico
    }

    // Detiene temporalmente la comunicación I2S mientras se realizan otras configuraciones
    i2s_stop(I2S_PORT);
    vTaskDelay(100); // Pausa de 100 ms para asegurar la estabilidad del sistema

    // Configuración de los registros del ADC SAR para gestionar los canales virtuales
    SYSCON.saradc_ctrl.sar1_patt_len = VIRTUAL_ADC_CHANNELS - 1; // Número de canales - 1
    SYSCON.saradc_sar1_patt_tab[0] = 0x0f3f4f5f; // Configuración de patrones para los primeros canales
    SYSCON.saradc_sar1_patt_tab[1] = 0x6f7f0000; // Configuración de patrones para los siguientes canales
    SYSCON.saradc_ctrl.sar_clk_div = 6; // Divisor de reloj del ADC

    // Mensaje en el log indicando que el controlador I2S está listo
    log_i("Medición: Controlador I2S listo");

    // Inicia la comunicación I2S para comenzar a recibir datos
    i2s_start(I2S_PORT);
}

/**
 * @brief Realiza una medición y procesa los datos de los canales.
 */
void measure_mes(void)
{
    int round = 0; // Contador cíclico para realizar múltiples rondas de muestreo dentro de un periodo fijo.

    // Inicializa las sumas de cada canal virtual antes de comenzar el procesamiento.
    for (int i = 0; i < VIRTUAL_CHANNELS; i++)
    {
        channelconfig[i].sum = 0.0;
    }

    // Define el tiempo límite de ejecución para la medición (1 segundo desde el momento actual).
    uint64_t NextMillis = millis() + 1000l;
    while (millis() < NextMillis)
    {
        // Buffers estáticos para almacenar datos temporales durante el procesamiento.
        static float adc_sample[VIRTUAL_CHANNELS], last_adc_sample[VIRTUAL_CHANNELS];
        // adc_sample: Almacena las muestras actuales de los canales virtuales.
        // last_adc_sample: Guarda las muestras anteriores para aplicar filtros y cálculos diferenciales.

        static float temp_adc_sample[VIRTUAL_CHANNELS];
        // temp_adc_sample: Variable temporal para cálculos intermedios con las muestras de los canales.

        static float ac_filtered[VIRTUAL_CHANNELS], last_ac_filtered[VIRTUAL_CHANNELS];
        // ac_filtered: Almacena las señales de corriente alterna (AC) después de aplicar un filtro de paso alto.
        // last_ac_filtered: Guarda los valores filtrados anteriores, necesarios para cálculos iterativos.

        static float dc_filtered[VIRTUAL_CHANNELS][64];
        // dc_filtered: Matriz que almacena las señales de corriente continua (DC) después de aplicar un filtro de paso bajo.
        // Cada canal tiene un buffer circular de 64 muestras para realizar un filtrado promedio.

        uint16_t *channel[VIRTUAL_ADC_CHANNELS];
        // channel: Arreglo de punteros que apunta a los datos de cada canal virtual para acceso rápido.

        uint16_t adc_tempsamples[numbersOfSamples];
        // adc_tempsamples: Buffer temporal para almacenar las muestras crudas leídas desde el ADC.

        uint16_t adc_samples[VIRTUAL_ADC_CHANNELS][numbersOfSamples];
        // adc_samples: Matriz que organiza las muestras del ADC por canal virtual.

        int phaseshift_0_degree;
        // phaseshift_0_degree: Desfase de 0 grados, calculado en función de las configuraciones del canal.

        int phaseshift_90_degree;
        // phaseshift_90_degree: Desfase de 90 grados, usado para determinar si una carga es capacitiva o inductiva.

        static float rms_history[16] = {0}; // Buffer circular para almacenar valores RMS.
        static int rms_index = 0;           // Índice actual del buffer.

        // Inicializa los punteros para cada canal virtual, apuntando al inicio del buffer correspondiente.
        for (int i = 0; i < VIRTUAL_ADC_CHANNELS; i++) 
        {
            channel[i] = &adc_samples[i][0];
        }

        // Bucle para leer y procesar bloques de datos desde el ADC a través de I2S.
        for (int adc_chunck = 0; adc_chunck < VIRTUAL_ADC_CHANNELS; adc_chunck++)
        {
            size_t num_bytes_read = 0; // Almacena la cantidad de bytes leídos.
            esp_err_t err;

            // Lee datos del ADC utilizando el driver I2S.
            err = i2s_read(
                I2S_PORT,                     // Puerto I2S configurado previamente.
                (char *)adc_tempsamples,     // Buffer temporal para almacenar las muestras crudas.
                sizeof(adc_tempsamples),     // Tamaño del buffer en bytes.
                &num_bytes_read,             // Puntero para almacenar el número de bytes leídos.
                100                          // Tiempo de espera (100 ms).
            );

            // Verifica si la lectura fue exitosa.
            if (err != ESP_OK)
            {
                log_e("Error al leer el buffer DMA: %d", err); // Muestra un mensaje de error en el log.
                while (true); // Detiene el sistema si ocurre un error crítico.
            }

            // Ajusta el número de muestras leídas dividiendo el tamaño en bytes entre 2 (tamaño de muestra en 16 bits).
            num_bytes_read /= 2;

            // Verifica si el número de muestras leídas coincide con lo esperado.
            if (num_bytes_read != numbersOfSamples)
            {
                log_e("El tamaño del bloque no coincide con el número de muestras: %d", num_bytes_read);
                while (true);
            }

            // Procesa cada muestra leída y la organiza en canales virtuales.
            for (int i = 0; i < num_bytes_read; i++)
            {
                // Extrae el canal del ADC (bits 15 a 12) de la palabra de 16 bits.
                int8_t chan = (adc_tempsamples[i] >> 12) & 0xf;

                // Verifica si el canal está mapeado y asigna la muestra al canal virtual correspondiente.
                if (chan < MAX_ADC_CHANNELS && channelmapping[chan] != CHANNEL_NOP)
                {
                    *channel[channelmapping[chan]] = adc_tempsamples[i] & 0x0fff; // Extrae los 12 bits de la muestra.
                    channel[channelmapping[chan]]++; // Avanza al siguiente espacio del buffer del canal.
                }
            }
        }

        // Recorre todas las muestras de cada canal virtual para procesarlas.
        for (int n = 0; n < numbersOfSamples; n++)
        {
            for (int i = 0; i < VIRTUAL_CHANNELS; i++)
            {
                // Verifica si el canal está activo y configurado correctamente.
                if (!groupconfig[channelconfig[i].group_id].active || channelconfig[i].type == NO_CHANNEL_TYPE)
                {
                    // Si el canal no está activo o no tiene un tipo definido, asigna valores neutros.
                    buffer[i][n] = 2048;        // Valor neutral en el buffer.
                    channelconfig[i].sum = 0.0; // Reinicia la suma acumulada del canal.
                    adc_sample[i] = 0.0;        // Limpia el valor actual de la muestra.
                    continue;                   // Pasa al siguiente canal.
                }

                // Procesa las operaciones configuradas en el canal.
                for (int operation = 0; operation < MAX_MICROCODE_OPS; operation++)
                {
                    // Calcula el desfase en grados para 0° y 90°.
                    phaseshift_0_degree = calculate_phaseshift(channelconfig[i].phaseshift, n, numbersOfSamples);

                    if (phaseshift_0_degree == numbersOfSamples - 1)
                        phaseshift_0_degree = 0;

                    phaseshift_90_degree = ((numbersOfSamples / 2) / 360.0) * ((channelconfig[i].phaseshift + 90) % 360);
                    phaseshift_90_degree = (n + phaseshift_90_degree) % numbersOfSamples;
                    if (phaseshift_90_degree == numbersOfSamples - 1)
                        phaseshift_90_degree = 0;

                    // Obtiene el canal de operación y verifica que esté dentro del rango permitido.
                    int op_channel = channelconfig[i].operation[operation] & ~OPMASK;
                    if (op_channel >= VIRTUAL_CHANNELS)
                        continue;

                    // Ejecuta la operación correspondiente en el canal.
                    switch (channelconfig[i].operation[operation] & OPMASK)
                    {
                    case ADD:
                        adc_sample[i] += adc_sample[op_channel];
                        break;
                    case SUB:
                        adc_sample[i] -= adc_sample[op_channel];
                        break;
                    case MUL:
                        adc_sample[i] *= adc_sample[op_channel];
                        break;
                    case MUL_RATIO:
                        adc_sample[i] *= channelconfig[op_channel].ratio;
                        break;
                    case MUL_SIGN:
                        adc_sample[i] *= (adc_sample[op_channel] > 0.0) ? 1.0 : -1.0;
                        break;
                    case MUL_REACTIVE:
                        // Multiplica por el signo reactivo basado en el desfase de 90°.
                        temp_adc_sample[i] = buffer[op_channel][phaseshift_90_degree] - 2048.0;
                        channelconfig[i].sign = (temp_adc_sample[i] > 0.0) ? 1.0 : -1.0;
                        adc_sample[i] *= channelconfig[i].sign;
                        break;
                    case ABS:
                        adc_sample[i] = fabs(adc_sample[i]);
                        break;
                    case NEG:
                        adc_sample[i] = -adc_sample[i];
                        break;
                    case PASS_NEGATIVE:
                        if (adc_sample[i] > 0.0)
                            adc_sample[i] = 0.0;
                        break;
                    case PASS_POSITIVE:
                        if (adc_sample[i] < 0.0)
                            adc_sample[i] = 0.0;
                        break;
                    case GET_ADC:
                        if (op_channel >= 0 && op_channel < VIRTUAL_ADC_CHANNELS)
                        {
                            adc_sample[i] = adc_samples[op_channel][phaseshift_0_degree];
                        }
                        break;
                    case SET_TO:
                        adc_sample[i] = op_channel;
                        break;
                    case FILTER:
                        // Aplica un filtro de paso alto para eliminar la componente DC.
                        ac_filtered[i] = HIGH_PASS_FILTER(last_adc_sample[i], last_ac_filtered[i], adc_sample[i]);
                        last_ac_filtered[i] = ac_filtered[i];
                        last_adc_sample[i] = adc_sample[i];
                        adc_sample[i] = ac_filtered[i];

                        // Aplica un filtro de paso bajo si corresponde.
                        if (op_channel)
                        {
                            int mul = (op_channel <= 6) ? (1 << op_channel) : low_pass_window_size;
                            dc_filtered[i][n % mul] = adc_sample[i];
                            float sum = 0.0;
                            for (int j = 0; j < mul; j++)
                            {
                                sum += dc_filtered[i][j];
                            }
                            adc_sample[i] = sum / mul; // Señal suavizada.
                        }
                        break;
                    case NOP:
                        break; // No hacer nada.
                    default:
                        operation = MAX_MICROCODE_OPS; // Salir del bucle si la operación no es válida.
                        break;
                    }
                }

                // Almacena las muestras procesadas en el buffer.
                if (channelconfig[i].type == AC_VOLTAGE && measure_get_channel_ratio(i) > 5)
                {
                    buffer[i][n] = 2048; // Saturación si el ratio es mayor a 5.
                }
                else
                {
                    buffer[i][n] = (adc_sample[i] + 2048 < 0.0) ? 0 : (adc_sample[i] * measure_get_channel_ratio(i)) + 2048;
                }

                // Suma los valores de la señal procesada para cálculos RMS o promedios.
                switch (channelconfig[i].type)
                {
                case AC_CURRENT:
                case AC_VOLTAGE:
                    if (channelconfig[i].true_rms)
                    {
                        channelconfig[i].sum += adc_sample[i] * adc_sample[i]; // Cuadrado para RMS.
                    }
                    else
                    {
                        channelconfig[i].sum += fabs(adc_sample[i]); // Módulo de la señal.
                    }
                    break;
                case AC_POWER:
                case AC_REACTIVE_POWER:
                case DC_CURRENT:
                case DC_VOLTAGE:
                case DC_POWER:
                    channelconfig[i].sum += (channelconfig[i].true_rms) ? (adc_sample[i] * adc_sample[i]) : adc_sample[i];
                    break;
                case NO_CHANNEL_TYPE:
                    break;
                }
            }
        }

        // Promedio acumulativo de RMS.
        float rms_sum = 0;

        // Actualiza el buffer circular con el RMS calculado de AC_VOLTAGE.
        for (int i = 0; i < VIRTUAL_CHANNELS; i++)
        {
            if (channelconfig[i].type == AC_VOLTAGE)
            {
                rms_history[rms_index] = channelconfig[i].rms; // Almacena el valor RMS en el buffer.
                break;
            }
        }

        // Incrementa el índice del buffer circular.
        rms_index = (rms_index + 1) % 16;

        // Calcula el promedio RMS acumulado.
        for (int i = 0; i < 16; i++)
        {
            rms_sum += rms_history[i];
        }
        float rms_avg = rms_sum / 16; // Promedio de los últimos 16 ciclos.

        if (rms_avg >= 40)
        {
            // Si el promedio supera el umbral, copia y transmite los datos.
            if (TX_buffer != -1)
            {
                memcpy(&buffer_probe[0][0], &buffer[0][0], sizeof(buffer));
                TX_buffer = -1; // Marca el buffer como procesado.
            }
        }
        else
        {
            // Si el promedio no supera el umbral, no se envía nada.
            if (TX_buffer != -1)
            {
                memset(&buffer_probe[0][0], 0, sizeof(buffer_probe)); // Limpia el buffer para evitar ruido.
                TX_buffer = -1;                                       // Marca el buffer como procesado.
            }
        }



               // Captura muestras para calcular la FFT durante las primeras 4 rondas.
        if (round < 4)
        {
            // Busca el primer canal configurado como AC_VOLTAGE.
            for (int i = 0; i < VIRTUAL_CHANNELS; i++)
            {
                // Si el canal es de tensión alterna (AC), copia las muestras al buffer de FFT.
                if (channelconfig[i].type == AC_VOLTAGE)
                {
                    for (int sample = 0; sample < numbersOfSamples; sample++)
                    {
                        // Guarda las muestras reales y inicializa las imaginarias a cero.
                        HerzvReal[(round * numbersOfSamples) + sample] = buffer[i][sample];
                        HerzvImag[(round * numbersOfSamples) + sample] = 0;
                    }
                    break; // Termina la búsqueda después de encontrar el primer canal válido.
                }
            }
        }


        round++;
    }
        // Verifica si el canal 1 es AC_VOLTAGE y su ratio es válido para el análisis.
    if (channelconfig[1].type == AC_VOLTAGE && measure_get_channel_ratio(1) <= 5.0)
    {
        // Configura la FFT con los datos reales, imaginarios y parámetros de la señal.
        FFT = arduinoFFT(HerzvReal, HerzvImag, numbersOfSamples * 4, (numbersOfSamples)*measure_get_network_frequency());

        // Aplica una ventana Hamming para minimizar efectos de discontinuidad en los bordes.
        for (int i = 0; i < numbersOfSamples * 4; i++)
        {
            HerzvReal[i] *= 0.54 - 0.46 * cos(2 * PI * i / (numbersOfSamples * 4));
        }

        // Realiza el cálculo de la FFT.
        FFT.Compute(FFT_REVERSE);

        // Calcula el desplazamiento de fase actual en grados.
        netfrequency_oldphaseshift = netfrequency_phaseshift;
        netfrequency_phaseshift = atan2(HerzvReal[8], HerzvImag[8]) * (180.0 / PI) + 180;

        // Verifica si el cambio de fase es válido para cálculos de frecuencia.
        if ((netfrequency_phaseshift - netfrequency_oldphaseshift) < 180 &&
            (netfrequency_phaseshift - netfrequency_oldphaseshift) > -180)
        {
            static float netfrequency_filter[16] = {0.0};    // Buffer para suavizar la frecuencia.
            static int index = 0;                     // Índice del filtro circular.
            static bool netfrequency_firstrun = true; // Bandera para inicialización del filtro.

            // Inicializa el filtro con la frecuencia de la red en la primera ejecución.
            if (netfrequency_firstrun)
            {
                for (int i = 0; i < 16; i++)
                {
                    netfrequency_filter[i] = measure_get_network_frequency();
                }
                netfrequency_firstrun = false;
            }

            // Calcula la frecuencia ajustada basada en el desplazamiento de fase.
            netfrequency_filter[index] = (netfrequency_phaseshift - netfrequency_oldphaseshift) * ((1.0f / PI) / 90) +
                                         measure_get_network_frequency();

            // Calcula el promedio de las últimas 16 frecuencias para estabilizar el valor.
            netfrequency = 0.0;
            for (int i = 0; i < 16; i++)
            {
                netfrequency += netfrequency_filter[i];
            }
            netfrequency /= 16.0;

            // Actualiza el índice del filtro para la próxima iteración.
            index = (index + 1) % 16;
        }
    }

            else
        {
            // Si el cambio de fase está fuera del rango permitido (±180°),
            // utiliza la frecuencia de red configurada como valor predeterminado.
            netfrequency = measure_get_network_frequency();
        }

    
// Bandera para determinar si la señal AC_VOLTAGE es válida.
bool ac_voltage_valid = false;

// Recorre todos los canales para procesar el RMS.
for (int i = 0; i < VIRTUAL_CHANNELS; i++)
{
    // Calcula el RMS base, evita divisiones por 0.
    float rms_calc = (round > 0) ? sqrt(channelconfig[i].sum / (numbersOfSamples * round)) : 0.0;

    // Procesa según el tipo de canal.
    switch (channelconfig[i].type)
    {
    case DC_CURRENT:
        if (channelconfig[i].ratio > 5)
        {
            channelconfig[i].rms = channelconfig[i].ratio + channelconfig[i].offset;
        }
        else
        {
            if (channelconfig[i].true_rms)
            {
                channelconfig[i].rms = (channelconfig[i].ratio * rms_calc) + channelconfig[i].offset;
            }
            else
            {
                channelconfig[i].rms = (channelconfig[i].ratio * (channelconfig[i].sum / (numbersOfSamples * round))) + channelconfig[i].offset;
            }
        }
        break;

    case AC_CURRENT:
        if (channelconfig[i].ratio > 5)
        {
            channelconfig[i].rms = channelconfig[i].ratio + channelconfig[i].offset;
        }
        else
        {
            if (channelconfig[i].true_rms)
            {
                channelconfig[i].rms = (channelconfig[i].ratio * rms_calc > 0)
                    ? (channelconfig[i].ratio * rms_calc) + channelconfig[i].offset
                    : 0;
            }
            else
            {
                channelconfig[i].rms = (channelconfig[i].ratio * (channelconfig[i].sum / (numbersOfSamples * round)) > 0.05)
                    ? (channelconfig[i].ratio * (channelconfig[i].sum / (numbersOfSamples * round))) + channelconfig[i].offset
                    : 0;
            }
        }
        break;

    case DC_POWER:
    case AC_POWER:
    case AC_REACTIVE_POWER:
        if (channelconfig[i].ratio > 5)
        {
            channelconfig[i].rms = channelconfig[i].ratio + channelconfig[i].offset;
        }
        else
        {
            if (channelconfig[i].true_rms)
            {
                channelconfig[i].rms = (channelconfig[i].ratio * rms_calc > 0)
                    ? (channelconfig[i].ratio * rms_calc) + channelconfig[i].offset
                    : 0;
            }
            else
            {
                channelconfig[i].rms = (channelconfig[i].ratio * (channelconfig[i].sum / (numbersOfSamples * round)) > 0.1)
                    ? (channelconfig[i].ratio * (channelconfig[i].sum / (numbersOfSamples * round))) + channelconfig[i].offset
                    : 0;
            }
        }
        break;

    case AC_VOLTAGE:
    case DC_VOLTAGE:
        if (channelconfig[i].ratio > 5)
        {
            channelconfig[i].rms = channelconfig[i].ratio + channelconfig[i].offset;
        }
        else
        {
            if (channelconfig[i].true_rms)
            {
                channelconfig[i].rms = (channelconfig[i].ratio * rms_calc > 0)
                    ? (channelconfig[i].ratio * rms_calc) + channelconfig[i].offset
                    : 0;
            }
            else
            {
                channelconfig[i].rms = (channelconfig[i].ratio * (channelconfig[i].sum / (numbersOfSamples * round)) > 40)
                    ? (channelconfig[i].ratio * (channelconfig[i].sum / (numbersOfSamples * round))) + channelconfig[i].offset
                    : 0;
            }
        }

        // Actualiza la bandera si la señal de AC_VOLTAGE es válida.
        if (channelconfig[i].rms >= 40)
        {
            ac_voltage_valid = true;
        }
        break;

    case NO_CHANNEL_TYPE:
        // No se realiza ninguna acción si el canal no tiene un tipo definido.
        break;
    }


// Si la señal de AC_VOLTAGE no es válida, fuerza todos los canales a 0.
if (!ac_voltage_valid)
{
    for (int i = 0; i < VIRTUAL_CHANNELS; i++)
    {
        channelconfig[i].rms = 0.0;
    }
}


        // Si la medición no es válida, reinicia el RMS del canal a 0.
        if (measurement_valid > 0)
        {
            channelconfig[i].rms = 0.0;
            measurement_valid--;
        }
    }
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
        esp_restart(); // Reinicia el sistema de manera controlada.

        //while (true);
    }
}

// Función auxiliar para calcular desfases
int calculate_phaseshift(int base_shift, int n, int samples) {
    int shift = ((samples / 2) / 360.0) * (base_shift % 360);
    return (n + shift) % samples;
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

uint16_t *measure_get_fft(void)
{
    double vReal[numbersOfFFTSamples * 2];
    double vImag[numbersOfFFTSamples * 2];
    double temp = 0;
    arduinoFFT FFT = arduinoFFT(vReal, vImag, numbersOfFFTSamples * 2, samplingFrequency * measure_get_network_frequency() / 4);

    for (int channel = 0; channel < VIRTUAL_CHANNELS; channel++){
    
        for (int i = 0; i < numbersOfFFTSamples * 2; i++)
        {
            vReal[i] = measure_get_channel_ratio(channel)*buffer_probe[channel][(i * 6) % numbersOfSamples];
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
        }}
        return (&buffer_fft[0][0]);
    
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
