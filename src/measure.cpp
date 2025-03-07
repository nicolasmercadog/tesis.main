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

#define MONITOR_CHANNEL 0

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
// Estructura para conexión global de armónicos
HarmonicData harmonic_values[VIRTUAL_CHANNELS]; // Definición del array global

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
    {"L1 Corriente", AC_CURRENT, 0, 0.1, 0.0, 0.0, false, 0.0, 0, 0, 0.0, GET_ADC | CHANNEL_0, FILTER | 0, BRK, BRK, BRK, BRK, BRK, BRK, BRK, BRK},
    {"L1 Tensión", AC_VOLTAGE, 30, 1.0, 0.0, 0.0, false, 0.0, 0, 0, 0.0, GET_ADC | CHANNEL_1, FILTER | 2, BRK, BRK, BRK, BRK, BRK, BRK, BRK, BRK},
    {"L1 Potencia", AC_POWER, 0, 0.1, 0.0, 0.0, false, 0.0, 3, 0, 0.0, SET_TO | 1, MUL | CHANNEL_0, MUL | CHANNEL_1, MUL_RATIO | CHANNEL_0, MUL_RATIO | CHANNEL_1, ABS, BRK, BRK, BRK, BRK},
    {"L1 Potencia Reactiva", AC_REACTIVE_POWER, 0, 0.1, 0.0, 0.0, false, 0.0, 3, 0, 0.0, SET_TO | 1, MUL | CHANNEL_0, MUL | CHANNEL_1, MUL_RATIO | CHANNEL_0, MUL_RATIO | CHANNEL_1, PASS_NEGATIVE, MUL_REACTIVE | CHANNEL_1, ABS, NEG, BRK},
    {"L2 Corriente", AC_CURRENT, 0, 0.1, 0.0, 0.0, false, 0.0, 0, 1, 0.0, GET_ADC | CHANNEL_5, FILTER | 0, BRK, BRK, BRK, BRK, BRK, BRK, BRK, BRK},
    {"L2 Tensión", AC_VOLTAGE, 30, 1.0, 0.0, 0.0, false, 0.0, 0, 1, 0.0, GET_ADC | CHANNEL_4, FILTER | 2, BRK, BRK, BRK, BRK, BRK, BRK, BRK, BRK},
    {"L2 Potencia", AC_POWER, 0, 0.1, 0.0, 0.0, false, 0.0, 3, 1, 0.0, SET_TO | 1, MUL | CHANNEL_5, MUL | CHANNEL_4, MUL_RATIO | CHANNEL_5, MUL_RATIO | CHANNEL_4, ABS, BRK, BRK, BRK, BRK},
    {"L2 Potencia Reactiva", AC_REACTIVE_POWER, 0, 0.1, 0.0, 0.0, false, 0.0, 3, 1, 0.0, SET_TO | 1, MUL | CHANNEL_5, MUL | CHANNEL_4, MUL_RATIO | CHANNEL_5, MUL_RATIO | CHANNEL_4, PASS_NEGATIVE, MUL_REACTIVE | CHANNEL_4, ABS, NEG, BRK},
    {"L3 Corriente", AC_CURRENT, 0, 0.1, 0.0, 0.0, false, 0.0, 0, 2, 0.0, GET_ADC | CHANNEL_2, FILTER | 0, BRK, BRK, BRK, BRK, BRK, BRK, BRK, BRK},
    {"L3 Tensión", AC_VOLTAGE, 30, 1.0, 0.0, 0.0, false, 0.0, 0, 2, 0.0, GET_ADC | CHANNEL_3, FILTER | 2, BRK, BRK, BRK, BRK, BRK, BRK, BRK, BRK},
    {"L3 Potencia", AC_POWER, 0, 0.1, 0.0, 0.0, false, 0.0, 3, 2, 0.0, SET_TO | 1, MUL | CHANNEL_2, MUL | CHANNEL_3, MUL_RATIO | CHANNEL_2, MUL_RATIO | CHANNEL_3, ABS, BRK, BRK, BRK, BRK},
    {"L3 Potencia Reactiva", AC_REACTIVE_POWER, 0, 0.1, 0.0, 0.0, false, 0.0, 3, 2, 0.0, SET_TO | 1, MUL | CHANNEL_2, MUL | CHANNEL_3, MUL_RATIO | CHANNEL_2, MUL_RATIO | CHANNEL_3, PASS_NEGATIVE, MUL_REACTIVE | CHANNEL_3, ABS, NEG, BRK},
    {"Todas las Potencias", AC_POWER, 0, 0.1, 0.0, 0.0, false, 0.0, 3, 3, 0.0, SET_TO | 0, ADD | CHANNEL_0, ADD | CHANNEL_4, ADD | CHANNEL_2, BRK, BRK, BRK, BRK, BRK, BRK}

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
double vReal[numbersOfSamples];                             // Buffer para los valores reales
double vImag[numbersOfSamples];                             // Buffer para los valores imaginarios (en FFT se inicializan a cero)
arduinoFFT FFT2 = arduinoFFT();
arduinoFFT FFT = arduinoFFT();
static int measurement_valid = 3; /** empieza a contar en 3 para evitar basura */

