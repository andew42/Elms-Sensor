#define NO_GLOBAL_TWOWIRE
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <LittleFS.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <OneWire.h>
#include <ezTime.h>
#include "main.h"
#include "bme680_driver.h"
#include "si705_driver.h"
#include "bh1750_driver.h"
#include "ds18b20_driver.h"
#include "ldr_driver.h"
#include "status_page.h"

// ***** Network credentials *****
#include "password.h"
// above secret include file defines:
// const char *ssid = "ssid";
// const char *password = "password";

// The influx database UDP address and port
#define SERVER_IP IPAddress(192, 168, 0, 14)
#define SERVER_PORT 8089

// Enable one of these name/trim pairs

// const char *hostname = "es-garage-ext";
// #define BME680_TEMP_TRIM 0.7f // Subtract this

// const char *hostname = "es-garage-int";
// #define BME680_TEMP_TRIM 0.8f // Subtract this

const char *hostname = "es-master-bedroom";
#define BME680_TEMP_TRIM 1.5f // Subtract this

// const char *hostname = "es-guest-bedroom";
// #define BME680_TEMP_TRIM 0.0f // Subtract this

// const char *hostname = "es-livingroom";
// #define BME680_TEMP_TRIM 1.2f // Subtract this

// const char *hostname = "es-study";
// #define BME680_TEMP_TRIM 1.5f // Subtract this

// const char *hostname = "es-kitchen";
// #define BME680_TEMP_TRIM 1.8f // Subtract this

// const char *hostname = "es-bathroom";
// #define BME680_TEMP_TRIM 0.0f // Subtract this

// const char *hostname = "es-testbed";
// #define BME680_TEMP_TRIM 0.0f // Subtract this

// const char *hostname = "es-attic";
// #define BME680_TEMP_TRIM 1.6f // Subtract this

// Requires Witty Cloud with LDR on board
// https://www.instructables.com/id/Witty-Cloud-Module-Adapter-Board/
#define LDR_DRIVER 0

// Flash led for debugging
#define FLASH_LED 0

// ********************************

// Drivers for active sensors
int drivers_count = 0;
SensorDriver *drivers[MAX_SENSOR_DRIVERS];

// HTTP web server for current status
ESP8266WebServer server(80);

// OneWire
OneWire ds;

// I2C
TwoWire I2C;

// Last startup date time
Timezone myTZ;
startupEntry startupLog[MAX_STARTUP_LOG_ENTRIES];
const char *startupLogFileName = "SULog";
extern struct rst_info resetInfo;

// Updates the start up log
void updateStartupLog()
{
  // Load startup log
  File f = LittleFS.open(startupLogFileName, "r");
  if (f)
  {
    f.readBytes((char *)&startupLog, sizeof(startupLog));
    f.close();
  }

  // Add new entry to start of the log
  for (int i = MAX_STARTUP_LOG_ENTRIES - 1; i > 0; i--)
    startupLog[i] = startupLog[i - 1];
  startupLog[0].time = now();
  startupLog[0].reason = resetInfo.reason;

  // Save startup log
  f = LittleFS.open(startupLogFileName, "w");
  if (f)
  {
    f.write((char *)&startupLog, sizeof(startupLog));
    f.close();
  }
}

