#include <Arduino.h>

// Lib depenencies which needs to be manually installed:
// https://github.com/adafruit/Adafruit_MQTT_Library
// https://github.com/moononournation/Arduino_GFX

//Change following line in the PNGdec library in file: PNGdec.h --> #define PNG_MAX_BUFFERED_PIXELS ((500*4 + 1)*2)

// PNG Decoder
// MQTT
// Node-red as image middleware and for PNG cropping
// radar picture from german DWD
// Fetch temperature from openweathermap.org
// Using SD.h instead ofS SD_MMC.h
// dim display via MQTT command

/*
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
*/


/*
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
*/



/*******************************************************************************
 * Start of Arduino_GFX setting
 ******************************************************************************/
#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include <U8g2lib.h>
#include "JPEG.h"


// MQTT Client
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>

#include <SPI.h>
#include <SD.h>

//http download
#include "HTTP.h"
#include <HTTPClient.h>

// png implementiation
#include "PNG.h"
PNG png;
// with this you can move the PNG draw area in case the picture is bigger then the complete screen
#define PNG_X_OFFSET 0
#define PNG_Y_OFFSET 0

//Devince SD card pins
#define SD_CS 3
#define SD_MOSI 1
#define SD_SCK 2
#define SD_MISO 13

#define GFX_BL 14


SPIClass sdSPI;

// Display 1
Arduino_DataBus *bus1 = new Arduino_ESP32QSPI(
    6 /* CS */, 7 /* SCK */, 8 /* D0 */, 9 /* D1 */, 10 /* D2 */, 11 /* D3 */, true /* is_shared_interface */);
Arduino_GFX *g = new Arduino_ST77916(
    bus1, 12 /* RST */, 0 /* rotation */, true /* IPS */, 360 /* width */, 360 /* height */,
    0 /* col offset 1 */, 0 /* row offset 1 */, 0 /* col offset 2 */, 0 /* row offset 2 */,
    st77916_150_init_operations, sizeof(st77916_150_init_operations));
#define CANVAS
Arduino_Canvas *gfx1 = new Arduino_Canvas(
    360 /* width */, 360 /* height */, g);

// Display 2
Arduino_DataBus *bus2 = new Arduino_ESP32QSPI(
    5 /* CS */, 7 /* SCK */, 8 /* D0 */, 9 /* D1 */, 10 /* D2 */, 11 /* D3 */, true /* is_shared_interface */);
Arduino_GFX *gfx2 = new Arduino_ST77916(
    bus2, GFX_NOT_DEFINED /* RST */, 0 /* rotation */, true /* IPS */, 360 /* width */, 360 /* height */,
    0 /* col offset 1 */, 0 /* row offset 1 */, 0 /* col offset 2 */, 0 /* row offset 2 */,
    st77916_150_init_operations, sizeof(st77916_150_init_operations));

// Display 3
Arduino_DataBus *bus3 = new Arduino_ESP32QSPI(
    4 /* CS */, 7 /* SCK */, 8 /* D0 */, 9 /* D1 */, 10 /* D2 */, 11 /* D3 */, true /* is_shared_interface */);
Arduino_GFX *gfx3 = new Arduino_ST77916(
    bus3, GFX_NOT_DEFINED /* RST */, 0 /* rotation */, true /* IPS */, 360 /* width */, 360 /* height */,
    0 /* col offset 1 */, 0 /* row offset 1 */, 0 /* col offset 2 */, 0 /* row offset 2 */,
    st77916_150_init_operations, sizeof(st77916_150_init_operations));



const char *SSID_NAME = "YOURE_SSID";
const char *SSID_PASSWORD = "Your_PASSWORD";
const long gmtOffset_sec = 1 * 60 * 60; // timezone

// MQTT
char mqtt_server[40] = "Your_MQTT_Server_IP";        // MQTT broker address
char mqtt_port[6] = "YOUR_MQTT_PORT";                    // MQTT broker port
char mqtt_username[40] = "Your_MQTT_Username";           // MQTT username
char mqtt_password[40] = "Your_MQTT_Password"; // MQTT password