const float high_pass_coef = 0.98;   // Menos agresivo que tu implementación actual
const int low_pass_window_size = 12; // Valor original

float centered_sample = 0.0;
float centered_last_sample = 0.0;
static float dc_offset[VIRTUAL_CHANNELS] = {0};
const float noise_threshold = 2.0; // Ajusta según tus necesidades
const float dead_band = 3.0;       // Ajusta según el nivel de ruido de fondo
static float signal_energy[VIRTUAL_CHANNELS] = {0};
const float energy_threshold = 5.0;
float threshold_value;

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
        .sample_rate = sample_rate,                                                // Frecuencia de muestreo calculada
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,                              // Resolución de 16 bits por muestra
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,                              // Usa solo un canal (derecho)
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,                           // Formato de comunicación I2S (MSB primero)
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,                                  // Interrupción de nivel 1
        .dma_buf_count = VIRTUAL_ADC_CHANNELS * 4,                                 // Tamaño del buffer DMA (24 bloques)
        .dma_buf_len = numbersOfSamples,                                           // Longitud del buffer (256 muestras por bloque)
        .use_apll = false,                                                         // No usa el PLL auxiliar
        .tx_desc_auto_clear = true,                                                // Limpia automáticamente las descripciones de transmisión
        .fixed_mclk = 0                                                            // Sin reloj maestro fijo
    };

    // Instala el controlador I2S con la configuración anterior
    esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK)
    {
        log_e("Error al instalar el driver I2S: %d", err);
        while (true)
            ; // Si ocurre un error crítico, el sistema se detiene
    }

    // Configura el modo del ADC interno, seleccionando la unidad ADC1 y el canal 6
    err = i2s_set_adc_mode(ADC_UNIT_1, (adc1_channel_t)6);
    if (err != ESP_OK)
    {
        log_e("Error al configurar el modo ADC: %d", err);
        while (true)
            ; // Detiene el sistema si ocurre un error crítico
    }

    // Detiene temporalmente la comunicación I2S mientras se realizan otras configuraciones
    i2s_stop(I2S_PORT);
    vTaskDelay(100); // Pausa de 100 ms para asegurar la estabilidad del sistema

    // Configuración de los registros del ADC SAR para gestionar los canales virtuales
    SYSCON.saradc_ctrl.sar1_patt_len = VIRTUAL_ADC_CHANNELS - 1; // Número de canales - 1
    SYSCON.saradc_sar1_patt_tab[0] = 0x0f3f4f5f;                 // Configuración de patrones para los primeros canales
    SYSCON.saradc_sar1_patt_tab[1] = 0x6f7f0000;                 // Configuración de patrones para los siguientes canales
    SYSCON.saradc_ctrl.sar_clk_div = 6;                          // Divisor de reloj del ADC

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
                I2S_PORT,                // Puerto I2S configurado previamente.
                (char *)adc_tempsamples, // Buffer temporal para almacenar las muestras crudas.
                sizeof(adc_tempsamples), // Tamaño del buffer en bytes.
                &num_bytes_read,         // Puntero para almacenar el número de bytes leídos.
                100                      // Tiempo de espera (100 ms).
            );

            for (int i = 0; i < num_bytes_read / 2; i++)
            {
                int8_t chan = (adc_tempsamples[i] >> 12) & 0xf;
            }

            // Verifica si la lectura fue exitosa.
            if (err != ESP_OK)
            {
                log_e("Error al leer el buffer DMA: %d", err); // Muestra un mensaje de error en el log.
                while (true)
                    ; // Detiene el sistema si ocurre un error crítico.
            }

            // Ajusta el número de muestras leídas dividiendo el tamaño en bytes entre 2 (tamaño de muestra en 16 bits).
            num_bytes_read /= 2;

            // Verifica si el número de muestras leídas coincide con lo esperado.
            if (num_bytes_read != numbersOfSamples)
            {
                log_e("El tamaño del bloque no coincide con el número de muestras: %d", num_bytes_read);
                while (true)
                    ;
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
                    channel[channelmapping[chan]]++;                              // Avanza al siguiente espacio del buffer del canal.
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
                    // Sincroniza phaseshift_0_degree con n
                    if (n == 0)
                    {
                        phaseshift_0_degree = 0; // Resetea cuando n es 0
                    }
                    else
                    {
                        phaseshift_0_degree = n; // Mantiene el phaseshift sincronizado con n
                    }

                    // El resto del código se mantiene igual
                    phaseshift_90_degree = ((numbersOfSamples / 2) / 360.0) * ((channelconfig[i].phaseshift + 90) % 360);
                    phaseshift_90_degree = (n + phaseshift_90_degree) % numbersOfSamples;

                    // Aplica la misma lógica al phaseshift de 90°
                    if (n == 0)
                        phaseshift_90_degree = 0;
                    else if (phaseshift_90_degree == numbersOfSamples - 1)
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
                            if (i == 1)
                            {
                                // log_i("Muestra %d, Valor: %f, phaseshift = %d", n, adc_sample[i], phaseshift_0_degree);
                            }
                        }
                        break;
                    case SET_TO:
                        adc_sample[i] = op_channel;
                        break;
                    case FILTER:
                        // Registro para depuración
                        if (i == 5)
                        {
                            //log_i("Muestra %d, Valor antes filtro: %f", n, adc_sample[i]);
                        }

                        // Actualizar variables declaradas antes del switch
                        dc_offset[i] = dc_offset[i] * 0.99 + adc_sample[i] * 0.01;
                        centered_sample = adc_sample[i] - dc_offset[i];
                        centered_last_sample = last_adc_sample[i] - dc_offset[i];

                        // Implementación del filtro paso alto con coeficiente optimizado
                        ac_filtered[i] = high_pass_coef * (last_ac_filtered[i] + centered_sample - centered_last_sample);
                        if (fabs(ac_filtered[i]) < noise_threshold)
                        {
                            ac_filtered[i] = 0.0; // Elimina ruido de muy bajo nivel
                        }
                        if (i == 5)
                        {
                           // log_i("Muestra %d, Valor despues filtro PasaAlto: %f", n, ac_filtered[i]);
                        }

                        last_ac_filtered[i] = ac_filtered[i];
                        last_adc_sample[i] = adc_sample[i];
                        adc_sample[i] = ac_filtered[i];

                        // Aplicar filtro paso bajo mejorado
                        if (op_channel)
                        {
                            // Tamaño de ventana adaptativo con mayor suavizado
                            int mul = (op_channel <= 4) ? (1 << (op_channel + 1)) : low_pass_window_size;
                            mul = (mul > 32) ? 32 : mul; // Limitar tamaño para evitar problemas de memoria

                            // Almacenar muestra actual en buffer circular
                            dc_filtered[i][n % mul] = adc_sample[i];

                            // Filtro de media ponderada con más peso en muestras recientes
                            float sum = 0.0;
                            float weight_sum = 0.0;

                            for (int j = 0; j < mul; j++)
                            {
                                int idx = (n - j + mul) % mul;
                                float weight = 1.0 + (mul - j) * 0.2; // Ponderación lineal simple
                                sum += dc_filtered[i][idx] * weight;
                                weight_sum += weight;
                            }

                            // Aplicar resultado del filtrado
                            if (weight_sum > 0)
                            {
                                adc_sample[i] = sum / weight_sum;
                            }
                        }
                        // Dentro de la sección FILTER
                        float threshold_value;
                        if (channelconfig[i].type == AC_CURRENT)
                        {
                            threshold_value = 1.0; // Umbral más bajo para canales de corriente
                        }
                        else
                        {
                            threshold_value = 3.0; // Umbral normal para otros canales
                        }

                        // Aplicar banda muerta adaptativa
                        if (fabs(adc_sample[i]) < threshold_value)
                        {
                            adc_sample[i] = 0.0;
                        }

                        // Añade esto después del filtrado completo

                        // Actualiza la energía de la señal usando promedio exponencial
                        signal_energy[i] = signal_energy[i] * 0.99 + adc_sample[i] * adc_sample[i] * 0.01;

                        // Si la energía está por debajo de un umbral, considera que no hay señal
                        if (signal_energy[i] < energy_threshold)
                        {
                            adc_sample[i] = 0.0; // Fuerza a cero para canales sin señal
                        }

                        if (i == 5)
                        {
                         //   log_i("Muestra %d, Valor despues filtro Pasa Bajo: %f", n, adc_sample[i]);
                        }
                        break;
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
                // Comentamos este bucle para no saturar la terminal con logs repetitivos:
                /*
                for (int i = 0; i < 10; i++)
                {
                    log_i("Canal Virtual %d en buffer_probe[%d] = %d", MONITOR_CHANNEL, i, buffer_probe[MONITOR_CHANNEL][i]);
                }
                */
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
        if (TX_buffer != -1)
        {
            // Copia los datos procesados al buffer de prueba para transmisión.
            memcpy(&buffer_probe[0][0], &buffer[0][0], sizeof(buffer));

            // Marca el buffer como procesado.
            TX_buffer = -1;
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
            static float netfrequency_filter[16] = {0.0}; // Buffer para suavizar la frecuencia.
            static int index = 0;                         // Índice del filtro circular.
            static bool netfrequency_firstrun = true;     // Bandera para inicialización del filtro.

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

    // Define el umbral de RMS para considerar que hay señal válida.
    const double RMS_THRESHOLD = 0.0;
    bool has_valid_signal = false;

    // Verificar si algún canal de tipo AC_VOLTAGE tiene señal válida.
    for (int i = 0; i < VIRTUAL_CHANNELS; i++)
    {
        if (channelconfig[i].type == AC_VOLTAGE)
        {
            // Validar antes de calcular el RMS.
            if (numbersOfSamples > 0 && round > 0 && channelconfig[i].sum >= 0)
            {
                double voltage_rms = channelconfig[i].ratio * sqrt(channelconfig[i].sum / (numbersOfSamples * round));

                if (voltage_rms >= RMS_THRESHOLD)
                {
                    has_valid_signal = true;
                    break; // Detener la verificación al encontrar una señal válida.
                }
            }
            else
            {
                printf("Canal %d: Valores no válidos para RMS. sum = %.2f, samples = %d, round = %d\n",
                       i, channelconfig[i].sum, numbersOfSamples, round);
            }
        }
    }

    // Procesar los canales según el resultado de la validación.
    for (int i = 0; i < VIRTUAL_CHANNELS; i++)
    {
        // Si no hay señal válida en los canales de tipo AC_VOLTAGE, asignar 0 a todos los canales.
        if (!has_valid_signal)
        {
            channelconfig[i].rms = 0.0;
            continue; // Saltar el procesamiento de este canal.
        }

        // Saltar canales desactivados.
        if (channelconfig[i].type == NO_CHANNEL_TYPE)
        {
            channelconfig[i].rms = 0.0;
            channelconfig[i].sum = 0.0;
            continue;
        }

        // Procesar cada canal según el tipo.
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
                    channelconfig[i].rms = (channelconfig[i].ratio * sqrt(channelconfig[i].sum / (numbersOfSamples * round))) + channelconfig[i].offset;
                else
                    channelconfig[i].rms = (channelconfig[i].ratio * (channelconfig[i].sum / (numbersOfSamples * round))) + channelconfig[i].offset;
            }
            break;

        case AC_CURRENT:
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
                    channelconfig[i].rms = (channelconfig[i].ratio * sqrt(channelconfig[i].sum / (numbersOfSamples * round))) + channelconfig[i].offset;
                }
                else
                {
                    channelconfig[i].rms = (channelconfig[i].ratio * (channelconfig[i].sum / (numbersOfSamples * round))) + channelconfig[i].offset;
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
                    channelconfig[i].rms = (channelconfig[i].ratio * sqrt(channelconfig[i].sum / (numbersOfSamples * round))) + channelconfig[i].offset;
                }
                else
                {
                    channelconfig[i].rms = (channelconfig[i].ratio * (channelconfig[i].sum / (numbersOfSamples * round))) + channelconfig[i].offset;
                }
            }
            break;

        case NO_CHANNEL_TYPE:
            break;
        }

        // Si la medición no es válida, asignar 0 al RMS y decrementar el contador.
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

        // while (true);
    }
}

