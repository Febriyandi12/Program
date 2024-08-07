#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>

// LoRa pins
const int csPin = 5;     // Chip select for LoRa
const int resetPin = 15; // Reset for LoRa
const int irqPin = 33;   // Interrupt for LoRa

long randNumber;
// Data fields
#include <random>

const char *device_id = "ESP32_Slave2";
float source_left;
float source_right;
float temperature;
float flow;

// Identifikasi Slave
const int slaveId = 1; // Ganti dengan ID slave sesuai

float getRandomNumber()
{
  return random(100, 601) / 100.0; // 601 karena batas atas adalah 6.00, jadi kita tambahkan 1
}

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
void sendData()
{

  float randomNumber = getRandomNumber();
  JsonDocument doc;
  doc["device_id"] = device_id;
  doc["source_left"] = random(100, 601) / 100.0;  // Contoh data
  doc["source_right"] = random(100, 601) / 100.0; // Contoh data
  doc["temperature"] = random(100, 601) / 100.0;  // Contoh data
  doc["flow"] = random(100, 601) / 100.0;         // Contoh data

  String jsonString;
  serializeJson(doc, jsonString);

  LoRa.beginPacket();
  LoRa.print(jsonString);
  LoRa.endPacket();

  Serial.print("Sent data: ");
  Serial.println(jsonString);
}

void setup()
{
  Serial.begin(115200);
  initializeLoRa();
}

void loop()
{
  // Memeriksa apakah ada paket yang diterima

  int packetSize = LoRa.parsePacket();
  if (packetSize)
  {
    String receivedData = "";
    while (LoRa.available())
    {
      receivedData += (char)LoRa.read();
    }

    // Menampilkan data yang diterima
    Serial.print("Received: ");
    Serial.println(receivedData);

    // Memeriksa MAC address yang diterima
    if (receivedData == "0x00" + String(slaveId, HEX))
    {
      // Membuat objek JSON
      JsonDocument doc;
      doc["device_id"] = device_id;
      doc["source_left"] = random(100, 601) / 100.0;  // Contoh data
      doc["source_right"] = random(100, 601) / 100.0; // Contoh data
      doc["temperature"] = random(100, 601) / 100.0;  // Contoh data
      doc["flow"] = random(100, 601) / 100.0;         // Contoh data

      // Mengubah objek JSON menjadi string
      String jsonString;
      serializeJson(doc, jsonString);

      // Menampilkan JSON string
      Serial.println(jsonString);
// delay(20);
      // Mengirim data melalui LoRa
      LoRa.beginPacket();
      LoRa.print(jsonString);
      LoRa.endPacket();
    }
  }
}
