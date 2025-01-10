// Declaración de las variables globales
var pause_osc = true;
var connect = true;
var timeout = 3;
var savecounter = 0;
var connection;
let fftChartData = [];
// Esperar a que el DOM esté listo antes de ejecutar cualquier lógica que acceda a elementos del DOM
document.addEventListener('DOMContentLoaded', function () {
    // Llamada a getconnect() una vez que el DOM está listo
    getconnect();

    // Configuración del intervalo para obtener el estado cada 500 ms
    setInterval(function () {
        if (connection && connection.readyState === WebSocket.OPEN) {
            getStatus();
            if (!pause_osc) {
                OScopeProbe();
            }
        }
    }, 1000);

    // Código para agregar el evento al botón de restablecer zoom
    const resetZoomButton = document.getElementById('resetZoomButton');
    if (resetZoomButton) {
        resetZoomButton.addEventListener('click', function () {
            if (charts['oscilloscopeChart']) {
                charts['oscilloscopeChart'].resetZoom();
            }
            if (charts['fftChart']) {
                charts['fftChart'].resetZoom();
            }
        });
    } else {
        console.error("El botón 'resetZoomButton' no se encontró.");
    }
});


function getconnect() {
    connection = new WebSocket('ws://' + location.hostname + '/ws', ['arduino']);

    connection.onopen = function () {
        connect = true; 	
        sendCMD("STA");

        if( document.getElementById( 'OScope' ) )
            sendCMD( "get_channel_list" );

        if( document.getElementById( 'measurement_settings' ) ) {
            sendCMD( "get_measurement_settings" );
            sendCMD( "get_channel_config" );
        }

        if( document.getElementById( 'wlan_settings' ) )
            sendCMD( "get_wlan_settings" );

        if( document.getElementById( 'group_settings' ) )
            sendCMD( "get_group_settings" );

        if( document.getElementById( 'mqtt_settings' ) )
            sendCMD( "get_mqtt_settings" );

        if( document.getElementById( 'hostname_settings' ) )
            sendCMD( "get_hostname_settings" );

        if( document.getElementById( 'display_settings' ) )
            sendCMD( "get_display_settings" );

        if( document.getElementById( 'ioport_settings' ) )
            sendCMD( "get_ioport_settings" );
    };

    connection.onmessage = function (e) {
        //console.log('Server: ', e.data);
        partsarry = e.data.split('\\');
    
        if (partsarry[0] == 'OScopeProbe') {
            window.GotOScope(e.data);
            return;
        }    
        // Procesar otros tipos de mensajes como antes
        if (partsarry[0] == 'status') {
            if (partsarry[1] == 'Save') {
                document.getElementById('fixedfooter').style.background = "#008000";
                savecounter = 2;
            } else {
                document.getElementById('status').firstChild.nodeValue = partsarry[1];
            }
    
            timeout = 4;
            return;
        }

    if (partsarry[0] == 'get_channel_list') {
        if (document.getElementById(partsarry[1])) {
            const label = document.getElementById(partsarry[1]);
            label.textContent = partsarry[2];
        }
        return;
    }

    if (partsarry[0] == 'get_channel_use_list') {
        if (document.getElementById(partsarry[1])) {
            document.getElementById(partsarry[1]).checked = partsarry[2] == 'true';
        }
        if (document.getElementById(partsarry[3])) {
            document.getElementById(partsarry[3]).options[partsarry[4]].text = partsarry[5];
        }
        return;
    }

    if (partsarry[0] == 'get_group_use_list') {
        if (document.getElementById(partsarry[1]) && partsarry[2] == 'true') {
            document.getElementById(partsarry[1]).checked = true;
        }
        if (document.getElementById(partsarry[3])) {
            const label = document.getElementById(partsarry[3]);
            label.textContent = partsarry[4];
        }
        return;
    }

    if (partsarry[0] == 'option') {
        if (document.getElementById(partsarry[1]).options[partsarry[2]]) {
            document.getElementById(partsarry[1]).options[partsarry[2]].text = partsarry[3];
            refreshOpcode('channel_opcodeseq_str');
        }
        return;
    }

    if (partsarry[0] == 'label') {
        if (document.getElementById(partsarry[1])) {
            document.getElementById(partsarry[1]).textContent = partsarry[2];
        }
        return;
    }

    if (partsarry[0] == 'checkbox') {
        if (document.getElementById(partsarry[1])) {
            document.getElementById(partsarry[1]).checked = partsarry[2] == 'true';
        }
        return;
    }

    if (document.getElementById(partsarry[0])) {
        document.getElementById(partsarry[0]).value = partsarry[1];
        if (document.getElementById('channel_opcodeseq_str')) {
            refreshOpcode('channel_opcodeseq_str');
        }
    }
}

    connection.onclose = function(e) {
        console.log('Socket is closed. Reconnect will be attempted in 1 second.', e.reason);
        connect = false;
        setTimeout(function() { getconnect(); }, 1000);
    };

    connection.onerror = function (error) {
        connect = false; 
        console.log('WebSocket Error ', error);
        connection.close();
    };
}

