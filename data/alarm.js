// Variables globales para el control del osciloscopio y la conexión
var pause_osc = true; // Indica si el osciloscopio está pausado
var connect = true; // Estado inicial de conexión
var timeout = 3; // Temporizador para control de conexiones
var savecounter = 0; // Contador de guardados (si aplica)
var connection; // WebSocket para manejar la conexión
let fftChartData = []; // Datos del gráfico FFT

// Ejecutar lógica solo después de que el DOM esté cargado
document.addEventListener('DOMContentLoaded', function () {
    getconnect(); // Configura la conexión al servidor

    // Actualizar estado cada 1000 ms si la conexión está activa
    setInterval(function () {
        if (connection && connection.readyState === WebSocket.OPEN) {
            getStatus(); // Actualiza el estado del sistema
            if (!pause_osc) { 
                OScopeProbe(); // Obtiene datos del osciloscopio si no está pausado
            }
        }
    }, 1000);

    // Restablecer el zoom de los gráficos al hacer clic en el botón correspondiente
    const resetZoomButton = document.getElementById('resetZoomButton');
    if (resetZoomButton) {
        resetZoomButton.addEventListener('click', function () {
            if (charts['oscilloscopeChart']) {
                charts['oscilloscopeChart'].resetZoom(); // Restablece el zoom del osciloscopio
            }
            if (charts['fftChart']) {
                charts['fftChart'].resetZoom(); // Restablece el zoom del FFT
            }
        });
    } else {
        console.error("El botón 'resetZoomButton' no se encontró.");
    }
});

function getconnect() {
    // Crear WebSocket para la conexión con el servidor
    connection = new WebSocket('ws://' + location.hostname + '/ws', ['arduino']);

    // Cuando la conexión está abierta
    connection.onopen = function () {
        connect = true; // Marca la conexión como activa
        sendCMD("STA"); // Envía un comando inicial al servidor

        // Solicita configuraciones según los elementos presentes en el DOM
        if (document.getElementById('OScope')) sendCMD("get_channel_list");
        if (document.getElementById('measurement_settings')) {
            sendCMD("get_measurement_settings");
            sendCMD("get_channel_config");
        }
        if (document.getElementById('wlan_settings')) sendCMD("get_wlan_settings");
        if (document.getElementById('group_settings')) sendCMD("get_group_settings");
        if (document.getElementById('mqtt_settings')) sendCMD("get_mqtt_settings");
        if (document.getElementById('hostname_settings')) sendCMD("get_hostname_settings");
        if (document.getElementById('display_settings')) sendCMD("get_display_settings");
        if (document.getElementById('ioport_settings')) sendCMD("get_ioport_settings");
    };

    // Cuando se recibe un mensaje del servidor
    connection.onmessage = function (e) {
        partsarry = e.data.split('\\'); // Divide el mensaje recibido en partes

        if (partsarry[0] == 'OScopeProbe') {
            console.time("GotOScope Execution");
            window.GotOScope(e.data); // Procesa los datos del osciloscopio
            console.timeEnd("GotOScope Execution");
            return;
        }

        if (partsarry[0] == 'status') {
            if (partsarry[1] == 'Save') {
                document.getElementById('fixedfooter').style.background = "#008000";
                savecounter = 2;
            } else {
                document.getElementById('status').firstChild.nodeValue = partsarry[1];
            }
            timeout = 4; // Reinicia el temporizador
            return;
        }

        // Actualizaciones específicas de elementos en el DOM
        if (partsarry[0] == 'get_channel_list') {
            const label = document.getElementById(partsarry[1]);
            if (label) label.textContent = partsarry[2];
            return;
        }

        if (partsarry[0] == 'get_channel_use_list') {
            const checkbox = document.getElementById(partsarry[1]);
            if (checkbox) checkbox.checked = partsarry[2] == 'true';
            const select = document.getElementById(partsarry[3]);
            if (select && select.options[partsarry[4]]) {
                select.options[partsarry[4]].text = partsarry[5];
            }
            return;
        }

        if (partsarry[0] == 'get_group_use_list') {
            const checkbox = document.getElementById(partsarry[1]);
            if (checkbox && partsarry[2] == 'true') checkbox.checked = true;
            const label = document.getElementById(partsarry[3]);
            if (label) label.textContent = partsarry[4];
            return;
        }

        if (partsarry[0] == 'option') {
            const select = document.getElementById(partsarry[1]);
            if (select && select.options[partsarry[2]]) {
                select.options[partsarry[2]].text = partsarry[3];
                refreshOpcode('channel_opcodeseq_str');
            }
            return;
        }

        if (partsarry[0] == 'label') {
            const label = document.getElementById(partsarry[1]);
            if (label) label.textContent = partsarry[2];
            return;
        }

        if (partsarry[0] == 'checkbox') {
            const checkbox = document.getElementById(partsarry[1]);
            if (checkbox) checkbox.checked = partsarry[2] == 'true';
            return;
        }

        const element = document.getElementById(partsarry[0]);
        if (element) {
            element.value = partsarry[1];
            if (document.getElementById('channel_opcodeseq_str')) {
                refreshOpcode('channel_opcodeseq_str');
            }
        }
    };

    // Manejo del cierre del WebSocket
    connection.onclose = function (e) {
        console.log('Socket is closed. Reconnect will be attempted in 1 second.', e.reason);
        connect = false; // Marca la conexión como inactiva
        setTimeout(getconnect, 1000); // Reintenta la conexión
    };

    // Manejo de errores del WebSocket
    connection.onerror = function (error) {
        connect = false;
        console.log('WebSocket Error ', error);
        connection.close(); // Cierra la conexión en caso de error
    };
}

