// Variables globales para el control del osciloscopio y la conexión
var pause_osc = true; // Indica si el osciloscopio está pausado
var connect = true; // Estado inicial de conexión
var timeout = 3; // Temporizador para control de conexiones
var savecounter = 0; // Contador de guardados (si aplica)
var connection; // WebSocket para manejar la conexión
let fftChartData = []; // Datos del gráfico FFT

document.addEventListener("DOMContentLoaded", function () {
    // Configura la conexión al servidor
    getconnect();

    // Actualizar estado cada 500 ms si la conexión está activa
    setInterval(function () {
        if (connection && connection.readyState === WebSocket.OPEN) {
            getStatus(); // Actualiza el estado del sistema
            if (!pause_osc) {
                OScopeProbe(); // Obtiene datos del osciloscopio si no está pausado
            }
        }
    }, 1000);

    // Restaurar modo oscuro desde localStorage
    if (localStorage.getItem("dark-Mode") === "true") {
        document.body.classList.add("dark-mode");
    }

    // Evento para el botón de menú hamburguesa
    document.getElementById("menu-button").addEventListener("click", function () {
        document.querySelector("#cssmenu ul").classList.toggle("open");
    });

    // Evento para el botón de modo oscuro
    document.getElementById("toggleDarkMode").addEventListener("click", function () {
        document.body.classList.toggle("dark-mode");
        localStorage.setItem("dark-Mode", document.body.classList.contains("dark-mode"));
    });
});

