#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

// LoRa pins
const int csPin = 5;     // Chip select for LoRa
const int resetPin = 15; // Reset for LoRa
const int irqPin = 33;   // Interrupt for LoRa

// Data fields
const char *device_id = "ESP32_Slave1";
float source_left;
float source_right;
float temperature;
float flow;

// Identifikasi Slave
const int slaveId = 5; // Ganti dengan ID slave sesuai

// Fungsi untuk mendapatkan data acak
float getRandomNumber()
{
  return random(100, 601) / 100.0; // 601 karena batas atas adalah 6.00, jadi kita tambahkan 1
}

// Fungsi untuk membaca serial number dari EEPROM
String readSerialNumberFromEEPROM()
{
  String serialNumber;
  for (int i = 0; i < 8; i++)
  {
    char ch = EEPROM.read(i);
    if (ch != 0)
    { // Asumsi serial number berakhir dengan null character
      serialNumber += ch;
    }
    else
    {
      break;
    }
  }
  return serialNumber;
}

// Inisialisasi LoRa
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

void sendData()
{
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
      // Membaca serial number dari EEPROM
      String serialNumber = readSerialNumberFromEEPROM();

      // Membuat objek JSON
      JsonDocument doc;
      doc["device_id"] = device_id;
      doc["serialnumber"] = serialNumber;
      doc["source_left"] = getRandomNumber();  // Contoh data
      doc["source_right"] = getRandomNumber(); // Contoh data
      doc["temperature"] = getRandomNumber();  // Contoh data
      doc["flow"] = getRandomNumber();         // Contoh data

      // Mengubah objek JSON menjadi string
      String jsonString;
      serializeJson(doc, jsonString);

      // Menampilkan JSON string
      Serial.println(jsonString);

      // Mengirim data melalui LoRa
      LoRa.beginPacket();
      LoRa.print(jsonString);
      LoRa.endPacket();
    }
  }
}

void setup()
{
  Serial.begin(115200);
  EEPROM.begin(512); // Inisialisasi EEPROM dengan kapasitas 512 bytes
  initializeLoRa();
}

void loop()
{
  sendData();
}