function setSensor(sensor) {
    console.log('Server: ', sensor);
    const partsarry = sensor.split('\\'); // Asegurar uso de `const` para valores inmutables

    if (partsarry.length < 4) {
        console.error("Datos incompletos del sensor:", partsarry);
        return;
    }

    const ratioElem = document.getElementById('channel_ratio');
    const phaseShiftElem = document.getElementById('channel_phaseshift');
    const offsetElem = document.getElementById('channel_offset');
    const trueRmsElem = document.getElementById('channel_true_rms');

    if (ratioElem) ratioElem.value = partsarry[0];
    if (phaseShiftElem) phaseShiftElem.value = partsarry[1];
    if (offsetElem) offsetElem.value = partsarry[2];
    if (trueRmsElem) trueRmsElem.checked = partsarry[3] === 'true';
}

function sendCMD(value) {
    console.log("Client: " + value);

    if (connect && connection.readyState === WebSocket.OPEN) {
        connection.send(value);
    } else {
        console.warn("No active connection. Command not sent:", value);
    }
}
function check_opcode_and_options(id, option) {
    const element = document.getElementById(id);
    const selectElement = document.getElementById(option);

    if (!element || !selectElement) {
        console.error("Element or select option not found.");
        return;
    }

    const value = element.value;

    const optionSets = {
        '5': [
            "ADC1-CH5 GPIO33", "ADC1-CH6 GPIO34", "ADC1-CH7 GPIO35",
            "ADC1-CH0 GPIO36 (VP)", "ADC1-CH3 GPIO39 (VN)", "ADC1-CH4 GPIO32",
            "not available", "not available", "not available",
            "not available", "not available", "not available", "not available"
        ],
        '6': Array.from({ length: 13 }, (_, i) => i.toString()),
        '7': [
            "level 0", "level 1", "level 2", "level 3", "level 4", "level 5",
            "not available", "not available", "not available",
            "not available", "not available", "not available", "not available"
        ],
        'default': Array(13).fill("-")
    };

    if (['1', '2', '3', '8', 'c', '9', 'b'].includes(value)) {
        // Copiar opciones del elemento "channel"
        for (let i = 0; i < 13; i++) {
            selectElement.options[i].text = document.getElementById("channel").options[i].text;
        }
    } else {
        const options = optionSets[value] || optionSets['default'];
        for (let i = 0; i < options.length; i++) {
            selectElement.options[i].text = options[i];
        }
    }
}