// Create an instance of the Adafruit MQTT client
WiFiClient espClient;
Adafruit_MQTT_Client mqtt(&espClient, mqtt_server, atoi(mqtt_port), "3rounddisplays", mqtt_username, mqtt_password);

// Define your MQTT topic
Adafruit_MQTT_Subscribe topic_switch = Adafruit_MQTT_Subscribe(&mqtt, "3displays/switch");
Adafruit_MQTT_Subscribe topic_dim = Adafruit_MQTT_Subscribe(&mqtt, "3displays/dim");

// SD card
const char *picture_folder = "/3displays";
const char *picture_file_1 = "/3displays/beach.jpg";
const char *picture_file_2 = "/3displays/berge360.jpg";
const char *picture_file_radar = "/3displays/wetter.png";

String node_red_api = "YOUR_Nodered_api/radarbild";


static unsigned long targetTime; // next action time
static unsigned long minuteCounter;
static unsigned long hourCounter;
String mqtt_display_message = "normal"; // default display
boolean mqtt_display_switch = false; // tells if a change is needed
String tempStr;


int PNGDraw(PNGDRAW *pDraw)
{

  // Convert the PNG line to RGB565 (little-endian) and draw it.
  static uint16_t lineBuf[2600]; // max display width
  if (pDraw->iWidth > (int)(sizeof(lineBuf) / sizeof(lineBuf[0])))
    return 0;

  // Convert common PNG source pixel types to RGB565 safely.
  uint8_t *s = pDraw->pPixels;
  
  if (pDraw->iPixelType == PNG_PIXEL_TRUECOLOR_ALPHA) {
    // Serial.print("bei 2 ");
    for (int x = 0; x < pDraw->iWidth; x++) {
      uint8_t r = s[0], g = s[1], b = s[2];
      uint16_t us = (b >> 3) | ((g >> 2) << 5) | ((r >> 3) << 11);
      lineBuf[x] = us;
      s += 4; // skip alpha
    }
  }
   else {
    // Fallback: ask the PNG library to produce RGB565 into our buffer.
    png.getLineAsRGB565(pDraw, lineBuf, PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);
    Serial.print("Fallback");
  }

  // adapt the PNG_X_OFFSET and PNG_Y_OFFSET offsets as needed to center the image on your display
  gfx3->draw16bitRGBBitmap(PNG_X_OFFSET, pDraw->y-PNG_Y_OFFSET, lineBuf, pDraw->iWidth, 1);

  return 1;
}

void drawPNG(const char *filename)
{
  int rc;

  rc = png.open(
      filename,
      pngOpen,
      pngClose,
      pngRead,
      pngSeek,
      PNGDraw);

  if (rc == PNG_SUCCESS)
  {

    png.decode(0, 0);
    png.close();

    gfx1->flush();
  }
  else
  {
    Serial.println("PNG open failed here");
  }
}

int JPEGDraw1(JPEGDRAW *pDraw)
{
  // Serial.printf("Draw pos = %d,%d. size = %d x %d\n", pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight);
  gfx1->draw16bitRGBBitmap(pDraw->x, pDraw->y, pDraw->pPixels, pDraw->iWidth, pDraw->iHeight);
  return 1;
}

int JPEGDraw2(JPEGDRAW *pDraw)
{
  // Serial.printf("Draw pos = %d,%d. size = %d x %d\n", pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight);
  gfx2->draw16bitRGBBitmap(pDraw->x, pDraw->y, pDraw->pPixels, pDraw->iWidth, pDraw->iHeight);
  return 1;
}


void publishToMQTT(const char *topic, const char *payload)
{
  if (mqtt.connected())
  {
    Adafruit_MQTT_Publish mqtt_publish(&mqtt, topic, MQTT_QOS_1);
    mqtt_publish.publish(payload);
    Serial.println("MQTT Send");
  }
  else
  {
    Serial.println("Not connected to MQTT. Unable to publish.");
  }
}