function getconnect() {
  // Crear WebSocket para la conexión con el servidor
  connection = new WebSocket("ws://" + location.hostname + "/ws", ["arduino"]);

  // Cuando la conexión está abierta
  connection.onopen = function () {
    connect = true; // Marca la conexión como activa
    sendCMD("STA"); // Envía un comando inicial al servidor

    // Solicita configuraciones según los elementos presentes en el DOM
    if (document.getElementById("OScope")) sendCMD("get_channel_list");
    if (document.getElementById("measurement_settings")) {
      sendCMD("get_measurement_settings");
      sendCMD("get_channel_config");
    }
    if (document.getElementById("wlan_settings")) sendCMD("get_wlan_settings");
    if (document.getElementById("group_settings"))
      sendCMD("get_group_settings");
    if (document.getElementById("mqtt_settings")) sendCMD("get_mqtt_settings");
    if (document.getElementById("hostname_settings"))
      sendCMD("get_hostname_settings");
    if (document.getElementById("display_settings"))
      sendCMD("get_display_settings");
    if (document.getElementById("ioport_settings"))
      sendCMD("get_ioport_settings");
  };

  // Cuando se recibe un mensaje del servidor
  connection.onmessage = function (e) {
    console.log("Mensaje WebSocket recibido:", e.data);
    partsarry = e.data.split("\\"); // Divide el mensaje recibido en partes

    if (partsarry[0] == "OScopeProbe") {
      //console.time("GotOScope Execution");
      window.GotOScope(e.data); // Procesa los datos del osciloscopio
      //console.timeEnd("GotOScope Execution");
      return;
    }
    if (partsarry[0] === "status") {


      // Extraer la frecuencia
      const matchFrequency = partsarry[1].match(/f\s*=\s*([\d.]+)\s*Hz/); // Busca "f = 50.3 Hz"
      const frequency = matchFrequency ? `${matchFrequency[1]} Hz` : "Frecuencia desconocida";

      // Separar los grupos (L1, L2, L3)
      const groups = partsarry[1].split(/\r\n/).filter((line) => line.trim().startsWith("L"));

      // Referencia al contenedor de la tabla
      const fixedFooter = document.getElementById("fixedfooter");
      if (!fixedFooter) {
        console.warn("No se encontró el elemento 'fixedfooter'");
        return;
      }

      // Verificar si la tabla ya existe; si no, crearla
      let table = document.getElementById("statusTable");
      if (!table) {
        table = document.createElement("table");
        table.id = "statusTable";
        table.style.width = "100%";
        table.style.textAlign = "center";
        table.style.borderCollapse = "collapse";
        table.innerHTML = `
              <thead>
                  <tr>
                      <th>Grupo</th>
                      <th>RMS (I)</th>
                      <th>I3</th>
                      <th>I5</th>
                      <th>I7</th>
                      <th>I9</th>
                      <th>THDI</th>
                      <th>RMS (U)</th>
                      <th>U3</th>
                      <th>U5</th>
                      <th>U7</th>
                      <th>U9</th>
                      <th>THD</th>
                      <th>P</th>
                      <th>Pvar</th>
                      <th>Cos Φ</th>
                  </tr>
              </thead>
              <tbody></tbody>
          `;
        fixedFooter.innerHTML = `<div><b>Estado General:</b> Online</div>`;
        fixedFooter.appendChild(table);
      }

      // Actualizar las filas de datos
      const tbody = table.querySelector("tbody");
      tbody.innerHTML = ""; // Limpiar filas anteriores

      groups.forEach((group) => {
        const matchGroup = group.match(
            /L(\d+):\[.*?I=([\d.]+)A.*?I3=([\d.]+)A.*?I5=([\d.]+)A.*?I7=([\d.]+)A.*?I9=([\d.]+)A.*?THDI=([\d.]+).*?U=([\d.]+)V.*?U3=([\d.]+)V.*?U5=([\d.]+)V.*?U7=([\d.]+)V.*?U9=([\d.]+)V.*?THDV=([\d.]+).*?P=([\d.]+)kW.*?Pvar=([\d.-]+)kVAr.*?Cos=([\d.]+)/
        );
    
        if (matchGroup) {
            const [
                ,
                groupId,
                rmsCurrent,
                i3,
                i5,
                i7,
                i9,
                thdi,          // Nuevo campo THDI
                rmsVoltage,
                u3,
                u5,
                u7,
                u9,
                thdv,
                p,
                pvar,
                cosPhi,
            ] = matchGroup;
    
            // Determinar el canal basado en el grupo
            const currentChannel = groupId === "1" ? 0 : groupId === "2" ? 4 : 8;
            const voltageChannel = groupId === "1" ? 1 : groupId === "2" ? 5 : 9;
    
            // Aplicar escalamiento a los valores de corriente y voltaje
            const scaledRMSCurrent = (parseFloat(rmsCurrent) * getScalingFactorFoot(currentChannel)).toFixed(2) + "A";
            const scaledI3 = (parseFloat(i3) * getScalingFactorFoot(currentChannel)).toFixed(2) + "A";
            const scaledI5 = (parseFloat(i5) * getScalingFactorFoot(currentChannel)).toFixed(2) + "A";
            const scaledI7 = (parseFloat(i7) * getScalingFactorFoot(currentChannel)).toFixed(2) + "A";
            const scaledI9 = (parseFloat(i9) * getScalingFactorFoot(currentChannel)).toFixed(2) + "A";
    
            const scaledRMSVoltage = (parseFloat(rmsVoltage) * getScalingFactorFoot(voltageChannel)).toFixed(2) + "V";
            const scaledU3 = (parseFloat(u3) * getScalingFactorFoot(voltageChannel)).toFixed(2) + "V";
            const scaledU5 = (parseFloat(u5) * getScalingFactorFoot(voltageChannel)).toFixed(2) + "V";
            const scaledU7 = (parseFloat(u7) * getScalingFactorFoot(voltageChannel)).toFixed(2) + "V";
            const scaledU9 = (parseFloat(u9) * getScalingFactorFoot(voltageChannel)).toFixed(2) + "V";
    
            // Generar la fila con valores escalados
            const row = `
                <tr>
                    <td>L${groupId}</td>
                    <td>${scaledRMSCurrent}</td>
                    <td>${scaledI3}</td>
                    <td>${scaledI5}</td>
                    <td>${scaledI7}</td>
                    <td>${scaledI9}</td>
                    <td>${thdi}</td>
                    <td>${scaledRMSVoltage}</td>
                    <td>${scaledU3}</td>
                    <td>${scaledU5}</td>
                    <td>${scaledU7}</td>
                    <td>${scaledU9}</td>
                    <td>${thdv}</td>
                    <td>${p} kW</td>
                    <td>${pvar} kVAr</td>
                    <td>${cosPhi}</td>
                </tr>
            `;
    
            tbody.innerHTML += row;
        }
    });
    

      // Agregar frecuencia como pie de página
      let footer = document.getElementById("statusFooter");
      if (!footer) {
        footer = document.createElement("div");
        footer.id = "statusFooter";
        fixedFooter.appendChild(footer);
      }
      footer.innerHTML = `<b>Frecuencia:</b> ${frequency}`;
    }

    // Actualizaciones específicas de elementos en el DOM
    if (partsarry[0] == "get_channel_list") {
      const label = document.getElementById(partsarry[1]);
      if (label) label.textContent = partsarry[2];
      return;
    }

    if (partsarry[0] == "get_channel_use_list") {
      const checkbox = document.getElementById(partsarry[1]);
      if (checkbox) checkbox.checked = partsarry[2] == "true";
      const select = document.getElementById(partsarry[3]);
      if (select && select.options[partsarry[4]]) {
        select.options[partsarry[4]].text = partsarry[5];
      }
      return;
    }

    if (partsarry[0] == "get_group_use_list") {
      const checkbox = document.getElementById(partsarry[1]);
      if (checkbox && partsarry[2] == "true") checkbox.checked = true;
      const label = document.getElementById(partsarry[3]);
      if (label) label.textContent = partsarry[4];
      return;
    }

    if (partsarry[0] == "option") {
      const select = document.getElementById(partsarry[1]);
      if (select && select.options[partsarry[2]]) {
        select.options[partsarry[2]].text = partsarry[3];
        refreshOpcode("channel_opcodeseq_str");
      }
      return;
    }

    if (partsarry[0] == "label") {
      const label = document.getElementById(partsarry[1]);
      if (label) label.textContent = partsarry[2];
      return;
    }

    if (partsarry[0] == "checkbox") {
      const checkbox = document.getElementById(partsarry[1]);
      if (checkbox) checkbox.checked = partsarry[2] == "true";
      return;
    }

    const element = document.getElementById(partsarry[0]);
    if (element) {
      element.value = partsarry[1];
      if (document.getElementById("channel_opcodeseq_str")) {
        refreshOpcode("channel_opcodeseq_str");
      }
    }
  };

  // Manejo del cierre del WebSocket
  connection.onclose = function (e) {
    console.log(
      "Socket is closed. Reconnect will be attempted in 1 second.",
      e.reason
    );
    connect = false; // Marca la conexión como inactiva
    setTimeout(getconnect, 1000); // Reintenta la conexión
  };

  // Manejo de errores del WebSocket
  connection.onerror = function (error) {
    connect = false;
    console.log("WebSocket Error ", error);
    connection.close(); // Cierra la conexión en caso de error
  };
}

function setSensor(sensor) {
  console.log("Server: ", sensor);
  const partsarry = sensor.split("\\"); // Asegurar uso de `const` para valores inmutables

  if (partsarry.length < 4) {
    console.error("Datos incompletos del sensor:", partsarry);
    return;
  }

  const ratioElem = document.getElementById("channel_ratio");
  const phaseShiftElem = document.getElementById("channel_phaseshift");
  const offsetElem = document.getElementById("channel_offset");
  const trueRmsElem = document.getElementById("channel_true_rms");
  //const escalaOscElem = document.getElementById("channel_escala_osc");
  //const escalaFoot = document.getElementById("channel_escala_foot");

  if (ratioElem) ratioElem.value = partsarry[0];
  if (phaseShiftElem) phaseShiftElem.value = partsarry[1];
  if (offsetElem) offsetElem.value = partsarry[2];
  if (trueRmsElem) trueRmsElem.checked = partsarry[3] === "true";
  //if (escalaOscElem) escalaOscElem.value = partsarry[4];
  //if (escalaFoot) escalaFoot.value = partsarry[5];

  //updateScaleFactors(escalaOscElem); // Guardar el factor de escala

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
    5: [
      "ADC1-CH5 GPIO33",
      "ADC1-CH6 GPIO34",
      "ADC1-CH7 GPIO35",
      "ADC1-CH0 GPIO36 (VP)",
      "ADC1-CH3 GPIO39 (VN)",
      "ADC1-CH4 GPIO32",
      "not available",
      "not available",
      "not available",
      "not available",
      "not available",
      "not available",
      "not available",
    ],
    6: Array.from({ length: 13 }, (_, i) => i.toString()),
    7: [
      "level 0",
      "level 1",
      "level 2",
      "level 3",
      "level 4",
      "level 5",
      "not available",
      "not available",
      "not available",
      "not available",
      "not available",
      "not available",
      "not available",
    ],
    default: Array(13).fill("-"),
  };

  if (["1", "2", "3", "8", "9", "b", "c"].includes(value)) {
    //  if (["1", "2", "3", "8", "c", "9", "b"].includes(value)) {
    // Copiar opciones del elemento "channel"
    for (let i = 0; i < 13; i++) {
      selectElement.options[i].text =
        document.getElementById("channel").options[i].text;
    }
  } else {
    const options = optionSets[value] || optionSets["default"];
    for (let i = 0; i < options.length; i++) {
      selectElement.options[i].text = options[i];
    }
  }
}

