# ESP32-X2-relay
[![Apache-2.0 license](https://img.shields.io/badge/License-Apache_2.0-green.svg?style=flat-square)](https://www.apache.org/licenses/LICENSE-2.0)
[![ESP32](https://img.shields.io/badge/ESP-32-green.svg?style=flat-square)](https://www.espressif.com/en/products/socs/esp32)

This project allows you to control a dual-channel relay module using an **ESP32**, enabling remote or automated switching for AC/DC devices.

<p align="center">
<img src="https://github.com/user-attachments/assets/72864717-ebb3-4218-aff5-bdfb89d969ad" width="408" height="407">
</p>

The board is sourced with no information. The following tries to fill the gap providing SW and guidelines to use all the four resources on board (the two relays, the led and the button), both through a Rest API and MQTT. 

A guide for the integration with Home Assistant is given as well.

The SW is developed for FreeRTOS OS and the development environment is the ESP-IDF. 

# USB connection
You can find some specs of the board ["here"](https://devices.esphome.io/devices/esp32-relay-x2/).

The board doesn't have a USB interface but a 6 pins header meant to connect an USB to TTL serial converter that you need to buy apart. 

Connect the header's and the USB-converter's GND and +5, and the TX & RX crossed. The two pins left in the header are a GND and the IO0. You need to keep the IO0 connected to GND while flashing.

# Environment settings
The source is in C language. 

The execution environment is FreeRTOS OS.

The IP connection is through Wi-Fi. You need to set your SSID and password in the file config.h

```
// WiFi
#define	WIFI_SSID	    "YOUR_SSID"
#define	WIFI_PASSWORD	"THE_PASSWORD_OF_YOUR_SSID"
```

The SW is expecting an MQTT broker. So you need to overwrite the IP address and Port of your MQTT server.
```
// Mosquitto address
#define MQTT_HOST_IP_ADDR 	"192.168.1.2"
#define MQTT_HOST_IP_PORT 	1883		// default Mosquitto port
```

And if you want, modify the MQTT device name.
```
#define DEVICE_MQTT_NAME	"relayX2board"
```

# Building & flashing
You need the ESP-IDF development environment installed. I am running ESP-IDF v5.4-dev-2194-gd7ca8b94c8 for Linux on a Raspberry Pi 3.

Go to ["Installation of ESP-IDF and Tools on Linux"](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/linux-setup.html) if you need to install it.


Create a directory for the project (e.g. ./relayX2board), download the files from Github and follow the regular process to generate and flash the SW as described in the ESP-IDF instructions ["Configure Your Projec"](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/linux-macos-setup.html#configure-your-project).


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
Release the IO0 pin and press the EN push button to reset the board and start the execution.

Note that you need to choose the right /dev/ttyXXX port depending on your environment.


# Rest API
The Rest API interface allow to manage the relays and retrieve board status.

The JSON data structure is the following:

(1) Set relays on & off 
```console
{"device":"....","set":"...","key":"..."}
```
Where
device= "led" / "uno"= relay 1 (outermost) / "dos"= relay 2

set= "on" / "off"

key= shared key defined in config.h

response:
```console
 {"device\":"...","status":"on/off/error"}
```

(2) Retrieve board information
```console
{"info":"all","key":"qWpJnwA0crlmgv"}
```
response:
```console
{"uptime":"0-00:00:09", "uno":"off","dos":"on", "led":"off"}
```
where 
uptime (days-hh:mm:ss) is the time elapsed since last boot 

## Test the Rest API interface
You can test the Rest API with CURL as follows:

To set REALY 1 "on" send
```console
curl  -X GET http://192.168.1.137:80 -d '{"device":"uno","set":"on","key":"qWpJnwA0crlmgv"}'	
```
the response will be
```console
{"device":"uno","status":"on"}
```

The IP address of the device is reported via MQTT in the first PUBLISH message as follows:
```console
{"ip":"192.168.1.137","MAC":"B0:A7:32:27:FF:5C"}
```

# MQTT
To set on/off a relay with MQTT you have to publish to topic 'relayX2board/set' the following: 
```console
{"device":"uno","set":"on"}
```
The device will set the relay 1 "on" and publish the status to topic 'relayX2board/info' as follows
```console
{"device":"uno","status":"on"}
```

## Test the MQTT interface
To test the MQTT interface open two shell instances, one for publishing and another for subscribing.

Subscribe to MQTT topic  'relayX2board/info':
```console
pi@MQTT:~ $ mosquitto_sub -d -t 'relayX2board/info'
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

Publish to MQTT topic  'relayX2board/set' to turn the relay 1 "on":
```console
pi@MQTT:~ $ mosquitto_pub -t "relayX2board/set" -m '{"device":"uno","set":"on"}'
```

# Home Assistant integration
Integration with Home Assistant is done through MQTT. You need to add the MQTT service to your Home Assistant instance: Go ["here"](https://www.home-assistant.io/integrations/mqtt/) and follow the instruction.

Once you have the MQTT service enabled, it is time to add the board as MQTT device.

Go to "Integration entities -> CONFIGURE -> Add MQTT Device"

<p align="center">
<img width="609" height="485566" alt="image" src="https://github.com/user-attachments/assets/ae74c090-91de-44d9-bb80-0a410d87eb1b" />
</p>

Go to configure MQTT device “relayX2board and select “Template”
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



