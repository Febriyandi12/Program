#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiManager.h>

#define TRIGGER_PIN 0

// LoRa pins
const int csPin = 5;     // Chip select for LoRa
const int resetPin = 15; // Reset untuk LoRa
const int irqPin = 33;   // Interrupt untuk LoRa

// MQTT settings
const char *mqttServer = "test.mosquitto.org";
const int mqttPort = 1883;

WiFiClient espClient;
PubSubClient client(espClient);
WiFiManager wifiManager;
bool wmNonblocking = false;

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
  LoRa.setTxPower(20);
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

void sendToMQTT(const String &payload, const String &topic)
{
  if (client.connected())
  {
    if (client.publish(topic.c_str(), payload.c_str()))
    {
      Serial.print("Sent to MQTT: ");
      Serial.println(payload);
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
    const char *serialNumber = doc["serialnumber"];
    String topic = "lora/data/" + String(serialNumber);

    // Send data to MQTT
    sendToMQTT(receivedData, topic);
  }
  else
  {
    Serial.print("Failed to deserialize JSON: ");
    Serial.println(error.c_str());
  }

  // Clear receivedData
  receivedData = "";
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
    handleReceivedData();
    awaitingResponse = false; // Respons diterima, siap mengirim MAC berikutnya
  }
}

void checkResponseTimeout()
{
  if (awaitingResponse && (millis() - requestTime >= responseTimeout))
  {
    Serial.println("No response from slave");
    awaitingResponse = false; // Timeout, siap mengirim MAC berikutnya
  }
}

void checkButton()
{
  if (digitalRead(TRIGGER_PIN) == LOW)
  {
    delay(50);
    if (digitalRead(TRIGGER_PIN) == LOW)
    {
      delay(3000);
      if (digitalRead(TRIGGER_PIN) == LOW)
      {
        wifiManager.resetSettings();
        ESP.restart();
      }

      wifiManager.setConfigPortalTimeout(10);

      if (!wifiManager.startConfigPortal("OnDemandAP", "password"))
      {
        Serial.println("Failed to connect or hit timeout");
      }
    }
  }
}

void handleMQTTConnection()
{
  if (!client.connected())
  {
    if (client.connect("ESP32Master"))
    {
      Serial.println("MQTT connected");
    }
    else
    {
      Serial.print("MQTT connection failed, rc=");
      Serial.print(client.state());
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
  client.setServer(mqttServer, mqttPort);
}

void loop()
{
  checkButton();
  receiveData();
  handleMQTTConnection();
  checkResponseTimeout();

  if (wmNonblocking)
    wifiManager.process();

  if (!awaitingResponse)
  {
    sendMACAddress();
  }
}
