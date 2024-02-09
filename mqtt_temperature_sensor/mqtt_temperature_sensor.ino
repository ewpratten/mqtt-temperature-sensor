// NOTE: For future readers, this is a gitignore-d file that contains passwords
#include "secrets.h"

// If building for ESP boards, we need WIFI support
#if defined(ARDUINO_ARCH_ESP8266)
  #include <ESP8266WiFi.h>
  WiFiClient network_client;
#else
  #include <Ethernet.h>
  EthernetClient network_client;
#endif

// HomeAssistant logic
// Source: https://github.com/dawidchyrzynski/arduino-home-assistant
#include <ArduinoHA.h>

// Libs for talking to the TMP102 board
#include <Wire.h>
#include <SparkFunTMP102.h>

// For getting a board ID
#include <ArduinoUniqueID.h>

// Define the Home Assistant MQTT client
HADevice ha_device;
HAMqtt ha_mqtt_client(network_client, ha_device);

// Define the connected sensors
TMP102 phy_temperature_sensor;
HASensorNumber ha_temperature_sensor("temperature", HASensorNumber::PrecisionP1);

// Timing
#define MEASURE_INTERVAL 15000
unsigned long last_iter_time = 0;

void setup()
{
  Serial.begin(9600);

  // Determine the board's serial number. Its stored in UniqueID8 by default
  char board_id[17] = {0};
  for (int i = 0; i < 8; i++)
  {
      sprintf(board_id + (i * 2), "%02X", UniqueID8[i]);
  }

  // Make a unique hostname for the device
  char hostname[50];
  sprintf(hostname, "Arduino-Sensor-%s", board_id);

  // Connect to the network
#if defined(ARDUINO_ARCH_ESP8266)
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.hostname(hostname);
  Serial.println(F("Waiting to join WIFI"));
  while (!WiFi.isConnected()) delay(500);
  Serial.println(F("WIFI Connected"));
#else
  byte mac_addr[] = {0xA8, 0x61, 0x0a, 0xae, 0x2d, 0xe2};
  Ethernet.begin(mac_addr);
  Serial.println(F("Waiting for ethernet"));
  while (Ethernet.linkStatus() != LinkON) delay(500);
  Serial.println(F("Ethernet ready"));
#endif

  // Set a Unique ID
  Serial.print(F("Board ID: ")); Serial.println(board_id);
  Serial.print(F("Device name: ")); Serial.println(hostname);
  ha_device.setUniqueId(UniqueID8, 8);
  ha_device.setName(hostname);

  // Set device info
#if defined(ARDUINO_ARCH_ESP8266)
  ha_device.setManufacturer("Espressif");
  ha_device.setModel("ESP-F");
#else
  ha_device.setManufacturer("Arduino");
  ha_device.setModel("Uno r3");
#endif

  // Configure the sensor
  ha_temperature_sensor.setIcon("mdi:thermometer");
  ha_temperature_sensor.setName("Temperature");
  ha_temperature_sensor.setUnitOfMeasurement("°C");
  ha_temperature_sensor.setForceUpdate(true);
  ha_temperature_sensor.setDeviceClass("temperature");

  // Connect to the MQTT broker
  ha_mqtt_client.begin("controller.home", MQTT_USERNAME, MQTT_PASSWORD);

  // Wait for an MQTT connection
  Serial.println(F("Waiting for MQTT connection"));
  ha_mqtt_client.loop();
  while (!ha_mqtt_client.isConnected()) delay(500);
  Serial.println(F("MQTT connected!"));
    
// Start I2C
#if defined(ARDUINO_ARCH_ESP8266)
  Wire.begin(13, 12);
#else
  Wire.begin();
#endif

  // Connect to the temperature sensor
  phy_temperature_sensor.begin();
  delay(100);
}

void loop()
{
#ifndef ARDUINO_ARCH_ESP8266
  Ethernet.maintain();
#endif

  // Chat with the MQTT broker
  ha_mqtt_client.loop();

  // If needed, do logic
  unsigned long now = millis();
  if (now - last_iter_time >= MEASURE_INTERVAL)
  {
    last_iter_time = now;

    // Wait for temperature data
    phy_temperature_sensor.oneShot(1);

    // Read sensors
    float temperature = phy_temperature_sensor.readTempC();
    Serial.println(temperature);

    // Write MQTT data
    ha_temperature_sensor.setValue(temperature);
  }
}