# Tri-Weather-Clock

## Introduction
Based on following instructable: https://www.instructables.com/Live-Regional-Weather-Clock/
but highly modified:
- Added PNG Decoder-
- Added MQTT Support
- Added Node-red as image middleware and for PNG cropping
- Added radar picture from german DWD
- Added fetching temperature from openweathermap.org
- Using SD.h instead ofS SD_MMC.h
- Added dimming display via MQTT command


## Used Libraries
Dependency Graph
|-- U8g2 @ 2.36.15 
|-- JPEGDEC @ 1.8.4
|-- PNGdec @ 1.1.6
|-- Adafruit MQTT Library @ 2.6.3 (manually installed)
|-- GFX Library for Arduino @ 1.6.4 (manually installed)
|-- HTTPClient @ 3.1.3 (included with ESP32 core)
|-- SD @ 3.1.3 (included with ESP32 core)
|-- SPI @ 3.1.3 (included with ESP32 core)
|-- WiFi @ 3.1.3 (included with ESP32 core)
|-- FS @ 3.1.3  (included with ESP32 core)


### Lib depenencies which needs to be manually installed:
https://github.com/adafruit/Adafruit_MQTT_Library
https://github.com/moononournation/Arduino_GFX


## Pin layout
ESP32-S3-Zero        LCD 1    LCD 2    LCD 3    Micro-SD / SD card slot
=============        =====    =====    =====    =======================
5V                   VCC      VCC      VCC
GND                  GND      GND      GND      GND
3V3(OUT)                                        VCC
GP1                                             MOSI
GP2                                             CLK
GP3                                             CS
GP4                                    CS       
GP5                           CS
GP6                  CS
GP7   -> Resistor -> SCL      SCL      SCL
GP8   -> Resistor -> SDA      SDA      SDA
GP9   -> Resistor -> IO1      IO1      IO1
GP10  -> Resistor -> IO2      IO2      IO2
GP11  -> Resistor -> IO3      IO3      IO3
GP12  -> Resistor -> RST      RST      RST
GP13                                            MISO
GP14                 BL       BL       BL


## How to use
- Change following line in the PNGdec library in file: PNGdec.h --> #define PNG_MAX_BUFFERED_PIXELS ((500*4 + 1)*2)
- Format SD card with FAT32
- Create a folder called: 3displays
- Upload flows.json nodered flow to your nodered instance

Provide your information for the following lines in the code:
- const char *SSID_NAME = "YOURE_SSID";
- const char *SSID_PASSWORD = "Your_PASSWORD";

- char mqtt_server[40] = "Your_MQTT_Server_IP";   
- char mqtt_port[6] = "YOUR_MQTT_PORT";                 
- char mqtt_username[40] = "Your_MQTT_Username";           
- char mqtt_password[40] = "Your_MQTT_Password";  
- String node_red_api = "YOUR_Nodered_api/radarbild";

Add your longitue and latitude values and your openweathermap API key
- const char *url = "http://api.openweathermap.org/data/3.0/onecall?lat=YOUR_LAT&lon=YOUR_LONG&appid=YOURAPIKEY&lang=de&exclude=minutely,hourly,daily&units=metric";