void connectToMQTT()
{
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Connecting to MQTT WiFi...");
  }
  Serial.println("Connected to MQTT WiFi");

  // Connect to MQTT server
  int8_t ret;
  while ((ret = mqtt.connect()) != 0)
  {
    Serial.println("Could not connect to MQTT. Retrying in 5 seconds...");
    mqtt.disconnect();
    delay(5000);
  }
  Serial.println("Connected to MQTT");
}

void processMQTTMessage(String message)
{
  Serial.print("MQTT Message received: ");

  mqtt_display_message = message;
  mqtt_display_switch = true;
  // reset screen before updating
  gfx3->fillScreen(RGB565(99, 154, 206));
}

void MQTTdim(int dim_value)
{
  // Setup backlight PWM 0-255
  Serial.print("Dim display:  ");
   Serial.println(dim_value);
   analogWrite(GFX_BL, dim_value);
 
}

// Fetch current temperature (metric) from OpenWeatherMap onecall API
float fetchTemperature()
{
  const char *url = "http://api.openweathermap.org/data/3.0/onecall?lat=YOUR_LAT&lon=YOUR_LONG&appid=YOURAPIKEY&lang=de&exclude=minutely,hourly,daily&units=metric";

  HTTPClient http;
  float result = NAN;

  if (http.begin(url))
  {
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK)
    {
      String payload = http.getString();

      int idx = payload.indexOf("\"feels_like\":");
      if (idx >= 0)
      {
        int start = idx + 13;
        int end = start;
        while (end < payload.length())
        {
          char c = payload.charAt(end);
          if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.')
          {
            end++;
          }
          else
          {
            break;
          }
        }
        String num = payload.substring(start, end);
        result = num.toFloat();
      }
      else
      {
        Serial.println("temp not found in payload");
      }
    }
    else
    {
      Serial.print("HTTP error: ");
      Serial.println(httpCode);
    }
    http.end();
  }
  else
  {
    Serial.println("Unable to begin HTTP request");
  }

  return result;
}

void setClock()
{
  configTime(0, 0, "time1.google.com", "pool.ntp.org");

  Serial.print("Waiting for NTP time sync: ");
  time_t nowSecs = time(nullptr);
  while (nowSecs < 8 * 3600 * 2)
  {
    delay(500);
    Serial.print(".");
    yield();
    nowSecs = time(nullptr);
  }

  Serial.println();
  struct tm timeinfo;
  gmtime_r(&nowSecs, &timeinfo);
  Serial.print("Current time: ");
  char buf[26];
  Serial.println(asctime_r(&timeinfo, buf));
}