// Enviar configuraciones específicas al servidor
function get_channel_config() { sendCMD("get_channel_config"); }
function get_wlan_settings() { sendCMD("get_wlan_settings"); }
function get_mqtt_settings() { sendCMD("get_mqtt_settings"); }
function get_measurement_settings() { sendCMD("get_measurement_settings"); }
function get_group_settings() { sendCMD("get_group_settings"); }
function get_hostname_settings() { sendCMD("get_hostname_settings"); }
function get_ioport_settings() { sendCMD("get_ioport_settings"); }

// Crear y enviar configuraciones de grupos de canales
function send_channel_groups() {
    var channel_group_str = 'channel_group\\';
    
    for (var channel = 0;; channel++) {
        if (!document.getElementById("channel" + channel + "_0")) break;
        
        for (var group = 0;; group++) {
            var checkbox = document.getElementById("channel" + channel + "_" + group);
            if (checkbox) {
                if (checkbox.checked) {
                    channel_group_str += group;
                    break;
                }
            } else {
                break;
            }
        }
    }
    sendCMD(channel_group_str);
}

// Refrescar el estado actual
function refreshValue() {
    sendCMD("STA");
}

// Guardar configuraciones
function SaveSettings() {
    sendCMD("SAV");
}

// Enviar estado de un checkbox
function SendCheckboxSetting(value) {
    var checkbox = document.getElementById(value);
    if (checkbox) {
        var isChecked = checkbox.checked ? 1 : 0;
        sendCMD(value + "\\" + isChecked);
    }
}

// Enviar un valor de configuración
function SendSetting(value) {
    var element = document.getElementById(value);
    if (element) {
        sendCMD(value + "\\" + element.value);
    }
}

// Obtener el estado del sistema
function getStatus() {
    sendCMD("STS");
    savecounter--;
    timeout--;

    if (timeout > 0 && savecounter < 0) {
        document.getElementById('fixedfooter').style.background = "#333333";
    } else if (timeout < 0) {
        var statusElem = document.getElementById('status');
        if (statusElem) {
            document.getElementById('fixedfooter').style.background = "#800000";
            statusElem.firstChild.nodeValue = 'offline ... Esperando conexión';
        }
    }
}

function refreshOpcode(value) {
    const element = document.getElementById(value);
    if (!element) return;

    const data = element.value;
    let i = 0;

    // Actualizar valores de opcode_ y channel_
    while (true) {
        const opcodeElem = document.getElementById("opcode_" + i);
        const channelElem = document.getElementById("channel_" + i);

        if (!opcodeElem || !channelElem) break;

        opcodeElem.value = data.substr(i * 2, 1);
        channelElem.value = data.substr(i * 2 + 1, 1);
        i++;
    }

    // Recalcular opciones basadas en opcode y channel
    i = 0; // Reiniciar contador
    while (true) {
        const opcodeElem = document.getElementById("opcode_" + i);
        const channelElem = document.getElementById("channel_" + i);

        if (!opcodeElem || !channelElem) break;

        check_opcode_and_options("opcode_" + i, "channel_" + i);
        i++;
    }

    // Enviar datos en tiempo real si es necesario
    const realtimeEdit = document.getElementById("realtime_edit");
    if (realtimeEdit && realtimeEdit.checked) {
        SendSetting(value);
    }
}

function refreshOpcodeStr(value) {
    const element = document.getElementById(value);
    if (!element) return;

    let opcode_str = '';
    let i = 0;

    // Construir el string opcode_str
    while (true) {
        const opcodeElem = document.getElementById("opcode_" + i);
        const channelElem = document.getElementById("channel_" + i);

        if (!opcodeElem || !channelElem) break;

        opcode_str += opcodeElem.value + channelElem.value;
        check_opcode_and_options("opcode_" + i, "channel_" + i);
        i++;
    }

    element.value = opcode_str;
    console.log(opcode_str);

    // Enviar datos en tiempo real si es necesario
    const realtimeEdit = document.getElementById("realtime_edit");
    if (realtimeEdit && realtimeEdit.checked) {
        SendSetting(value);
    }
}

window.OScopeProbe = function() {
    const channelListStr = Array.from({ length: 13 })
        .reduce((acc, _, i) => {
            const channelElement = document.getElementById(`channel${i}`);
            return acc + (channelElement && channelElement.checked ? "1" : "0");
        }, "");
    sendCMD(`OSC\\${channelListStr}`);
}

