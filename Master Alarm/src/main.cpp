#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiManager.h>

#define TRIGGER_PIN 0
// LoRa pins
const int csPin = 5;     // Chip select for LoRa
const int resetPin = 15; // Reset for LoRa
const int irqPin = 33;   // Interrupt for LoRa

// MQTT settings
const char *mqtt_server = "test.mosquitto.org";
const int mqtt_port = 1883;
const char *mqtt_topic = "lora/data1";

WiFiClient espClient;
PubSubClient client(espClient);
WiFiManager wifiManager;
bool wm_nonblocking = false;

unsigned long lastLoRaTime = 0;
String receivedData = "";
bool awaitingResponse = false;
unsigned long responseTimeout = 180; // Timeout dalam milidetik
unsigned long requestTime = 0;       // Waktu saat MAC address dikirim

void initializeLoRa()
{
  LoRa.setPins(csPin, resetPin, irqPin);
  LoRa.setSpreadingFactor(7); // SF7 for maximum data rate
  LoRa.setSignalBandwidth(500E3);
  if (!LoRa.begin(433E6))
  {
    Serial.println("Starting LoRa failed!");
    while (1)
      ;
  }
}

void setupWiFi()
{
  pinMode(TRIGGER_PIN, INPUT);
  wifiManager.setClass("invert");
  wifiManager.autoConnect("ESP32-Setup");
}

void sendMACAddress()
{
  static int slaveId = 0;
  String macAddress = "0x00" + String(slaveId, HEX);

  LoRa.beginPacket();
  LoRa.print(macAddress);
  LoRa.endPacket();
  LoRa.receive();

  Serial.print("Sent MAC Address: ");
  Serial.println(macAddress);

  slaveId = (slaveId + 1) % 8;
  awaitingResponse = true; // Menunggu respons dari slave
  requestTime = millis();  // Catat waktu pengiriman
}

void sendToMQTT(const String &payload)
{
  if (client.connected())
  {
    if (client.publish(mqtt_topic, payload.c_str()))
    {
      Serial.print("Sent to MQTT: ");
      Serial.println(payload);
    }
    else
    {
      Serial.println("Failed to send to MQTT.");
    }
  }
  else
  {
    Serial.println("MQTT client not connected.");
  }
}

void handleReceivedData()
{
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, receivedData);

  if (!error)
  {
    // Send data to MQTT
    sendToMQTT(receivedData);
  }
  else
  {
    Serial.print("Failed to deserialize JSON: ");
    Serial.println(error.c_str());
  }

  // Clear receivedData
  receivedData = "";
}

void receiveData()
{
  int packetSize = LoRa.parsePacket();
  if (packetSize)
  {
    receivedData = ""; // Clear receivedData
    while (LoRa.available())
    {
      receivedData += (char)LoRa.read();
    }
    Serial.print("Received from Slave: ");
    Serial.println(receivedData);
    handleReceivedData();
    awaitingResponse = false; // Respons diterima, siap mengirim MAC berikutnya
  }
}

void checkResponseTimeout()
{
  if (awaitingResponse && (millis() - requestTime >= responseTimeout))
  {
    Serial.println("No response from slave, moving to next slave.");
    awaitingResponse = false; // Timeout, siap mengirim MAC berikutnya
  }
}

void checkButton()
{
  // check for button press
  if (digitalRead(TRIGGER_PIN) == LOW)
  {
    // poor mans debounce/press-hold, code not ideal for production
    delay(50);
    if (digitalRead(TRIGGER_PIN) == LOW)
    {
      Serial.println("Button Pressed");
      // still holding button for 3000 ms, reset settings, code not ideaa for production
      delay(3000); // reset delay hold
      if (digitalRead(TRIGGER_PIN) == LOW)
      {
        Serial.println("Button Held");
        Serial.println("Erasing Config, restarting");
        wifiManager.resetSettings();
        ESP.restart();
      }

      // start portal w delay
      Serial.println("Starting config portal");
      wifiManager.setConfigPortalTimeout(10);

      if (!wifiManager.startConfigPortal("OnDemandAP", "password"))
      {
        Serial.println("failed to connect or hit timeout");
        // ESP.restart();
      }
      else
      {
        // if you get here you have connected to the WiFi
        Serial.println("connected...yeey :)");
      }
    }
  }
}

void handleMQTTConnection()
{
    if (!client.connected())
    {
        // Serial.print("Attempting MQTT connection...");
        if (client.connect("ESP32Master"))
        {
            Serial.println("connected");
        }
        else
        {
            Serial.println("failed, rc=");
            Serial.print(client.state());
            // Serial.println(" try again in 5 seconds");
            // // delay(5000);
        }
    }
    client.loop();
}

void setup()
{
  Serial.begin(115200);

  // Initialize LoRa
  initializeLoRa();

  // Setup WiFi Manager
  setupWiFi();

  // Connect to MQTT
  client.setServer(mqtt_server, mqtt_port);
}

void loop()
{

  checkButton();
  // Terima data dari slave
  receiveData();
  handleMQTTConnection();

  // Cek apakah waktu tunggu respons dari slave sudah habis
  checkResponseTimeout();

  if (wm_nonblocking)
    wifiManager.process();

  // Jika tidak sedang menunggu respons dari slave, kirim MAC address
  if (!awaitingResponse)
  {
    sendMACAddress();
  }
}
