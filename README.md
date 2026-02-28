# ESP32-X2-relay
[![Apache-2.0 license](https://img.shields.io/badge/License-Apache_2.0-green.svg?style=flat-square)](https://www.apache.org/licenses/LICENSE-2.0) 
[![ESP32](https://img.shields.io/badge/ESP-32-green.svg?style=flat-square)](https://www.espressif.com/en/products/socs/esp32)

Firmware to control a dual-channel relay module using an ESP32 — remote or automated switching for AC/DC devices.

<p align="center">
<img src="https://github.com/user-attachments/assets/72864717-ebb3-4218-aff5-bdfb89d969ad" width="408" height="407">
</p>

The board is shipped without documentation. This project provides firmware and usage guidelines to control the board's two relays, the status LED, and the push button via a REST API and MQTT. 

A short guide for integration with Home Assistant is included.


## USB connection
Some board specs are available [here](https://devices.esphome.io/devices/esp32-relay-x2/).

The board does not include a USB connector; it provides a 6-pin header for an external USB-to-TTL serial adapter. Connect GND and +5V between the board and the adapter, and cross TX/RX. The remaining two pins are GND and IO0 — hold IO0 low while flashing.


## Environment settings
The source is in C language. 

The execution environment is FreeRTOS OS.

Edit `config.h` to set your Wi‑Fi credentials and MQTT broker information.

Example (in `config.h`):

```c
// WiFi
#define WIFI_SSID       "YOUR_SSID"
#define WIFI_PASSWORD   "THE_PASSWORD_OF_YOUR_SSID"
```

The SW is expecting an MQTT broker. So you need to overwrite the IP address and Port of your MQTT server.
```c
// MQTT broker
#define MQTT_HOST_IP_ADDR "192.168.1.2"
#define MQTT_HOST_IP_PORT 1883 // default MQTT port
```

And if you want, modify the MQTT device name.
```c
#define DEVICE_MQTT_NAME	"relayX2board"
```

# Building & flashing
You need the ESP-IDF development environment installed. I am running ESP-IDF v5.4-dev-2194-gd7ca8b94c8 for Linux on a Raspberry Pi 3.

Go to ["Installation of ESP-IDF and Tools on Linux"](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/linux-setup.html) if you need to install it.


Create a project directory (e.g. ./relayX2board), download the files from Github and follow the regular process to generate and flash the SW as described in the ESP-IDF instructions ["Configure Your Projec"](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/linux-macos-setup.html#configure-your-project).


Run the ESP-IDF script to make the tools usable from the command line and to set the necessary environment variables.
```console
. $HOME/esp/esp-idf/export.sh
```
Go to your project directory, e.g.
```console
cd ~/esp/relayX2board
```
Select your target
```console
idf.py set-target esp32
```
You can skip the menuconfig for the are no project specific variables to set up
```console
idf.py menuconfig
```
Compile and generate the code
```console
idf.py build
```
Set IO0 to GND and press the EN button. And then type
```console
idf.py -p /dev/ttyUSB1 flash monitor
```
Adjust the serial port to match your system (e.g. `/dev/ttyUSB0`, `COM3`, etc.). After flashing, release IO0 and press EN to start the firmware.

## Rest API
The REST API allows managing relays and retrieving board status.

The JSON data structure is the following:

(1) Set relays on & off: 
```json
{"device":"....","set":"...","key":"...",["time":"<time_in_seconds>"]}
```
Where
device= "led" / "uno"= relay 1 (outermost) / "dos"= relay 2

set= "on" / "off"

key= shared key defined in config.h

time= timeout to turn the relay OFF

response:
```json
 {"device":"...","status":"on/off/error"}
```

(2) Retrieve board information:
```json
{"info":"all","key":"qWpJnwA0crlmgv"}
```
response:
```json
{"uptime":"0-00:00:09","RSSI":"<WiFi_signal_dBm>","uno":"off","dos":"on", "led":"off"}
```
where:

uptime (days-hh:mm:ss) is the time since last boot.

RSSI is a measure of the Wi-Fi signal strength, represented in negative dBm values.

### Test the Rest API interface
Use `curl` to send requests. Example (replace IP and key):

To set REALY 1 "on" send
```console
curl  -X GET http://192.168.1.137:80 -d '{"device":"uno","set":"on","key":"qWpJnwA0crlmgv"}'	
```
the response will be
```jon
{"device":"uno","status":"on"}
```

On boot the device publishes its network info over MQTT, for example:
```json
{"ip":"192.168.1.137","MAC":"B0:A7:32:27:FF:5C"}
```

## MQTT
Publish control messages to the `relayX2board/set` topic. Example payload:
```json
{"device":"uno","set":"on"}
```
The device will apply the command and publish status updates to `relayX2board/info`, for example:
```json
{"device":"uno","status":"on"}
```

### Test the MQTT interface
Open two shells: one to subscribe and one to publish.

Subscribe to status updates:
```console
mosquitto_sub -d -t 'relayX2board/info'
```
Example messages received:
```console
Client null sending CONNECT
Client null received CONNACK (0)
Client null sending SUBSCRIBE (Mid: 1, Topic: relayX2board/info, QoS: 0, Options: 0x00)
Client null received SUBACK
Subscribed (mid: 1): 0
Client null received PUBLISH (d0, q0, r0, m0, 'relayX2board/info', ... (48 bytes))
{"IP":"192.168.1.137","MAC":"4C:C3:82:BF:3E:24"}
Client null received PUBLISH (d0, q0, r0, m0, 'relayX2board/info', ... (30 bytes))
{"device":"uno","status":"on"}
```

Publish a command to turn relay 1 on:
```console
mosquitto_pub -t 'relayX2board/set' -m '{"device":"uno","set":"on"}'
```

## Home Assistant integration
Integration with Home Assistant is done through MQTT. You need to add the MQTT service to your Home Assistant instance.
Go [here](https://www.home-assistant.io/integrations/mqtt/) and follow the instruction.

Once you have the MQTT service enabled, it is time to add the board as MQTT device.

Go to "Integration entities -> CONFIGURE -> Add MQTT Device"

<p align="center">
<img width="609" height="485566" alt="image" src="https://github.com/user-attachments/assets/ae74c090-91de-44d9-bb80-0a410d87eb1b" />
</p>

Go to configure MQTT device “relayX2board" and select “Template”
<p align="center">
<img width="633" height="822" alt="image" src="https://github.com/user-attachments/assets/f426e5eb-5919-4207-b4d2-4c9ae174d195" />
</p>

Press "Next" and then "Finish".

Repeat the steps above for RELAY_2.

Add the device to the Dashboard
<p align="center">
<img width="413" height="220" alt="image" src="https://github.com/user-attachments/assets/c1e0159e-d2cb-4161-a02d-2abe691fe0c3" />
</p>


I had to edit the YAML file to get th shape in the picture above.
<p align="center">
<img width="630" height="246" alt="image" src="https://github.com/user-attachments/assets/efe0d4f3-a58a-4d71-b53a-5ab7dfe997e1" />
</p>

Alternatively create an MQTT Template.

Use the MQTT device discovery/configuration in Home Assistant and create template entities for each relay. After adding the entities you can place them on the dashboard.

Go to configure MQTT device “relayX2board" and select “Template”

Add the following `switch` entries to your Home Assistant `configuration.yaml` (replace broker settings and topics if different):

```yaml
switch:
  - platform: mqtt
    name: "Relay 1"
    state_topic: "relayX2board/info"
    value_template: >-
      {% if value_json.device is defined and value_json.device == 'uno' %}
        {{ value_json.status }}
      {% elif value_json.uno is defined %}
        {{ value_json.uno }}
      {% else %}
        unknown
      {% endif %}
    command_topic: "relayX2board/set"
    payload_on: '{"device":"uno","set":"on"}'
    payload_off: '{"device":"uno","set":"off"}'
    qos: 0
    retain: false

  - platform: mqtt
    name: "Relay 2"
    state_topic: "relayX2board/info"
    value_template: >-
      {% if value_json.device is defined and value_json.device == 'dos' %}
        {{ value_json.status }}
      {% elif value_json.dos is defined %}
        {{ value_json.dos }}
      {% else %}
        unknown
      {% endif %}
    command_topic: "relayX2board/set"
    payload_on: '{"device":"dos","set":"on"}'
    payload_off: '{"device":"dos","set":"off"}'
    qos: 0
    retain: false
```

Notes:
- Ensure Home Assistant is configured to connect to the same MQTT broker used by the device.
- The `state_topic` receives different JSON messages (network info and per-device status). The `value_template` handles both formats.
- Adjust names, topics, and QoS to match your setup.