// Función auxiliar para calcular desfases
int calculate_phaseshift(int base_shift, int n, int samples)
{
    // El desfase debe ser proporcional al índice de muestra
    double shift_per_sample = (base_shift % 360) / (360.0 / samples);
    return (n + (int)round(shift_per_sample)) % samples;
}

float measure_get_network_frequency(void)
{
    return (measure_config.network_frequency);
}

/**
 * @brief Configura la frecuencia de red y ajusta la frecuencia de muestreo del ADC.
 *
 * @param network_frequency Frecuencia de red deseada (en Hz).
 */
void measure_set_network_frequency(float network_frequency)
{
    // Verifica que la frecuencia esté dentro del rango permitido (16 Hz a 120 Hz).
    if (network_frequency >= 16.0 && network_frequency <= 120.0)
    {
        // Actualiza la configuración de frecuencia de red en la estructura global.
        measure_config.network_frequency = network_frequency;

        // Calcula y establece la nueva frecuencia de muestreo para el ADC.
        esp_err_t err = i2s_set_sample_rates(
            I2S_PORT,
            (samplingFrequency * measure_config.network_frequency / 2) + measure_config.samplerate_corr);

        // Verifica si hubo un error al configurar la frecuencia de muestreo.
        if (err != ESP_OK)
        {
            log_e("Error al configurar la frecuencia de muestreo: %d", err);
            esp_restart(); // Reinicia el sistema de forma controlada.
        }
        else
        {
            log_i("Frecuencia de red configurada: %.2f Hz, Frecuencia de muestreo: %d Hz",
                  measure_config.network_frequency,
                  (int)((samplingFrequency * measure_config.network_frequency / 2) + measure_config.samplerate_corr));
        }
    }
    else
    {
        // Log de advertencia si la frecuencia está fuera de rango.
        log_w("Frecuencia fuera de rango: %.2f Hz. No se realizó la configuración.", network_frequency);
    }
}

