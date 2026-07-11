#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Adafruit_SSD1306.h>

extern Adafruit_SSD1306 display;
extern void setRGB(bool r, bool g, bool b);

// Replace this URL with your direct data stream link on raw GitHub when ready!
const char* firmwareUpdateUrl = "https://raw.githubusercontent.com/YOUR_GITHUB_USERNAME/YOUR_REPO/main/firmware.bin";

void handleSystemCloudUpdate() {
  if (WiFi.status() != WL_CONNECTED) {
    setRGB(true, false, false); // Red Light
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("SYSTEM UPDATE");
    display.println("---------------------");
    display.println("\n[ERROR]\nNo WiFi Connection!");
    display.println("\nClick G2 to Go Back");
    display.display();
    delay(2000);
    return;
  }

  setRGB(true, true, false); // Solid Yellow Light
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("SYSTEM UPDATE");
  display.println("---------------------");
  display.setCursor(0, 20);
  display.println("Connecting stream...");
  display.display();

  Serial.println("[OTA] Opening single-partition server connection...");

  HTTPClient http;
  http.begin(firmwareUpdateUrl);
  
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("[OTA] Server returned HTTP Error: %d\n", httpCode);
    setRGB(true, false, false); // Red
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("UPDATE ERROR");
    display.println("---------------------");
    display.printf("\nHTTP Error Code:\n%d", httpCode);
    display.println("\n\nClick G2 to Go Back");
    display.display();
    http.end();
    return;
  }

  int contentLength = http.getSize();
  if (contentLength <= 0) {
    Serial.println("[OTA] Could not resolve stream asset size.");
    setRGB(true, false, false);
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("UPDATE ERROR");
    display.println("---------------------");
    display.println("\nInvalid file size!");
    display.println("\nClick G2 to Go Back");
    display.display();
    http.end();
    return;
  }

  // Check if your primary partition layout can accommodate the new size
  if (!Update.begin(contentLength, U_FLASH)) {
    Serial.printf("[OTA] Partition error. Maximum available space: %d bytes\n", Update.size());
    setRGB(true, false, false);
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("SPACE ERROR");
    display.println("---------------------");
    display.println("\nBinary too large\nfor 4MB flash layout!");
    display.println("\nClick G2 to Go Back");
    display.display();
    http.end();
    return;
  }

  Serial.printf("[OTA] Flashing firmware image (%d bytes) directly into flash...\n", contentLength);
  WiFiClient* stream = http.getStreamPtr();
  
  size_t writtenBytes = 0;
  uint8_t buffer[512];
  unsigned long lastOledProgressTime = 0;

  // Process data blocks continuously through RAM cache until stream ends
  while (http.connected() && (writtenBytes < contentLength)) {
    size_t availableBytes = stream->available();
    if (availableBytes > 0) {
      size_t readLen = stream->readBytes(buffer, min(availableBytes, sizeof(buffer)));
      if (readLen > 0) {
        Update.write(buffer, readLen);
        writtenBytes += readLen;
        
        // Feed the hardware watchdog timer (WDT) to maintain background safety
        yield(); 

        // Update the OLED display periodically without slowing down the stream
        if (millis() - lastOledProgressTime > 300) {
          lastOledProgressTime = millis();
          int progressPercent = (writtenBytes * 100) / contentLength;
          
          display.clearDisplay();
          display.setCursor(0, 0);
          display.println("FLASHING DEVICE");
          display.println("---------------------");
          display.setCursor(0, 20);
          display.printf("Progress: %d%%\n", progressPercent);
          display.printf("Bytes: %d/%d\n", writtenBytes, contentLength);
          display.println("\nDO NOT POWER OFF!");
          display.display();
        }
      }
    }
    yield();
  }

  if (Update.end(true)) {
    Serial.println("[OTA] Single-partition flash success!");
    setRGB(false, true, false); // Solid Green
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("UPDATE SUCCESS");
    display.println("---------------------");
    display.println("\nFlashing Complete!\nRebooting device...");
    display.display();
    delay(2000);
    
    ESP.restart(); // Software restart to load the new firmware code directly
  } else {
    Serial.printf("[OTA] Update error occurred: #%d\n", Update.getError());
    setRGB(true, false, false);
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("FLASH FAILURE");
    display.println("---------------------");
    display.printf("\nError Code: %d\n", Update.getError());
    display.println("\nClick G2 to Go Back");
    display.display();
  }
  
  http.end();
}

#endif
