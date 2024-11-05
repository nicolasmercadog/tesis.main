import csv
import paho.mqtt.client as mqtt
import time
import json

# Función para redondear valores numéricos a dos décimas
def round_to_decimal(value, decimals=2):
    try:
        return round(float(value), decimals)
    except ValueError:
        return value

# Nombre del archivo CSV
csv_file = 'mqtt_data.csv'

# Crear el archivo CSV si no existe y agregar encabezados (solo si el archivo está vacío)
with open(csv_file, mode='a', newline='') as file:
    writer = csv.writer(file, delimiter=';')
    if file.tell() == 0:
        writer.writerow([
            'Time', 'Frequency',
            'L1C [A]', 'L1V [V]',
            'L1P ', 'L1Q [kVar]',
            'L1CosPhi', 'L2C [A]', 'L2V [V]',
            'L2P [kW]', 'L2Q [kVar]',
            'L2CosPhi', 'L3C [A]', 'L3V [V]',
            'L3P [kW]', 'L3Q [kVar]',
            'L3CosPhi'
        ])

# Callback para cuando se establece la conexión con el broker
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Conectado al broker MQTT con éxito")
        client.subscribe('stat/medidorenergia/realtimepower')  # Suscribirse a todos los tópicos
    else:
        print(f"Error al conectar al broker MQTT. Código de error: {rc}")

# Callback para manejar los mensajes recibidos
def on_message(client, userdata, message):
    topic = message.topic
    payload = message.payload.decode()

    # Procesar el JSON del payload
    try:
        data = json.loads(payload)
        time_ = data.get('time', '')
        frequency = round_to_decimal(data.get('frequency', ''))

        # Extraer datos del grupo
        group = data.get('group', {})
        l1 = group.get('L1', {})
        l2 = group.get('L2', {})
        l3 = group.get('L3', {})

        # Crear filas para el CSV con redondeo a dos décimas
        row = [
            time_, frequency,
            round_to_decimal(l1.get('current', {}).get('value', '')),
            round_to_decimal(l1.get('voltage', {}).get('value', '')),
            round_to_decimal(l1.get('power', {}).get('value', '')),
            round_to_decimal(l1.get('reactive power', {}).get('value', '')),
            round_to_decimal(l1.get('cos_phi', '')),
            round_to_decimal(l2.get('current', {}).get('value', '')),
            round_to_decimal(l2.get('voltage', {}).get('value', '')),
            round_to_decimal(l2.get('power', {}).get('value', '')),
            round_to_decimal(l2.get('reactive power', {}).get('value', '')),
            round_to_decimal(l2.get('cos_phi', '')),
            round_to_decimal(l3.get('current', {}).get('value', '')),
            round_to_decimal(l3.get('voltage', {}).get('value', '')),
            round_to_decimal(l3.get('power', {}).get('value', '')),
            round_to_decimal(l3.get('reactive power', {}).get('value', '')),
            round_to_decimal(l3.get('cos_phi', ''))
        ]

        # Guardar los datos en el archivo CSV
        with open(csv_file, mode='a', newline='') as file:
            writer = csv.writer(file, delimiter=';')
            writer.writerow(row)
            print(f"Datos guardados en CSV: {row}")

    except json.JSONDecodeError as e:
        print(f"Error al procesar el JSON: {e}")

# Configuración del cliente MQTT
client = mqtt.Client()
client.on_connect = on_connect  # Asignar el callback de conexión
client.on_message = on_message   # Asignar el callback de mensajes

# Conectar al broker público mqtthq
print("Conectando al broker MQTT...")
client.connect('public.mqtthq.com', 1883)

# Empezar a escuchar
client.loop_forever()