window.toggleDarkMode = function() {
    document.body.classList.toggle('dark-mode'); // Alternar modo oscuro
    updateCanvasColors(); // Actualizar colores de los gráficos
}

const charts = {};

const channelNames = [
    'L1 Corriente', 'L1 Tensión', 'L1 Potencia', 'L1 Potencia Reactiva',
    'L2 Corriente', 'L2 Tensión', 'L2 Potencia', 'L2 Potencia Reactiva',
    'L3 Corriente', 'L3 Tensión', 'L3 Potencia', 'L3 Potencia Reactiva', 'Todas las potencias'
];


function createOrUpdateChart(chartId, datasets, numSamples, title, yLabel) {
    const canvas = document.getElementById(chartId);
    if (!canvas) {
        console.error(`Canvas con ID '${chartId}' no encontrado.`);
        return;
    }

    const ctx = canvas.getContext('2d');

    const defaultOptions = {
        responsive: true,
        animation: false,
        maintainAspectRatio: false,
        interaction: {
            mode: 'nearest',
            intersect: false,
        },
        plugins: {
            legend: { display: true, position: 'top' },
            title: { display: true, text: title, font: { size: 16 } },
            zoom: {
                zoom: {
                    wheel: { enabled: true },
                    mode: 'y',
                    limits: { y: { min: -100, max: 1000 } },
                },
            },
            tooltip: {
                callbacks: {
                    label: (tooltipItem) => `Valor: ${tooltipItem.raw.y.toFixed(2)}`,
                },
            },
        },
        scales: {
            x: {
                type: 'linear',
                position: 'bottom',
                title: { display: true, text: 'Muestras' },
                min: 0,
                max: numSamples,
            },
            y: {
                title: { display: true, text: yLabel },
                beginAtZero: false,
                min: -1000,
                suggestedMin: -1000,
                suggestedMax: 1000,
                ticks: {
                    callback: (value) => value.toFixed(1),
                },
            },
        },
    };

    if (!charts[chartId]) {
        charts[chartId] = new Chart(ctx, {
            type: 'line',
            data: { datasets: datasets },
            options: defaultOptions,
        });
    } else {
        charts[chartId].data.datasets = datasets;
        charts[chartId].options.scales.x.min = 0;
        charts[chartId].options.scales.x.max = numSamples;
        charts[chartId].update('none'); // Actualización más eficiente
    }
}

let updateOscChart = false; // Variable global para alternar gráficos

let lastExecution = 0;
const interval = 200; // Ejecutar máximo una vez cada 100 ms
function GotOScope(data) {
    requestAnimationFrame(() => {
        const now = performance.now();
        if (now - lastExecution < interval) return;

        lastExecution = now;
        const parts = data.split('\\');
        if (parts[0] !== 'OScopeProbe') return;

        const numChannels = parseInt(parts[1], 10);
        const numSamples = parseInt(parts[2], 10);
        const fftSamples = parseInt(parts[3], 10);
        const channelData = parts[5];
        const fftData = parts[6];
        const activeChannels = decodeActiveChannels(parts[7]);

        const oscilloscopeDatasets = prepareOscilloscopeData(activeChannels, numSamples, channelData);
        const fftDatasets = prepareFFTData(activeChannels, fftSamples, fftData);

        if (updateOscChart) {
            createOrUpdateChart('oscilloscopeChart', oscilloscopeDatasets, numSamples, 'Osciloscopio', 'Amplitud');
        } else {
            createOrUpdateChart('fftChart', fftDatasets, fftSamples, 'FFT', 'Magnitud');
        }

        updateOscChart = !updateOscChart;
    });
}


function decodeActiveChannels(activeChannelsHex) {
    // Convertir el hexadecimal a binario, rellenando hasta 13 bits
    const binaryString = parseInt(activeChannelsHex, 16).toString(2).padStart(13, '0');

    // Crear un array con los índices de los canales activos (bits '1')
    return binaryString
        .split('') // Dividir el binario en bits individuales
        .map((bit, index) => (bit === '1' ? index : null)) // Mapear bits '1' a sus índices
        .filter(index => index !== null); // Filtrar los canales activos
}