// Enviar configuraciones específicas al servidor
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

// Crear y enviar configuraciones de grupos de canales
function send_channel_groups() {
  var channel_group_str = "channel_group\\";

  for (var channel = 0; ; channel++) {
    if (!document.getElementById("channel" + channel + "_0")) break;

    for (var group = 0; ; group++) {
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

if (timeout < 0) {
    var statusElem = document.getElementById("status");
    if (statusElem) {
      document.getElementById("fixedfooter").style.background = "#800000";
      statusElem.firstChild.nodeValue = "offline ... Esperando conexión";
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

  let opcode_str = "";
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

window.OScopeProbe = function () {
  const channelListStr = Array.from({ length: 13 }).reduce((acc, _, i) => {
    const channelElement = document.getElementById(`channel${i}`);
    return acc + (channelElement && channelElement.checked ? "1" : "0");
  }, "");
  sendCMD(`OSC\\${channelListStr}`);
};

function toggleDarkMode() {
  document.body.classList.toggle("dark-mode");
  localStorage.setItem("darkMode", document.body.classList.contains("dark-mode"));
}

document.addEventListener("DOMContentLoaded", () => {
  if (localStorage.getItem("darkMode") === "true") {
      document.body.classList.add("dark-mode");
  }

  document.getElementById("menu-button").addEventListener("click", function () {
      document.querySelector("#cssmenu ul").classList.toggle("open");
  });
});

const channelNames = [
  "L1 Corriente",
  "L1 Tensión",
  "L1 Potencia",
  "L1 Potencia Reactiva",
  "L2 Corriente",
  "L2 Tensión",
  "L2 Potencia",
  "L2 Potencia Reactiva",
  "L3 Corriente",
  "L3 Tensión",
  "L3 Potencia",
  "L3 Potencia Reactiva",
  "Todas las potencias",
];

let isUpdating = false; // Bandera global

function GotOScope(data) {
  console.log("GotOScope:", data);
  if (isUpdating) {
    console.warn(
      "Actualización en curso. Ignorando nueva llamada a GotOScope."
    );
    return;
  }

  isUpdating = true; // Bloquea nuevas actualizaciones mientras se procesa
  try {
    // Dividir el mensaje del WebSocket
    const parts = data.split("\\");

    const numSamples = parseInt(parts[1], 10); // Número de muestras del osciloscopio
    const activeChannelsHex = parts[4]; // Hexadecimal de canales activos
    const rawData = parts[2]; // Datos en formato hexadecimal (Osciloscopio)
    const exp = parts[3]; //exponente del canal

    // Decodificar canales activos desde la parte correcta
    //console.log("Hexadecimal recibido para canales activos:", activeChannelsHex);
    const activeChannels = decodeActiveChannels(activeChannelsHex);
    //console.log("Canales activos decodificados:", activeChannels);

    // Procesar datos del osciloscopio
    const oscilloscopeData = preprocessData(
      rawData,
      numSamples,
      activeChannels,
      exp
    );

    // Procesar datos FFT
    const fftProcessedData = prepareFFTData(
      activeChannels
    );

    // Gráfico de osciloscopio (línea)
    createOrUpdateChart(oscilloscopeData, "Osciloscopio", numSamples, "Gráfico Osciloscopio", "Amplitud");

    // Gráfico de FFT (barras)
    createOrUpdateChart(fftProcessedData, "Armonicos",32, "Gráfico Armonicos", "Amplitud", true);

  } catch (error) {
    console.error("Error al procesar GotOScope:", error);
  } finally {
    isUpdating = false; // Libera la bandera
  }
}



function decodeActiveChannels(activeChannelsHex) {
  // Convertir hexadecimal a binario y rellenar hasta 13 bits
  const binaryString = parseInt(activeChannelsHex, 16).toString(2).padStart(13, '0');

  // Crear un array con los índices de los canales activos
  const activeChannels = binaryString
    .split('') // Dividir en bits individuales
    .map((bit, index) => (bit === '1' ? index : null)) // Mapear bits activos ('1') a sus índices
    .filter(index => index !== null); // Filtrar solo los canales activos


  return activeChannels;
}

// Objeto global para almacenar las instancias de los gráficos
const charts = {};
function preprocessData(rawData, numSamples, activeChannels, exp) {
  if (!rawData || activeChannels.length === 0) {
      console.warn("No hay datos o no hay canales activos.");
      return [];
  }

  const datasets = [];

  activeChannels.forEach((channelIndex, channelPos) => {
      const values = [];
      const offsetBase = channelPos * numSamples;
      //const scaleFactor = getChannelScaleFactor(channelIndex);


      for (let i = 0; i < numSamples; i++) {
          const offset = (offsetBase + i) * 3;
          const hexValue = rawData.slice(offset, offset + 3);
          const adcValue = parseInt(hexValue, 16);

          if (isNaN(adcValue)) continue;

          // Ajustar la referencia de 2048 y aplicar la escala
          let adjustedValue = (adcValue - 2048) * getScalingFactor(channelIndex);// * scaleFactor;
          //console.log(`Canal ${channelIndex} - Factor de escala: ${scaleFactor}`);


          values.push({ x: i, y: adjustedValue });
      }

      // Aplicar interpolación y suavizado
      let interpolatedValues = interpolateData(values);
      interpolatedValues = smoothData(interpolatedValues, 5);

      datasets.push({
          label: `Canal ${channelIndex}`,
          data: interpolatedValues,
          borderColor: getColor(channelIndex),
          fill: false,
          pointRadius: 0,
          tension: 0.2,
      });
  });

  return datasets;
}

/*const channelScaleFactors = {}; // Objeto para almacenar factores de escala

function getChannelScaleFactor(channelIndex) {
    return channelScaleFactors[channelIndex] || 1; // Si no hay valor, devuelve 1 (sin ajuste)
}

// Esta función actualiza los factores de escala desde el ESP
function updateScaleFactors(sensorData) {
    console.log("Actualizando factores de escala desde el sensor:", sensorData);
    const parts = sensorData.split("\\");
    if (parts.length < 5) return;

    const channelIndex = parseInt(parts[0], 10);
    const scaleFactor = parseFloat(parts[4]);

    if (!isNaN(channelIndex) && !isNaN(scaleFactor)) {
        channelScaleFactors[channelIndex] = scaleFactor;
    }
    console.log("Escalas actualizadas:", channelScaleFactors);

}
*/

// Función de interpolación lineal para suavizar la señal
function interpolateData(data) {
  if (data.length < 2) return data;

  const interpolatedData = [];
  for (let i = 0; i < data.length - 1; i++) {
      interpolatedData.push(data[i]);
      const midX = (data[i].x + data[i + 1].x) / 2;
      const midY = (data[i].y + data[i + 1].y) / 2;
      interpolatedData.push({ x: midX, y: midY });
  }
  interpolatedData.push(data[data.length - 1]);

  return interpolatedData;
}

// Filtro de media móvil para suavizar la señal
function smoothData(data, windowSize) {
  if (data.length < windowSize) return data;

  const smoothedData = [];
  for (let i = 0; i < data.length; i++) {
      let sum = 0;
      let count = 0;

      for (let j = -Math.floor(windowSize / 2); j <= Math.floor(windowSize / 2); j++) {
          const index = i + j;
          if (index >= 0 && index < data.length) {
              sum += data[index].y;
              count++;
          }
      }

      smoothedData.push({ x: data[i].x, y: sum / count });
  }

  return smoothedData;
}

function getScalingFactorFoot(channelIndex) {
  // Factores de escala según el canal, estos valores deben coincidir con los usados en el hardware
  const scaleMap = {
      0: 0.098,  // Escalar corriente L1
      1: 1.302,  // Escalar voltaje L1
      4: 0.122,  // Escalar corriente L2
      5: 1.286,  // Escalar voltaje L2
      8: 0.125,  // Escalar corriente L3
      9: 1.325   // Escalar voltaje L3
  };

  return scaleMap[channelIndex] || 1; // Si el canal no está definido, usar 1 (sin ajuste)
}


function getScalingFactor(channelIndex) {
  // Factores de escala según el canal, estos valores deben coincidir con los usados en el hardware
  const scaleMap = {
      0: 0.0933 ,  // Escalar corriente L1    si disminuyo el valor de abajo, aumenta la corriente
      1: 1.11,  // Escalar voltaje L1
      4: 0.115,  // Escalar corriente L2
      5: 1.2,  // Escalar voltaje L2
      8: 0.123,  // Escalar corriente L3
      9: 1.217   // Escalar voltaje L3
  };

  return scaleMap[channelIndex] || 1; // Si el canal no está definido, usar 1 (sin ajuste)
}


function prepareFFTData(activeChannels) {

  const datasets = [];
  const allowedChannels = [0, 1, 4, 5, 8, 9]; // Canales permitidos
  const harmonics = [3, 9, 15, 21, 27]; // Índices de los armónicos deseados
  const channelOffsets = {}; // Mapeo de offsets fijos por canal
  let shouldZeroOut = true; // Comienza en true, solo se desactiva si encontramos señal válida

  // Obtener los valores desde la tabla "statusTable"
  const table = document.getElementById("statusTable");
  if (!table) {
    console.warn("Tabla de estado no encontrada. No se pueden obtener los datos.");
    return [];
  }

  const rows = table.querySelectorAll("tbody tr");
  const groupData = {};

  // Mapear cada grupo a sus valores
  rows.forEach((row) => {
    const cells = row.querySelectorAll("td");
    if (cells.length < 15) return;

    const group = cells[0].innerText.trim();
    groupData[group] = {
      I1: parseFloat(cells[1].innerText),
      I3: parseFloat(cells[2].innerText),
      I5: parseFloat(cells[3].innerText),
      I7: parseFloat(cells[4].innerText),
      I9: parseFloat(cells[5].innerText),
      U1: parseFloat(cells[6].innerText),
      U3: parseFloat(cells[7].innerText),
      U5: parseFloat(cells[8].innerText),
      U7: parseFloat(cells[9].innerText),
      U9: parseFloat(cells[10].innerText),
    };
  });

  // Asignar los valores de la tabla según el canal
  const channelToGroupMap = {
    0: { group: "L1", keys: ["I1", "I3", "I5", "I7", "I9"] },
    1: { group: "L1", keys: ["U1", "U3", "U5", "U7", "U9"] },
    4: { group: "L2", keys: ["I1", "I3", "I5", "I7", "I9"] },
    5: { group: "L2", keys: ["U1", "U3", "U5", "U7", "U9"] },
    8: { group: "L3", keys: ["I1", "I3", "I5", "I7", "I9"] },
    9: { group: "L3", keys: ["U1", "U3", "U5", "U7", "U9"] },
  };

  // Procesar los datos de cada canal permitido
  activeChannels.forEach((channelIndex) => {
    if (!allowedChannels.includes(channelIndex)) return;

    const channelInfo = channelToGroupMap[channelIndex];
    if (!channelInfo || !groupData[channelInfo.group]) return;

    const values = channelInfo.keys.map((key) => groupData[channelInfo.group][key] || 0);

    datasets.push({
      label: `Canal ${channelIndex}`,
      data: values,
      backgroundColor: getColor(channelIndex),
    });
  });

  return datasets;
}

function determineYAxisScale(activeChannels, isFFT = false) {
  // Canales que requieren escala -1 a 1
  const smallScaleChannels = [0, 4, 8];
  // Canales que requieren escala -400 a 400
  const mediumScaleChannels = [1, 5, 9];
  // Escala máxima -600 a 600 para osciloscopio
  const maxScale = { min: -600, max: 600 };

  if (isFFT) {
      // Para FFT, los valores son siempre positivos (sin negativo)
      let maxY = 600; // Valor por defecto
      if (activeChannels.length === 1) {
          if (smallScaleChannels.includes(activeChannels[0])) return { min: 0, max: 1 };
          if (mediumScaleChannels.includes(activeChannels[0])) return { min: 0, max: 400 };
      }
      if (activeChannels.some(ch => smallScaleChannels.includes(ch)) && !activeChannels.some(ch => mediumScaleChannels.includes(ch))) {
          return { min: 0, max: 1 };
      }
      if (activeChannels.some(ch => mediumScaleChannels.includes(ch)) && !activeChannels.some(ch => smallScaleChannels.includes(ch))) {
          return { min: 0, max: 400 };
      }
      return { min: 0, max: maxY }; // Para múltiples canales, usar escala máxima
  } else {
      let minY = 0, maxY = 0;
      if (activeChannels.length === 1) {
          if (smallScaleChannels.includes(activeChannels[0])) return { min: -1, max: 1 };
          if (mediumScaleChannels.includes(activeChannels[0])) return { min: -400, max: 400 };
      }
      if (activeChannels.some(ch => smallScaleChannels.includes(ch)) && !activeChannels.some(ch => mediumScaleChannels.includes(ch))) {
          return { min: -1, max: 1 };
      }
      if (activeChannels.some(ch => mediumScaleChannels.includes(ch)) && !activeChannels.some(ch => smallScaleChannels.includes(ch))) {
          return { min: -400, max: 400 };
      }
      return maxScale;
  }
}

// Modificar la función para que aplique la escala correcta según si es FFT o no
function createOrUpdateChart(datasets, chartId, numSamples, title, yLabel, isFFT = false) {
    console.log(`Actualizando gráfico ${chartId} con datos:`, datasets);

    let chart = charts[chartId];
    let activeChannels = datasets.map(ds => parseInt(ds.label.split(" ")[1])); // Extraer números de canales
    let yScaleLimits = determineYAxisScale(activeChannels, isFFT);

    if (chart) {
        // Guardar los valores actuales del zoom en Y
        let currentYMin = chart.scales.y.min;
        let currentYMax = chart.scales.y.max;

        // Detectar si hay un canal nuevo activado
        let previousActiveChannels = chart.activeChannels || [];
        let isNewChannelActivated = JSON.stringify(previousActiveChannels) !== JSON.stringify(activeChannels);
        chart.activeChannels = activeChannels; // Guardar los canales actuales para la próxima actualización

        // Actualizar datos
        chart.data.datasets = datasets;

        // Si se activó un canal nuevo, aplicar la escala automática
        if (isNewChannelActivated) {
            chart.options.scales.y.min = yScaleLimits.min;
            chart.options.scales.y.max = yScaleLimits.max;
        } else {
            // Mantener el zoom actual del usuario en Y
            chart.options.scales.y.min = currentYMin;
            chart.options.scales.y.max = currentYMax;
        }

        chart.update();
        return;
    }

    const canvas = document.getElementById(chartId);
    if (!canvas) {
        console.error(`Canvas con ID '${chartId}' no encontrado.`);
        return;
    }

    const ctx = canvas.getContext("2d");

    charts[chartId] = new Chart(ctx, {
        type: isFFT ? 'bar' : 'line',
        data: {
            labels: isFFT ? ["1º", "3º", "5º", "7º", "9º"] : undefined,
            datasets: datasets,
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: false,
            plugins: {
                legend: { display: true },
                title: { display: true, text: title },
                tooltip: {
                    enabled: true,
                    backgroundColor: "rgba(0, 0, 0, 0.8)",
                    titleFont: { size: 14, weight: "bold" },
                    bodyFont: { size: 12 },
                    padding: 10,
                    cornerRadius: 5,
                    displayColors: true,
                    callbacks: {
                        label: function (tooltipItem) {
                            let datasetLabel = tooltipItem.dataset.label || "";
                            let value = tooltipItem.raw;
                            if (typeof value === "object" && value !== null && "y" in value) {
                                value = value.y;
                            }
                            return `${datasetLabel}: ${value.toFixed(2)}`;
                        },
                        title: function (tooltipItems) {
                            return isFFT ? `Armónico ${tooltipItems[0].label}` : `Muestra ${tooltipItems[0].parsed.x}`;
                        }
                    }
                },
                zoom: {
                    zoom: {
                        drag: { enabled: true }, // Permite hacer zoom con arrastre
                        mode: 'y', // 🔥 Solo zoom en Y
                        wheel: { enabled: true }, // Habilita zoom con la rueda del mouse
                    },
                    pan: {
                        enabled: true,
                        mode: 'y', // 🔥 Solo pan en Y
                    },
                },
            },
            scales: {
                x: { 
                    type: isFFT ? 'category' : 'linear', 
                    title: { display: true, text: isFFT ? 'Armónicos' : 'Muestras' } 
                },
                y: { 
                    title: { display: true, text: yLabel },
                    min: yScaleLimits.min, 
                    max: yScaleLimits.max 
                },
            },
            interaction: { mode: "index", intersect: false },
            elements: { point: { radius: 3, hitRadius: 10, hoverRadius: 6 } }
        },
    });

    // Guardar los canales activos iniciales
    charts[chartId].activeChannels = activeChannels;

    // Agregar después de la creación del chart en createOrUpdateChart
    if (!document.getElementById(`download-${chartId}`)) {
        const downloadButton = document.createElement('button');
        downloadButton.id = `download-${chartId}`;
        downloadButton.innerHTML = 'Descargar CSV';
        downloadButton.className = 'download-button';
        downloadButton.onclick = () => downloadCSV(chartId);
        
        // Agregar estilos al botón
        downloadButton.style.position = 'absolute';
        downloadButton.style.right = '10px';
        downloadButton.style.top = '10px';
        downloadButton.style.padding = '5px 10px';
        downloadButton.style.backgroundColor = '#4CAF50';
        downloadButton.style.color = 'white';
        downloadButton.style.border = 'none';
        downloadButton.style.borderRadius = '4px';
        downloadButton.style.cursor = 'pointer';
        
        // Agregar el botón junto al canvas
        const container = canvas.parentElement;
        container.style.position = 'relative';
        container.appendChild(downloadButton);
    }
}



// Actualizar gráficos
function updateChartAndCanvas(data, numSamples, chartId, yLabel) {
  console.log("ChartID", chartId)
  createOrUpdateChart(data, chartId, numSamples, chartId, yLabel);
}


// Dibujar líneas en el Canvas secundario
function drawOnCanvas(data, chart, channels) {
  const xScale = chart.scales.x;
  const yScale = chart.scales.y;

  // Ajustar dimensiones del Canvas secundario
  lineCanvas.width = chartCanvas.clientWidth;
  lineCanvas.height = chartCanvas.clientHeight;

  ctxLine.clearRect(0, 0, lineCanvas.width, lineCanvas.height);

  channels.forEach((channelIndex) => {
    const channelData = data[channelIndex];
    ctxLine.beginPath();
    ctxLine.strokeStyle = getColor(channelIndex); // Define un color para cada canal

    channelData.forEach((value, i) => {
      const x = xScale.getPixelForValue(i);
      const y = yScale.getPixelForValue(value);
      i === 0 ? ctxLine.moveTo(x, y) : ctxLine.lineTo(x, y);
    });

    ctxLine.stroke();
  });
}


function getColor(index) {
  const colors = [
    "#FF5733",
    "#3498DB",
    "#2ECC71",
    "#F39C12",
    "#C0392B",
    "#2980B9",
    "#27AE60",
  ];

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
  const button = document.getElementById("pauseOscButton");
  if (button) {
    button.textContent = pause_osc
      ? "Reanudar Osciloscopio"
      : "Pausar Osciloscopio";
  }

  // Reanudar si está despausado
  if (!pause_osc) OScopeProbe();
}

function downloadCSV(chartId) {
    const chart = charts[chartId];
    if (!chart) {
        console.error(`No se encontró el gráfico con ID: ${chartId}`);
        return;
    }

    let csvContent = "data:text/csv;charset=utf-8,";
    
    // Agregar encabezados
    const headers = ["Índice"];
    chart.data.datasets.forEach(dataset => {
        headers.push(dataset.label);
    });
    csvContent += headers.join(",") + "\n";

    // Determinar si es un gráfico FFT o de osciloscopio
    const isFFT = chart.config.type === 'bar';

    if (isFFT) {
        // Para gráfico FFT (datos de armónicos)
        const harmonics = ["1º", "3º", "5º", "7º", "9º"];
        harmonics.forEach((harmonic, idx) => {
            let row = [harmonic];
            chart.data.datasets.forEach(dataset => {
                row.push(dataset.data[idx]);
            });
            csvContent += row.join(",") + "\n";
        });
    } else {
        // Para gráfico de osciloscopio (datos de muestras)
        const maxLength = Math.max(...chart.data.datasets.map(ds => ds.data.length));
        for (let i = 0; i < maxLength; i++) {
            let row = [i];
            chart.data.datasets.forEach(dataset => {
                row.push(dataset.data[i]?.y || "");
            });
            csvContent += row.join(",") + "\n";
        }
    }

    // Crear y activar el enlace de descarga
    const encodedUri = encodeURI(csvContent);
    const link = document.createElement("a");
    link.setAttribute("href", encodedUri);
    link.setAttribute("download", `datos_${chartId}_${new Date().toISOString().slice(0,19).replace(/[:]/g, '-')}.csv`);
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
}
