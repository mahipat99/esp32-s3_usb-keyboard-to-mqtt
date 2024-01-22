# ESP32 S3 OTG Keyboard with MQTT Integration

<img src="https://i.imgur.com/ZRKtb5E.jpg" alt="ESP32-S3" width= "300" height="300"/>

## Overview

This project demonstrates the integration of an ESP32 S3-based OTG keyboard with an MQTT server using WiFi. The ESP32 keyboard sends data to the MQTT server, providing a seamless way to transmit keyboard inputs over a network.

## Table of Contents

- [Prerequisites](#prerequisites)
- [Getting Started](#getting-started)
- [Gitpod Deployment](#gitpod-deployment) 
- [Changing MQTT Credentials](#changing-mqtt-credentials)
- [Optional: Changing Topic](#optional-changing-topic)
- [Flashing the Binaries](#flashing-the-binaries)
  - [Using ESP Flash Download Tool](#using-esp-flash-download-tool)
  - [Using esptool.py](#using-esptoolpy)
- [Usage](#usage)

## Prerequisites

Before getting started, ensure you have the following:

- Visual Studio Code installed, or you can use Gitpod for online compilation
- PlatformIO installed as an extension for Visual Studio Code
- ESP32 S3 development board
- MQTT server set up (e.g., [Mosquitto](https://mosquitto.org/))

## Getting Started

1. Clone the repository:

   ```bash
   git clone https://github.com/mahipat99/esp32-s3_usb-keyboard-to-mqtt.git
   ```

2. Open the project in Visual Studio Code.

3. Connect your ESP32 S3 development board to your computer.

4. Build and upload the code to your ESP32 using the PlatformIO extension in Visual Studio Code.

## Gitpod Deployment

You can also try this project in a Gitpod environment. Click the following link to open the project in Gitpod:

[![Open in Gitpod](https://gitpod.io/button/open-in-gitpod.svg)](https://gitpod.io/#https://github.com/mahipat99/esp32-s3_usb-keyboard-to-mqtt)

## Changing MQTT Credentials

To use your own MQTT credentials, modify the following lines in the `src/main.cpp` file:

```cpp
#define MQTT_USERNAME "your-mqtt-username"
#define MQTT_PASSWORD "your-mqtt-password"
#define MQTT_HOST IPAddress(192, 168, 1, 1)
#define MQTT_PORT 1883
```

Replace `"your-mqtt-username"` and `"your-mqtt-password"` with your MQTT server credentials. Update the `MQTT_HOST` and `MQTT_PORT` if your MQTT server has a different address or port.

## Optional: Changing Topic

Optionally, you can change the MQTT topic by modifying the following line:

```cpp
#define EVENT_TOPIC "keypads/lr-numpad01/action"
```

Replace `"keypads/lr-numpad01/action"` with your desired MQTT topic.

## Flashing the Binaries

### Using ESP Flash Download Tool

1. Download the [ESP Flash Download Tool](https://www.espressif.com/sites/default/files/tools/flash_download_tool_3.9.5.zip) and extract the contents.

2. Open the ESP Flash Download Tool, set chip type to ESP32-S3

3. Set the addresses and file paths as follows:
   - `0x0`: bootloader.bin
   - `0x10000`: firmware.bin
   - `0x8000`: partition.bin

4. Connect your ESP32 S3 development board to your computer.

5. Put your ESP32 into flashing mode.

6. Set the COM port, and then click the "Erase" followed by the "Start" button  in the ESP Flash Download Tool to initiate the flashing of the binaries

7. Once the flashing process is complete, restart your ESP32.

### Using esptool.py

1. Open a terminal.

2. Navigate to the project directory.

3. Run the following command to erase the flash:

   ```bash
   esptool.py -p COM10 erase_flash
   ```

4. Run the following command to flash the binaries:

   ```bash
   esptool.py -p COM10 write_flash --flash_mode dio --flash_size 8MB 0x0 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin
   ```

5. Once the flashing process is complete, restart your ESP32.

## Usage

Once you have uploaded the code, the ESP32-S3 OTG keyboard will initiate an access point (ESPS3-OTG-KB), connect to it, configure the Wi-Fi credentials, and then attach a keyboard. It will start sending data to the specified MQTT server and topic.