void setup()
{
  Serial.begin(115200);
  // Serial.setDebugOutput(true);

  Serial.println("3 rounds clock");

  delay(2000); // wait 2 seconds


  // Init Display
  Serial.println("gfx1->begin()");
  if (!gfx1->begin())
  {
    Serial.println("gfx1->begin() failed!");
  }

  Serial.println("gfx2->begin()");
  if (!gfx2->begin(GFX_SKIP_DATABUS_UNDERLAYING_BEGIN))
  {
    Serial.println("gfx2->begin() failed!");
  }

  Serial.println("gfx3->begin()");
  if (!gfx3->begin(GFX_SKIP_DATABUS_UNDERLAYING_BEGIN))
  {
    Serial.println("gfx3->begin() failed!");
  }




  gfx1->fillScreen(RGB565_NAVY);
  gfx1->setCursor(20, 145);
  gfx1->setTextColor(RGB565_RED);
  gfx1->setTextSize(10, 10, 4);
  gfx1->print("START");
  gfx1->flush();

  delay(1000);

  // show wifi connecting status
  gfx1->fillScreen(RGB565_BLACK);
  gfx1->setCursor(20, 145);
  gfx1->setTextColor(RGB565_RED);
  gfx1->setTextSize(10, 10, 4);
  gfx1->print("WIFI");
  gfx1->flush();
  delay(100);

  WiFi.begin(SSID_NAME, SSID_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Wifi not connected yet");
    delay(500);
  }

  // show wifi connecting status
  gfx1->fillScreen(RGB565_BLACK);
  gfx1->setCursor(20, 145);
  gfx1->setTextColor(RGB565_GREEN);
  gfx1->setTextSize(10, 10, 4);
  gfx1->print("WIFI");
  gfx1->flush();

  delay(1000);

  // Initialize MQTT subscription
  mqtt.subscribe(&topic_switch);
  mqtt.subscribe(&topic_dim);

  setClock();

  targetTime = ((millis() / 1000) + 1) * 1000;
  // targetTimeMinute = ((millis() / 1000) + 60) * 1000 ;

  int textSize = gfx1->width() / 8 / 6;

  gfx1->fillScreen(RGB565(99, 154, 206));
  gfx2->fillScreen(RGB565(99, 154, 206));
  gfx3->fillScreen(RGB565(99, 154, 206));
  gfx1->setTextColor(RGB565_WHITE);
  gfx1->setTextSize(textSize, textSize, 2 /* pixel_margin */);
  gfx1->flush();

  // SDcard init (use dedicated SPI instance)
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  if (!SD.begin(SD_CS, sdSPI))
  {
    Serial.println("SD card mount failed");
    // show sd connecting status
    gfx1->fillScreen(RGB565_BLACK);
    gfx1->setCursor(20, 145);
    gfx1->setTextColor(RGB565_RED);
    gfx1->setTextSize(10, 10, 4);
    gfx1->print("SD FAIL");
    gfx1->flush();
    delay(1500);
    return;
  }
  else
  {
    Serial.println("SD card mounted successfully");

    // show sd connecting status
    gfx1->fillScreen(RGB565_BLACK);
    gfx1->setCursor(20, 145);
    gfx1->setTextColor(RGB565_GREEN);
    gfx1->setTextSize(10, 10, 4);
    gfx1->print("SD OK");
    gfx1->flush();

    delay(1500);

  }




}