// Función para determinar el factor de ajuste basado en el índice del canal
function getAdjustmentFactor(channelIndex) {
    switch (true) {
        case [0, 4, 8].includes(channelIndex): return 1.5;
        case [1, 5, 9].includes(channelIndex): return 0.89;
        case [2, 6, 10].includes(channelIndex): return 3;
        case [3, 7, 11].includes(channelIndex): return 4;
        default:
            console.warn(`Canal ${channelIndex} no tiene un factor definido. Usando 1.`);
            return 1;
    }
}

function limitDataset(data, maxPoints = 500) {
    const step = Math.ceil(data.length / maxPoints);
    return data.filter((_, index) => index % step === 0);
}

function prepareOscilloscopeData(activeChannels, numSamples, data) {
    const datasets = [];
    const dataLength = numSamples * activeChannels.length * 3;

    if (data.length < dataLength) {
        console.error("Los datos del osciloscopio están incompletos.");
        return datasets;
    }

    activeChannels.forEach((channelIndex, activeIndex) => {
        const factor = getAdjustmentFactor(channelIndex);
        const values = [];

        for (let i = 0; i < numSamples; i++) {
            const offsetPos = (numSamples * activeIndex + i) * 3;
            const hexValue = data.slice(offsetPos, offsetPos + 3);
            const adcValue = parseInt(hexValue, 16);

            // Ajustar el valor
            const adjustedValue = (adcValue - 2048) * factor;
            values.push({ x: i, y: adjustedValue });
        }

        const limitedValues = limitDataset(values, 500);
datasets.push({
    label: `${channelNames[channelIndex]}`,
    data: limitedValues, // Datos limitados
    borderColor: getColor(channelIndex),
    fill: false,
    pointRadius: 2,
    tension: 0.01,
});
    });

    return datasets;
}

function prepareFFTData(activeChannels, fftSamples, data) {
    const datasets = [];
    const dataLength = fftSamples * activeChannels.length * 3;

    if (data.length < dataLength) {
        console.error("Los datos FFT están incompletos.");
        return datasets;
    }

    activeChannels.forEach((channelIndex, activeIndex) => {
        const factor = getAdjustmentFactor(channelIndex);
        const values = [];

        for (let i = 0; i < fftSamples; i++) {
            const offset = (fftSamples * activeIndex + i) * 3;
            const hexValue = data.slice(offset, offset + 3);
            const value = (parseInt(hexValue, 16) / 1024) * 1000 * factor;
            values.push({ x: i, y: value });
        }

        const limitedValues = limitDataset(values, 500);
datasets.push({
    label: `${channelNames[channelIndex]}`,
    data: limitedValues, // Datos limitados
    borderColor: getColor(channelIndex),
    fill: false,
    pointRadius: 2,
    tension: 0.01,
});
    });

    return datasets;
}


function getColor(index) {
    const colors = ['#FF5733', '#3498DB', '#2ECC71', '#F39C12', '#C0392B', '#2980B9', '#27AE60'];
    
    // Generar colores dinámicamente si el índice excede la lista
    if (index >= colors.length) {
        const hue = (index * 137.5) % 360; // Distribuye colores en el espectro
        return `hsl(${hue}, 70%, 50%)`; // Genera un color dinámico en formato HSL
    }

    return colors[index];
}

function ToggleOScopePause() {
    pause_osc = !pause_osc;

    // Actualizar el texto de un botón o indicador
    const button = document.getElementById('pauseOscButton');
    if (button) {
        button.textContent = pause_osc ? 'Reanudar Osciloscopio' : 'Pausar Osciloscopio';
    }

    // Reanudar si está despausado
    if (!pause_osc) OScopeProbe();
}

/*function PhaseshiftPlus() {
    sendCMD("PS+");
}

function PhaseshiftMinus() {
    sendCMD("PS-");
}

function SampleratePlus() {
    sendCMD( "FQ+" );	
}

function SamplerateMinus() {
    sendCMD( "FQ-" );		
}
*/