function setSensor( sensor ) {
    console.log('Server: ', sensor );
	partsarry = sensor.split('\\');
	
	console.log('Sensor ratio: ', partsarry[ 0 ] );
	console.log('Sensor phaseshift: ', partsarry[ 1 ] );
	console.log('Sensor offset: ', partsarry[ 2 ] );
	
	if( document.getElementById( 'channel_ratio' ) )
	    document.getElementById( 'channel_ratio' ).value = partsarry[ 0 ];
	if( document.getElementById( 'channel_phaseshift' ) )
	    document.getElementById( 'channel_phaseshift' ).value = partsarry[ 1 ];
	if( document.getElementById( 'channel_offset' ) )
	    document.getElementById( 'channel_offset' ).value = partsarry[ 2 ];
	if( document.getElementById( 'channel_true_rms' ) ) {
  	if( partsarry[ 3 ] == 'true' ) 
	    document.getElementById( 'channel_true_rms' ).checked = true;
	  else
	    document.getElementById( 'channel_true_rms' ).checked = false;
	}
}

function sendCMD( value ) {
    console.log( "Client: " + value );
	if ( connect )
        connection.send( value );
    else
        console.log( "no connection" );
}

function check_opcode_and_options( id, option ) {
    if( document.getElementById( id ).value == '5' ) {
        document.getElementById( option ).options[ 0 ].text = "ADC1-CH5 GPIO33";
        document.getElementById( option ).options[ 1 ].text = "ADC1-CH6 GPIO34";
        document.getElementById( option ).options[ 2 ].text = "ADC1-CH7 GPIO35";
        document.getElementById( option ).options[ 3 ].text = "ADC1-CH0 GPIO36 (VP)";
        document.getElementById( option ).options[ 4 ].text = "ADC1-CH3 GPIO39 (VN)";
        document.getElementById( option ).options[ 5 ].text = "ADC1-CH4 GPIO32";
        document.getElementById( option ).options[ 6 ].text = "not available";
        document.getElementById( option ).options[ 7 ].text = "not available";
        document.getElementById( option ).options[ 8 ].text = "not available";
        document.getElementById( option ).options[ 9 ].text = "not available";
        document.getElementById( option ).options[ 10 ].text = "not available";
        document.getElementById( option ).options[ 11 ].text = "not available";
        document.getElementById( option ).options[ 12 ].text = "not available";
    }
    else if( document.getElementById( id ).value == '6' ) {
        document.getElementById( option ).options[ 0 ].text = "0";
        document.getElementById( option ).options[ 1 ].text = "1";
        document.getElementById( option ).options[ 2 ].text = "2";
        document.getElementById( option ).options[ 3 ].text = "3";
        document.getElementById( option ).options[ 4 ].text = "4";
        document.getElementById( option ).options[ 5 ].text = "5";
        document.getElementById( option ).options[ 6 ].text = "6";
        document.getElementById( option ).options[ 7 ].text = "7";
        document.getElementById( option ).options[ 8 ].text = "8";
        document.getElementById( option ).options[ 9 ].text = "9";
        document.getElementById( option ).options[ 10 ].text = "10";
        document.getElementById( option ).options[ 11 ].text = "11";
        document.getElementById( option ).options[ 12 ].text = "12";
    }
    else if( document.getElementById( id ).value == '7' ) {
        document.getElementById( option ).options[ 0 ].text = "level 0";
        document.getElementById( option ).options[ 1 ].text = "level 1";
        document.getElementById( option ).options[ 2 ].text = "level 2";
        document.getElementById( option ).options[ 3 ].text = "level 3";
        document.getElementById( option ).options[ 4 ].text = "level 4";
        document.getElementById( option ).options[ 5 ].text = "level 5";
        document.getElementById( option ).options[ 6 ].text = "not available";
        document.getElementById( option ).options[ 7 ].text = "not available";
        document.getElementById( option ).options[ 8 ].text = "not available";
        document.getElementById( option ).options[ 9 ].text = "not available";
        document.getElementById( option ).options[ 10 ].text = "not available";
        document.getElementById( option ).options[ 11 ].text = "not available";
        document.getElementById( option ).options[ 12 ].text = "not available";
    }
    else if( ( document.getElementById( id ).value >= '1' && document.getElementById( id ).value <= '3' ) || document.getElementById( id ).value == '8' || document.getElementById( id ).value == 'c' || document.getElementById( id ).value == '9'  || document.getElementById( id ).value == 'b' ) {
        for( var i = 0 ; i < 13 ; i++ )
            document.getElementById( option ).options[ i ].text = document.getElementById( "channel" ).options[ i ].text;
    }
    else if( document.getElementById( id ) ) {
        document.getElementById( option ).value = 0;
        for( var i = 0 ; i < 13 ; i++ )
            document.getElementById( option ).options[ i ].text = "-";
    }
}