void loop()
{

  unsigned long cur_millis = millis();
  if (cur_millis >= targetTime)
  {
    targetTime += 1000;

    char timeStr[9];
    time_t now;
    time(&now);
    now += gmtOffset_sec;
    struct tm *tmLocal = localtime(&now);
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", tmLocal);

    // decouple time
    char timeStrHour[3];
    char timeStrMin[3];
    char timeStrSec[3];
    char day[3];
    char month[3];
    char year[5];
    strftime(timeStrHour, sizeof(timeStrHour), "%H", tmLocal);
    strftime(timeStrMin, sizeof(timeStrMin), "%M", tmLocal);
    strftime(timeStrSec, sizeof(timeStrSec), "%S", tmLocal);

    strftime(day, sizeof(day), "%d", tmLocal);
    strftime(month, sizeof(month), "%m", tmLocal);
    strftime(year, sizeof(year), "%Y", tmLocal);

    //normal display
    if (mqtt_display_message == "normal")
    {

      if (mqtt_display_switch == true)
      { // reload radar image and temperature after mqtt normal command
         drawPNG(picture_file_radar);
         gfx3->setCursor(65, 320);
         gfx3->println(tempStr);
         mqtt_display_switch = false;
      }
      // change second counter
      gfx3->setFont(u8g2_font_inb38_mf);
      gfx3->setTextSize(1, 1, 0);
      // gfx3->fillScreen(RGB565_BLACK);
      gfx3->setCursor(140, 70);
      gfx3->setTextColor(RGB565_BLACK, RGB565(99, 154, 206));
      gfx3->println(timeStrSec);
      gfx3->flush();
    }
    //mqtt message display
    else if (mqtt_display_message != "normal")
    {
      if (mqtt_display_switch == true)
      {
      gfx3->fillScreen(RGB565_RED);
      // gfx3->drawRect(100, 100, 50, 50, RGB565_WHITE);
      gfx3->setCursor(30, 100);
      gfx3->setTextColor(RGB565_BLACK);
      gfx3->println(mqtt_display_message);
      gfx3->flush();
        
        mqtt_display_switch = false;
      }
      
    }

    // change minute counter
    if (minuteCounter != timeStrMin[1])
    {
      minuteCounter = timeStrMin[1];

      //load background image from SD card
      if (SD.exists(picture_file_2))
      {

        if (jpeg.open(picture_file_2, jpegOpenSD, jpegClose, jpegRead, jpegSeek, JPEGDraw2))
        {
          jpeg.setPixelType(RGB565_LITTLE_ENDIAN);
          jpeg.setCropArea(1, 1, 360, 360); // requested area
          jpeg.decode(0 /* x */, 0 /* y */, 0 /* options */);
          jpeg.close();
          gfx2->flush();
        }
        else
        {
          Serial.print("error = ");
          Serial.println(jpeg.getLastError(), DEC);
        }

        Serial.print("JPEG loading finished ");

        delay(1000);

      }
      else
      {
        Serial.println("SD card connection lost");
      }

      // gfx2->setFont(u8g2_font_logisoso58_tn);
      gfx2->setFont(u8g2_font_fub49_tn);
      gfx2->setTextSize(2, 2, 0);
      //gfx2->fillScreen(RGB565(99, 154, 206));
      gfx2->setCursor(95, 230);
      gfx2->setTextColor(RGB565_WHITE);
      gfx2->println(timeStrMin);
      gfx2->flush();
    }

    // change hour counter + everything which should be updated every hour
    if (hourCounter != timeStrHour[1])
    {
      hourCounter = timeStrHour[1];
      
      //load background image from SD card
      if (SD.exists(picture_file_1))
      {

        if (jpeg.open(picture_file_1, jpegOpenSD, jpegClose, jpegRead, jpegSeek, JPEGDraw1))
        {
          jpeg.setPixelType(RGB565_LITTLE_ENDIAN);
          jpeg.setCropArea(0, 0, 360, 360); // requested area
          jpeg.decode(0 /* x */, 0 /* y */, 0 /* options */);
          jpeg.close();
          gfx1->flush();
        }
        else
        {
          Serial.print("error = ");
          Serial.println(jpeg.getLastError(), DEC);
        }

        Serial.print("JPEG loading finished ");

        delay(1000);

      }
      else
      {
        Serial.println("SD card connection lost");
      }

      gfx1->setFont(u8g2_font_fub49_tn);
      gfx1->setTextSize(2, 2, 0);
      // gfx1->fillScreen(RGB565(99,154,206));
      gfx1->setCursor(95, 235);
      gfx1->setTextColor(RGB565_WHITE);
      gfx1->println(timeStrHour);

      // show date
      gfx1->setFont();
      gfx1->setTextSize(5, 5, 0);
      gfx1->setCursor(110, 280);
      gfx1->setTextColor(RGB565_BLACK);
      String dateStr = String(day) + "." + String(month) /*+ "." + String(year)*/;
      gfx1->println(dateStr);
      gfx1->flush();



      //load radar background once an hour but displayed on seconds display
      downloadRadar(node_red_api, SD, picture_file_radar);
      drawPNG(picture_file_radar);

      // update temperature ervery hour and shown on seconds display
      gfx3->setTextColor(RGB565_BLACK);
      //gfx3->setCursor(10, 200);
      //gfx3->println("Gef\xFChlt:");
      gfx3->setCursor(65, 320);
      tempStr = String(fetchTemperature()) + "C";
      gfx3->println(tempStr);
    }
  }

  // Ensure the connection to the MQTT server is maintained
  if (!mqtt.connected())
  {
    connectToMQTT();
  }

  // Process MQTT messages
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription()))
  {
    if (subscription == &topic_switch)
    {
      // Handle MQTT message
      String message = (char *)topic_switch.lastread;
      Serial.println("Message MQTT: " + message);
      processMQTTMessage(message);
    } else if (subscription == &topic_dim)
    {
      // Handle MQTT message
      //int message = (int)topic_dim.lastread;
      //Serial.println("Message MQTT: " + message);
      char buf[10];
      memcpy(buf, topic_dim.lastread, topic_dim.datalen);
      buf[topic_dim.datalen] = '\0';

      int dim = atoi(buf);

      MQTTdim(dim);
    }
  }

  delay(10);
}