// SETUP
void setup()
{
  // Debugging over serial terminal
  Serial.begin(115200);
  Serial.println("Booting");

  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.hostname(hostname);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    // Try again if connection failed
    Serial.println("Failed to connect to WiFi");
    delay(5000);
    Serial.println("Rebooting...");
    ESP.restart();
  }

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(hostname);

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  // OTA start callback
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
    {
      type = "sketch";
    }
    else
    {
      // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
    LittleFS.end();
  });

  // OTA end callback
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });

  // OTA progress callback
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  // OTA error callback
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
    {
      Serial.println("Auth Failed");
    }
    else if (error == OTA_BEGIN_ERROR)
    {
      Serial.println("Begin Failed");
    }
    else if (error == OTA_CONNECT_ERROR)
    {
      Serial.println("Connect Failed");
    }
    else if (error == OTA_RECEIVE_ERROR)
    {
      Serial.println("Receive Failed");
    }
    else if (error == OTA_END_ERROR)
    {
      Serial.println("End Failed");
    }
  });

  // Mount file system
  if (LittleFS.begin())
    Serial.println("File System Mounted OK");
  else
  {
    Serial.println("Formatting File System");
    bool ok = LittleFS.format();
    if (!ok)
      Serial.println("Failed to format file system");
    else if (!LittleFS.begin())
      Serial.println("File System not available");
  }

  // Initialize time library
  int i = 0;
  for (; i < 5; i++)
    if (myTZ.setLocation(F("Europe/London")))
      break;
  if (i == 5)
      Serial.println("Failed to setLocation(...)");
  if (!waitForSync(15000))
      Serial.println("Failed to set time");

  // Update the startup log
  updateStartupLog();

  // Server HTTP request for current status
  server.on("/", []() {
    const char *page = BuildStatusPage();
    server.send(200, "text/html", page);
  });

  // Server HTTP post
  server.on("/commands", HTTP_POST, []() {
    if (server.hasArg("recalibrate"))
    {
      server.send(200, "text/html", "Calibration data cleared. Restarting...");
      for (int i = 0; i < drivers_count; i++)
        drivers[i]->Recalibrate();
      delay(3000);
      ESP.reset();
      return;
    }

    if (server.hasArg("restart"))
    {
      server.send(200, "text/html", "Restarting...");
      delay(3000);
      ESP.reset();
      return;
    }

    server.send(400, "text/html", "Unknown Request");
  });

  // Allow OTA updates and server HTTP requests
  ArduinoOTA.begin();
  server.begin();

  // Report our IP
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Configure output for blue LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // DS18B20 sensors on GPIO14 D5
  ds.begin(14);

  // Configure I2C on (SDA GPIO0 D3) and (SCL GPIO5 D1)
  I2C.begin(0, 5);
  I2C.setClock(100000);

  // TODO:  Without this delay detection sometimes fails after power on
  delay(1000);

  // Create drivers for each of our sensors
  drivers_count += Bme680Driver::CreateDriverInstances(&I2C, &drivers[drivers_count], MAX_SENSOR_DRIVERS - drivers_count, BME680_TEMP_TRIM, BME680_TEMP_TRIM);
  drivers_count += Si705Driver::CreateDriverInstances(&I2C, &drivers[drivers_count], MAX_SENSOR_DRIVERS - drivers_count);
  drivers_count += Bh1750Driver::CreateDriverInstances(&I2C, &drivers[drivers_count], MAX_SENSOR_DRIVERS - drivers_count);
  drivers_count += Ds18b20Driver::CreateDriverInstances(&ds, &drivers[drivers_count], MAX_SENSOR_DRIVERS - drivers_count);
#if LDR_DRIVER
  drivers_count += LdrDriver::CreateDriverInstances(&drivers[drivers_count], MAX_SENSOR_DRIVERS - drivers_count);
#endif
}

long lastPollMillis = 0;
char lastPacket[512];
WiFiUDP udp;

// LOOP
void loop()
{
  // Give various services a chance to do their stuff
  ArduinoOTA.handle();
  server.handleClient();

  // Call handle() on all the sensors
  for (int i = 0; i < drivers_count; i++)
    drivers[i]->Handle();

  // Report last sensor outputs
  if ((unsigned long)(millis() - lastPollMillis) >= POLL_PERIOD_MS)
  {
    // Construct UDP packet (skip failed sensors)
    int packetLen = 0;
    for (int i = 0; i < drivers_count; i++)
      if (drivers[i]->IsLastReadingValid())
        packetLen += drivers[i]->GetPacketData(&lastPacket[packetLen]);
    packetLen++;
    Serial.print("*** PACKET (");
    Serial.print(packetLen);
    Serial.println(") ***");
    Serial.print(lastPacket);
    Serial.println("*** PACKET END ***");

    // Send packet
    udp.beginPacket(SERVER_IP, SERVER_PORT);
    udp.write(lastPacket);
    udp.endPacket();

    // Schedule next poll
    lastPollMillis = millis();
  }

#if FLASH_LED
  // Flash led for debug
  digitalWrite(LED_BUILTIN, HIGH);
  delay(200);
  digitalWrite(LED_BUILTIN, LOW);
  delay(200);
#endif
}