function get_channel_config() {
    sendCMD("get_channel_config");
}

function get_wlan_settings() {
    sendCMD("get_wlan_settings");
}

function get_mqtt_settings() {
    sendCMD("get_mqtt_settings");
}

function get_measurement_settings() {
    sendCMD("get_measurement_settings");
}

function get_group_settings() {
    sendCMD("get_group_settings");
}

function get_hostname_settings() {
    sendCMD("get_hostname_settings");
}

function get_ioport_settings() {
    sendCMD("get_ioport_settings");
}

function send_channel_groups() {
    var channel_group_str = 'channel_group\\';
    
    for( var channel = 0 ;; channel++ ) {
        if ( !document.getElementById( "channel" + channel + "_0" ) )
            break;
        
        for( var group = 0 ;; group++ ) {
            if ( document.getElementById( "channel" + channel + "_"+ group ) ) {
                if( document.getElementById( "channel" + channel + "_"+ group ).checked ) {
                    channel_group_str += group;
                    break;          
                }
            }
            else {
                break;      
            }
        }
    }
    sendCMD( channel_group_str );
}

function refreshValue() {
    sendCMD("STA");
}

function SaveSettings() {
    sendCMD("SAV");
}

function SendCheckboxSetting( value ) {
    if( document.getElementById( value ) ) {
        if( document.getElementById( value ).checked )
            sendCMD( value + "\\" + 1 );
        else
            sendCMD( value + "\\" + 0 );
    }
}

function SendSetting( value ) {
    sendCMD( value + "\\" + document.getElementById( value ).value );
}

function getStatus() {
    sendCMD("STS");
    savecounter--;
    timeout--;

    if (timeout > 0 && savecounter < 0) {
        document.getElementById('fixedfooter').style.background = "#333333";
    } else if (timeout < 0) {
        if (document.getElementById('status')) {
            document.getElementById('fixedfooter').style.background = "#800000";
            document.getElementById('status').firstChild.nodeValue = 'offline ... Esperando conexión';
        }
    }
}

function refreshOpcode( value ) {
    if( !document.getElementById( value ) )
        return;
        
    var data = document.getElementById( value ).value;
    
    for( var i = 0;; i++ ) {
        /**
         * check if opcode id exist
         */
        if ( document.getElementById( "opcode_" + i ) )
            document.getElementById( "opcode_" + i ).value = data.substr( ( i * 2 ), 1);
        else
            break;
        /**
         * check if channel id exist
         */
        if ( document.getElementById( "channel_" + i ) )
            document.getElementById( "channel_" + i ).value = data.substr( ( i * 2 ) + 1, 1);
        else
            break;
    }
    /**
     * crawl all opcodes and channels for change channel names
     */
    for( var i = 0;; i++ ) {
        if ( document.getElementById( "opcode_" + i ) && document.getElementById( "channel_" + i ) )
            check_opcode_and_options( "opcode_" + i, "channel_" + i );
        else
            break;
    }

    if( document.getElementById( "realtime_edit" ) ) {
        if( document.getElementById( "realtime_edit" ).checked )
            SendSetting( value );
    }
    return;
}