/**
 * @brief Devuelve un puntero al buffer de medición procesado.
 *
 * @return uint16_t* Puntero al inicio del buffer de prueba, o NULL si hay un timeout.
 */
uint16_t *measure_get_buffer(void)
{
    // Marca el buffer de transmisión como disponible.
    TX_buffer = 0;

    // Límite de tiempo para salir del bucle.
    unsigned long startTime = millis();
    unsigned long timeout = 1000; // Timeout en milisegundos.

    // Espera activa con límite de tiempo.
    while (TX_buffer == 0)
    {
        if (millis() - startTime > timeout)
        {
            log_e("Timeout al esperar el buffer de transmisión.");
            return NULL; // Retorna NULL si se supera el tiempo límite.
        }
        vTaskDelay(1); // Agrega un pequeño retraso para liberar CPU (solo si usas FreeRTOS).
    }

    // Devuelve un puntero al buffer de prueba.
    return (&buffer_probe[0][0]);
}
/**
 * @brief Realiza la transformada rápida de Fourier (FFT) para los canales virtuales,
 * calcula los armónicos principales (hasta el noveno) y la distorsión armónica total (THD).
 *
 * @return uint16_t* Puntero al inicio del buffer FFT (buffer_fft).
 */
uint16_t *measure_get_fft(void)
{
    static double vReal[numbersOfFFTSamples * 2];
    static double vImag[numbersOfFFTSamples * 2];

    // Configuración de frecuencia de muestreo ajustada
    double samplingRate = (numbersOfSamples) * measure_get_network_frequency();
    samplingRate = round(samplingRate / measure_get_network_frequency()) * measure_get_network_frequency();

    log_i("FFT: Frecuencia de muestreo: %.2f Hz, Frecuencia de red: %.2f Hz", 
          samplingRate, measure_get_network_frequency());
    log_i("FFT: Resolución espectral: %.2f Hz/bin", samplingRate / (numbersOfFFTSamples * 2));

    arduinoFFT FFT(vReal, vImag, numbersOfFFTSamples * 2, samplingRate);

    static const int selectedChannels[] = {0, 1, 4, 5, 8, 9};
    static const size_t numSelectedChannels = sizeof(selectedChannels) / sizeof(selectedChannels[0]);

    for (size_t idx = 0; idx < numSelectedChannels; idx++)
    {
        const int channel = selectedChannels[idx];
        log_i("FFT: Procesando canal %d (%s)", channel, measure_get_channel_name(channel));

        // Preprocesamiento
        double suma = 0;
        for (int i = 0; i < numbersOfFFTSamples * 2; i++)
        {
            int sampleIdx = i * numbersOfSamples / (numbersOfFFTSamples * 2);
            vReal[i] = buffer_probe[channel][sampleIdx] - 2048.0;
            vImag[i] = 0;
            suma += vReal[i];
        }

        double media = suma / (numbersOfFFTSamples * 2);
        log_i("FFT: Canal %d - Valor medio: %.2f", channel, media);

        for (int i = 0; i < numbersOfFFTSamples * 2; i++)
        {
            vReal[i] -= media;
            vReal[i] *= 0.5 * (1 - cos(2 * PI * i / (numbersOfFFTSamples * 2 - 1)));
        }

        FFT.Compute(FFT_FORWARD);
        FFT.ComplexToMagnitude();

        for (int i = 1; i < numbersOfFFTSamples; i++)
        {
            vReal[i] = vReal[i] / numbersOfFFTSamples;
        }

        // Búsqueda de la fundamental
        int freqBinWidth = samplingRate / (numbersOfFFTSamples * 2);
        int minFundamentalBin = (int)(measure_get_network_frequency() * 0.9 / freqBinWidth);
        int maxFundamentalBin = (int)(measure_get_network_frequency() * 1.1 / freqBinWidth);

        log_i("FFT: Canal %d - Buscando fundamental entre bin %d (%.2f Hz) y bin %d (%.2f Hz)", 
              channel, minFundamentalBin, minFundamentalBin * freqBinWidth, 
              maxFundamentalBin, maxFundamentalBin * freqBinWidth);

        int fundamentalIndex = minFundamentalBin;
        double maxValue = vReal[minFundamentalBin];

        for (int i = minFundamentalBin + 1; i <= maxFundamentalBin; i++)
        {
            if (i < numbersOfFFTSamples && vReal[i] > maxValue)
            {
                maxValue = vReal[i];
                fundamentalIndex = i;
            }
        }

        log_i("FFT: Canal %d - Fundamental detectada en bin %d (%.2f Hz) con amplitud %.2f", 
              channel, fundamentalIndex, fundamentalIndex * freqBinWidth, maxValue);

        // Función para obtener amplitud armónica con interpolación
        auto getHarmonicAmplitude = [&](int harmonicMultiple) {
            int binIndex = fundamentalIndex * harmonicMultiple;
            if (binIndex >= numbersOfFFTSamples) 
                return 0.0;
                
            double y1 = (binIndex > 0) ? vReal[binIndex-1] : 0;
            double y2 = vReal[binIndex];
            double y3 = (binIndex < numbersOfFFTSamples-1) ? vReal[binIndex+1] : 0;
            
            double d = (y3 - y1) / (2 * (2 * y2 - y1 - y3));
            if (fabs(d) < 1.0)
                return y2 - 0.25 * (y1 - y3) * d;
            else
                return y2;
        };

        // Cálculo de los armónicos
        harmonic_values[channel].fundamental = getHarmonicAmplitude(1);
        harmonic_values[channel].thirdHarmonic = getHarmonicAmplitude(3);
        harmonic_values[channel].fifthHarmonic = getHarmonicAmplitude(5);
        harmonic_values[channel].seventhHarmonic = getHarmonicAmplitude(7);
        harmonic_values[channel].ninthHarmonic = getHarmonicAmplitude(9);

        log_i("FFT: Canal %d - Armónicos: F: %.2f, 3°: %.2f, 5°: %.2f, 7°: %.2f, 9°: %.2f Hz", 
              channel, 
              fundamentalIndex * freqBinWidth,
              fundamentalIndex * 3 * freqBinWidth,
              fundamentalIndex * 5 * freqBinWidth,
              fundamentalIndex * 7 * freqBinWidth,
              fundamentalIndex * 9 * freqBinWidth);

        log_i("FFT: Canal %d - Amplitudes: F: %.2f, 3°: %.2f, 5°: %.2f, 7°: %.2f, 9°: %.2f", 
              channel, 
              harmonic_values[channel].fundamental,
              harmonic_values[channel].thirdHarmonic,
              harmonic_values[channel].fifthHarmonic,
              harmonic_values[channel].seventhHarmonic,
              harmonic_values[channel].ninthHarmonic);

        double sumSquaresHarmonics = 
            pow(harmonic_values[channel].thirdHarmonic, 2) +
            pow(harmonic_values[channel].fifthHarmonic, 2) +
            pow(harmonic_values[channel].seventhHarmonic, 2) +
            pow(harmonic_values[channel].ninthHarmonic, 2);

        if (harmonic_values[channel].fundamental > 0)
        {
            harmonic_values[channel].thd = (sqrt(sumSquaresHarmonics) / harmonic_values[channel].fundamental) * 100.0;
            log_i("FFT: Canal %d - THD: %.2f%%", channel, harmonic_values[channel].thd);
        }
        else
        {
            harmonic_values[channel].thd = 0.0;
            log_i("FFT: Canal %d - THD: 0.00% (fundamental = 0)", channel);
        }

        // Guardar datos para visualización
        for (int i = 0; i < numbersOfFFTSamples && i < numbersOfSamples; i++)
        {
            buffer_fft[channel][i] = (uint16_t)(vReal[i] * 100 + 2048);
        }
    }

    return &buffer_fft[0][0];
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

/*double measure_get_channel_escala_osc(uint16_t channel) {
    if (channel >= VIRTUAL_CHANNELS)
        return 1.0; // Por defecto, sin ajuste
    return channelconfig[channel].escala_osc;
}

void measure_set_channel_escala_osc(uint16_t channel, float escala_osc) {
    if (channel >= VIRTUAL_CHANNELS)
        return;
    channelconfig[channel].escala_osc = escala_osc;
}

double measure_get_channel_escala_foot(uint16_t channel) {
    if (channel >= VIRTUAL_CHANNELS)
        return 1.0; // Por defecto, sin ajuste
    return channelconfig[channel].escala_foot;
}

void measure_set_channel_escala_foot(uint16_t channel, float escala_foot) {
    if (channel >= VIRTUAL_CHANNELS)
        return;
    channelconfig[channel].escala_foot = escala_foot;
}
*/
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
