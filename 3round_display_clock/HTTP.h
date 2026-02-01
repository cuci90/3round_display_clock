#pragma once

#include <FS.h>
#include <HTTPClient.h>
#include <WiFi.h>


#define FILE_BUFFER_SIZE 32768
uint8_t file_buf[FILE_BUFFER_SIZE];

void downloadRadar(String uri, fs::FS &fs, String path)
{

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.printf("file open %s failed!\n", path.c_str());
    return;
  }

  HTTPClient http;
  if (!http.begin(uri)) {
    Serial.println("Unable to begin HTTP request");
    file.close();
    return;
  }

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    WiFiClient *stream = http.getStreamPtr();

    while (http.connected()) {
      int available = stream->available();

      if (available) {
         
        int toRead = (available > FILE_BUFFER_SIZE) ? FILE_BUFFER_SIZE : available;
        int read = stream->readBytes((char *)file_buf, toRead);

        if (read > 0) {
          file.write(file_buf, read);
        }
      } else {
        // no data available yet
        delay(10);
        // break if nothing left and connection closed
       
        if (!http.connected()) break;
      }
    }
    Serial.println("flushing");
    file.flush();
  } else {
    Serial.print("HTTP error: ");
    Serial.println(httpCode);
  }
Serial.println("httpend");
  http.end();
  file.close();
}