function refreshOpcodeStr( value ) {
    var opcode_str = '';
    /**
     * check if id value exist
     */
    if( !document.getElementById( value ) )
        return;
    /**
     * crawl all opcodes and channel
     */
    for( var i = 0;; i++ ) {
        /**
         * check if opcode id exist
         */
        if ( document.getElementById( "opcode_" + i ) )
            opcode_str += document.getElementById( "opcode_" + i ).value;
        else
            break;
        /**
         * check if channel id exist
         */
        if ( document.getElementById( "channel_" + i ) )
            opcode_str += document.getElementById( "channel_" + i ).value;
        else
            break;
        /**
         * check opcode and this channel option
         */
        check_opcode_and_options( "opcode_" + i, "channel_" + i );
    }
    /**
     * puch opcode string out
     */
    document.getElementById( value ).value = opcode_str;
    console.log( opcode_str );

    if( document.getElementById( "realtime_edit" ) ) {
        if( document.getElementById( "realtime_edit" ).checked )
            SendSetting( value );
    }

}

// Asegúrate de que las funciones estén en el ámbito global
window.OScopeProbe = function() {
    let channelListStr = Array.from({ length: 13 }, (_, i) => {
        const channelElement = document.getElementById(`channel${i}`);
        return channelElement && channelElement.checked ? "1" : "0";
    }).join("");

    sendCMD(`OSC\\${channelListStr}`);
}

window.toggleDarkMode = function() {
    document.body.classList.toggle('dark-mode');
    updateCanvasColors();
}

const charts = {};

const channelNames = [
    'L1 Corriente', 'L1 Tensión', 'L1 Potencia', 'L1 Potencia Reactiva',
    'L2 Corriente', 'L2 Tensión', 'L2 Potencia', 'L2 Potencia Reactiva',
    'L3 Corriente', 'L3 Tensión', 'L3 Potencia', 'L3 Potencia Reactiva', 'Todas las potencias'
];

// Funciones de inicialización y actualización de gráficos
function createOrUpdateChart(chartId, datasets, numSamples, title, yLabel) {
    const canvas = document.getElementById(chartId);
    if (!canvas) {
        console.error(`Canvas con ID '${chartId}' no encontrado.`);
        return;
    }

    const ctx = canvas.getContext('2d');

    if (!charts[chartId]) {
        charts[chartId] = new Chart(ctx, {
            type: 'line',
            data: { datasets: datasets },
            options: {
                responsive: true,
                animation: false, // Desactiva animaciones para evitar saltos
                maintainAspectRatio: false,
                interaction: {
                    mode: 'index', // Permitir inspección en todos los puntos
                    intersect: false,
                },
                plugins: {
                    legend: { display: true, position: 'top' },
                    title: {
                        display: true,
                        text: title,
                        font: { size: 16 }
                    },
                    tooltip: {
                        callbacks: {
                            label: (tooltipItem) => `Valor: ${tooltipItem.raw.y.toFixed(2)}`
                        }
                    },
                    zoom: {
                        zoom: {
                            wheel: { enabled: true },
                            mode: 'y',
                            limits: { y: { min: -100, max: 1000 } },
                        },
                    }
                },
                scales: {
                    x: {
                        type: 'linear',
                        position: 'bottom',
                        title: { display: true, text: 'Muestras' },
                        min: 0,
                        max: numSamples
                    },
                    y: {
                        title: { display: true, text: yLabel },
                        beginAtZero: false,
                        min: -1000,
                        suggestedMin: -1000,
                        suggestedMax: 1000,
                        ticks: {
                            callback: function (value) {
                                return value.toFixed(1);
                            }
                        }
                    }
                }
            }
        });
    } else {
        charts[chartId].data.datasets = datasets;
        charts[chartId].options.scales.x.min = 0;
        charts[chartId].options.scales.x.max = numSamples;
        charts[chartId].update();
    }
}

function GotOScope(data) {
    const parts = data.split('\\');
    if (parts[0] !== 'OScopeProbe') return;

    const numChannels = parseInt(parts[1]);
    const numSamples = parseInt(parts[2]);
    const fftSamples = parseInt(parts[3]);
    const channelData = parts[5];
    const fftData = parts[6];
    const activeChannels = decodeActiveChannels(parts[7]);

    const oscilloscopeDatasets = prepareOscilloscopeData(activeChannels, numSamples, channelData);
    const fftDatasets = prepareFFTData(activeChannels, fftSamples, fftData);

    // Actualizar gráficos
    createOrUpdateChart('oscilloscopeChart', oscilloscopeDatasets, numSamples, 'Osciloscopio', 'Amplitud');
    createOrUpdateChart('fftChart', fftDatasets, fftSamples, 'FFT', 'Magnitud');
}

function decodeActiveChannels(activeChannelsHex) {
    const binaryString = parseInt(activeChannelsHex, 16).toString(2).padStart(13, '0');
    const activeChannels = [];
    for (let i = 0; i < binaryString.length; i++) {
        if (binaryString[i] === '1') activeChannels.push(i);
    }
    return activeChannels;
}

function prepareOscilloscopeData(activeChannels, numSamples, data) {
    const datasets = [];


    activeChannels.forEach((channelIndex, activeIndex) => {
        const values = [];

                // Determinar el factor de ajuste basado en el channelIndex
                switch (true) {
                    case [0, 4, 8].includes(channelIndex):
                        factor = 1.5;
                        break;
                    case [1, 5, 9].includes(channelIndex):
                        factor = 0.89;
                        break;
                    case [2, 6, 10].includes(channelIndex):
                        factor = 3;
                        break;
                    case [3, 7, 11].includes(channelIndex):
                        factor = 4;
                        break;
                    default:
                        console.warn(`Canal ${channelIndex} no tiene un factor definido. Usando 1.`);
                }

        for (let i = 0; i < numSamples; i++) {
            const offsetPos = (numSamples * activeIndex + i) * 3;
            const hexValue = data.substr(offsetPos, 3);
            const adcValue = parseInt(hexValue, 16);
            const adjustedValue = (adcValue - 2048) * factor;

            values.push({ x: i, y: adjustedValue });
        }

        datasets.push({
            label: `${channelNames[channelIndex]}`,
            data: values,
            borderColor: getColor(channelIndex),
            fill: false,
            pointRadius: 2,
            tension: 0.1
        });
    });

    //console.log('Datasets preparados:', datasets);
    return datasets;
}

function prepareFFTData(activeChannels, fftSamples, data) {
    const datasets = [];
    activeChannels.forEach((channelIndex, activeIndex) => {
        const values = [];
        // Determinar el factor de ajuste basado en el channelIndex
        switch (true) {
            case [0, 4, 8].includes(channelIndex):
                factor = 1.5;
                break;
            case [1, 5, 9].includes(channelIndex):
                factor = 1;
                break;
            case [2, 6, 10].includes(channelIndex):
                factor = 3;
                break;
            case [3, 7, 11].includes(channelIndex):
                factor = 4;
                break;
            default:
                console.warn(`Canal ${channelIndex} no tiene un factor definido. Usando 1.`);
        }

        for (let i = 0; i < fftSamples; i++) {
            const offset = (fftSamples * activeIndex + i) * 3;
            const hexValue = data.substr(offset, 3);
            //const value = parseInt(hexValue, 16) / 1024 * 100;
            const value = (parseInt(hexValue, 16) / 1024 )* 1000 * factor;
            values.push({ x: i, y: value });
        }
        datasets.push({
            label: `${channelNames[channelIndex]} FFT`,
            data: values,
            borderColor: getColor(channelIndex),
            fill: false,
            pointRadius: 2,
            tension: 0.1
        });
    });
    console.log('Datasets FFT preparados:', datasets);

    return datasets;
}

function getColor(index) {
    const colors = ['#FF5733', '#3498DB', '#2ECC71', '#F39C12', '#C0392B', '#2980B9', '#27AE60'];
    return colors[index % colors.length];
}

// Función para pausar/reanudar el osciloscopio
function ToggleOScopePause() {
    pause_osc = !pause_osc;
    if (!pause_osc) OScopeProbe();
}

function PhaseshiftPlus() {
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
