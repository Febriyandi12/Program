#include <SPI.h>
#include <Arduino.h>
#include <SoftwareSerial.h>
#include <Nextion.h>
#include <FS.h>
#include <SD.h>
#include <EEPROM.h>
#include <TaskScheduler.h>
#include <esp_system.h>
#include <LoRa.h>
#include <ArduinoJson.h>
#include <WiFi.h>

const int csPin = 5;     // LoRa radio chip select
const int resetPin = 14; // LoRa radio reset
const int irqPin = 2;    // LoRa radio interrupt

const uint8_t masterAddress = 0x10;
const char *slaveMacs[] = {
    "OX-1111", // Oksigen
    "NO-1111", // Nitrous Oxide
    "CO-1111", // Carbon Dioxide
    "KP-1111", // Kompressor
    "VK-1111", // Vakum
    "NG-1111"  // Nitrogen
};

int numSlaves = sizeof(slaveMacs) / sizeof(slaveMacs[0]);

// Waktu tunggu respons dari slave
const long timeout = 70; // 70 ms timeout

// Struktur untuk menyimpan data dari setiap slave
struct SlaveData
{
  String mac;
  float supply;
  float leftBank;
  float rightBank;
  bool hasMultipleBanks;
};

// Array untuk menyimpan data dari setiap slave
SlaveData slaveData[6];

Scheduler runner;
void t1Callback();
void t2Callback();
// void t3Callback();
// void t4Callback();

// Task definitions
Task t1(0, TASK_FOREVER, &t1Callback);
Task t2(1000, TASK_FOREVER, &t2Callback);
// Task t3(1000, TASK_FOREVER, &t3Callback);
// Task t4(1000, TASK_FOREVER, &t4Callback);

// Variabel
int currentSlaveIndex = 0;
bool awaitingResponse = false;
unsigned long requestTime = 0;

String receivedData = "";
byte eepromAddressUnit = 28;
uint8_t lastSelectedButton = 0;

int unitState; // 1 = Bar, 2 = KPa, 3 = Psi
int currentState = 0;
int statebutton;
// Buffer untuk menyimpan nilai
char data0[10], data1[10], data2[10], data3[10], data4[10], data5[10], data6[10], data7[10], data8[10];
const char *nilai[] = {data0, data1, data2, data3, data4, data5, data6, data7, data8};

float sensorValues[6] = {6.0, 6.0, 6.0, 6.0, 6.0, 6.0};

// Satuan, Status, Gas, dan Keterangan
const char *satuan = "Bar";
const char *satuan1 = "mmHg";
const char *satuan2 = "KPa";
const char *satuan3 = "Psi";

const char *status[] = {"Normal", "Low", "Over"};
const char *gas[] = {"Oxygen", "Nitrous Oxide", "Carbondioxide", "Kompressor", "Vakum", "Nitrogen"};
const char *ket[] = {"Supply", "Gas", "Left Bank", "Right Bank", "Medical Air", "Tools Air"};

const char *bar = "1";
const char *kpa = "1";
const char *psi = "1";

// Nextion objects
NexButton bpagelogin = NexButton(0, 18, "b0");
NexButton bpagehome = NexButton(1, 5, "b1");
NexButton bsubmit = NexButton(1, 4, "b0");
NexButton balarmsetting = NexButton(1, 25, "b2");
NexButton blogin = NexButton(2, 4, "b0");
NexButton btutup = NexButton(2, 5, "b1");
NexButton bback = NexButton(3, 7, "b1");
// NexButton bsetting = NexButton(3, 8, "b0");
NexDSButton bBar = NexDSButton(1, 21, "bt0");
NexDSButton bKPa = NexDSButton(1, 22, "bt1");
NexDSButton bPSi = NexDSButton(1, 23, "bt0");

NexPage settingPage = NexPage(1, 0, "menusetting");
NexPage home = NexPage(0, 0, "mainmenu3");
NexPage loginpage = NexPage(2, 0, "loginpage");
NexPage alarmpage = NexPage(4, 0, "settingalarm");
NexText password = NexText(2, 3, "tpassword");
NexText alert = NexText(2, 1, "t1");

NexNumber setPassword = NexNumber(1, 17, "npassword");

NexNumber inputnumber1 = NexNumber(1, 1, "n0");
NexNumber inputnumber2 = NexNumber(1, 2, "n1");
NexNumber inputnumber3 = NexNumber(1, 3, "n2");
NexNumber inputnumber4 = NexNumber(1, 11, "n3");
NexNumber inputnumber5 = NexNumber(1, 12, "n4");
NexNumber inputnumber6 = NexNumber(1, 15, "n5");

NexNumber xminsensor = NexNumber(3, 8, "xminsensor");
NexNumber xminvakum = NexNumber(3, 10, "xminvakum");
NexNumber xminkompressor = NexNumber(3, 11, "xminkompressor");
NexNumber xmaxsensor = NexNumber(3, 9, "xmaxsensor");

NexText backgorund[6] = {
    NexText(0, 3, "bg0"),
    NexText(0, 2, "bg1"),
    NexText(0, 1, "bg2"),
    NexText(0, 28, "bg3"),
    NexText(0, 27, "bg4"),
    NexText(0, 63, "bg5")};

NexText gasText[6] = {
    NexText(0, 24, "tGas0"),
    NexText(0, 25, "tGas1"),
    NexText(0, 26, "tGas2"),
    NexText(0, 41, "tGas3"),
    NexText(0, 42, "tGas4"),
    NexText(0, 70, "tGas5")};

NexText satuanText[6][3] = {
    {NexText(0, 55, "tsatuan0_0"), NexText(0, 56, "tsatuan1_0"), NexText(0, 57, "tsatuan2_0")},
    {NexText(0, 54, "tsatuan0_1"), NexText(0, 53, "tsatuan1_1"), NexText(0, 52, "tsatuan2_1")},
    {NexText(0, 51, "tsatuan0_2"), NexText(0, 50, "tsatuan1_2"), NexText(0, 49, "tsatuan2_2")},
    {NexText(0, 43, "tsatuan0_3"), NexText(0, 44, "tsatuan1_3"), NexText(0, 45, "tsatuan2_3")},
    {NexText(0, 48, "tsatuan0_4"), NexText(0, 47, "tsatuan1_4"), NexText(0, 46, "tsatuan2_4")},
    {NexText(0, 73, "tsatuan0_5"), NexText(0, 72, "tsatuan1_5"), NexText(0, 71, "tsatuan2_5")}};

NexText supplyText[6] = {
    NexText(0, 6, "tsupply0"),
    NexText(0, 11, "tsupply1"),
    NexText(0, 13, "tsupply2"),
    NexText(0, 30, "tsupply3"),
    NexText(0, 36, "tsupply4"),
    NexText(0, 65, "tsupply5")};

NexText ketText[6][2] = {
    {NexText(0, 6, "tket0_0"), NexText(0, 7, "tket1_0")},
    {NexText(0, 20, "tket0_1"), NexText(0, 19, "tket1_1")},
    {NexText(0, 14, "tket0_2"), NexText(0, 15, "tket1_2")},
    {NexText(0, 31, "tket0_3"), NexText(0, 32, "tket1_3")},
    {NexText(0, 38, "tket0_4"), NexText(0, 37, "tket1_4")},
    {NexText(0, 67, "tket0_5"), NexText(0, 66, "tket1_5")}};

NexText nilaiText[6][2] = {
    {NexText(0, 8, "nilai0_0"), NexText(0, 9, "nilai1_0")},
    {NexText(0, 21, "nilai0_1"), NexText(0, 22, "nilai1_1")},
    {NexText(0, 16, "nilai0_2"), NexText(0, 17, "nilai1_2")},
    {NexText(0, 33, "nilai0_3"), NexText(0, 34, "nilai1_3")},
    {NexText(0, 39, "nilai0_4"), NexText(0, 40, "nilai1_4")},
    {NexText(0, 68, "nilai0_5"), NexText(0, 69, "nilai1_5")}};

NexText valueText[6] = {
    NexText(0, 4, "value0"),
    NexText(0, 10, "value1"),
    NexText(0, 12, "value2"),
    NexText(0, 29, "value3"),
    NexText(0, 35, "value4"),
    NexText(0, 64, "value5")};

NexTouch *nex_listen_list[] = {&bpagelogin, &bpagehome, &settingPage, &bsubmit, &balarmsetting, &blogin, &btutup, &bBar, &bKPa, &bPSi, &bback, &xminvakum, &xminsensor, &xminkompressor, &xmaxsensor, NULL};

String perhitunganStatus(float value)
{
  if (value < 4.00)
  {
    return "low";
  }
  else if (value > 8.00)
  {
    return "high";
  }
  else
  {
    return "normal";
  }
}

void saveButtonState(uint8_t buttonNumber)
{
  EEPROM.write(eepromAddressUnit, buttonNumber);
  EEPROM.commit();
}

void writeEEprom()
{
  // Array untuk menyimpan semua nilai byte
  byte values[11];

  uint32_t tempValue; // Variabel sementara untuk menyimpan nilai dari Nextion
  uint32_t Xfloat;
  // Ambil nilai dari komponen Nextion
  setPassword.getValue(&tempValue);
  EEPROM.put(38, tempValue);

  xminsensor.getValue(&Xfloat);
  EEPROM.put(10, Xfloat);

  xminvakum.getValue(&Xfloat);
  EEPROM.put(13, Xfloat);

  xminkompressor.getValue(&Xfloat);
  EEPROM.put(16, Xfloat);

  xmaxsensor.getValue(&Xfloat);
  EEPROM.put(19, tempValue);

  inputnumber1.getValue(&tempValue);
  values[0] = (byte)tempValue;

  inputnumber2.getValue(&tempValue);
  values[1] = (byte)tempValue;

  inputnumber3.getValue(&tempValue);
  values[2] = (byte)tempValue;

  inputnumber4.getValue(&tempValue);
  values[3] = (byte)tempValue;

  inputnumber5.getValue(&tempValue);
  values[4] = (byte)tempValue;

  inputnumber6.getValue(&tempValue);
  values[5] = (byte)tempValue;

  // xminsensor.getValue(&tempValue);
  // values[7] = (byte)tempValue;

  // xminvakum.getValue(&tempValue);
  // values[8] = (byte)tempValue;

  // xminkompressor.getValue(&tempValue);
  // values[9] = (byte)tempValue;

  // xmaxsensor.getValue(&tempValue);
  // values[10] = (byte)tempValue;

  // Tulis semua nilai ke EEPROM
  for (int i = 0; i < 11; i++)
  {
    EEPROM.write(i, values[i]);
  }

  EEPROM.commit(); // Simpan semua perubahan ke EEPROM

  // Cetak nilai untuk memastikan mereka ditulis dengan benar
  // Serial.println("Tulis input");
  // Serial.println(values[10]);
  // Serial.println(values[13]);
  // Serial.println(values[16]);
  // Serial.println(values[19]);
}

void readEEprom()
{
  // Array untuk menyimpan semua nilai byte
  byte values[11];
  uint32_t setpassword;
  EEPROM.get(38, setpassword);
  int sandi = setpassword;
  // Baca semua nilai dari EEPROM
  for (int i = 0; i < 11; i++)
  {
    values[i] = EEPROM.read(i);
  }

  // Set nilai pada komponen Nextion
  inputnumber1.setValue(values[0]);
  inputnumber2.setValue(values[1]);
  inputnumber3.setValue(values[2]);
  inputnumber4.setValue(values[3]);
  inputnumber5.setValue(values[4]);
  inputnumber6.setValue(values[5]);
  setPassword.setValue(sandi);

  xminsensor.setValue(values[10]);
  xminvakum.setValue(values[13]);
  xminkompressor.setValue(values[16]);
  xmaxsensor.setValue(values[19]);

  // Cetak nilai yang dibaca untuk verifikasi
}

// Function to read EEPROM values
void readEEPROMValues(byte *numberValue)
{
  numberValue[0] = EEPROM.read(0);
  numberValue[1] = EEPROM.read(1);
  numberValue[2] = EEPROM.read(2);
  numberValue[3] = EEPROM.read(3);
  numberValue[4] = EEPROM.read(4);
  numberValue[5] = EEPROM.read(5);
}

// Fungsi untuk mendapatkan status tombol dari EEPROM

// Fungsi untuk memperbarui warna tombol berdasarkan tombol yang dipilih
void updateButtonColors(uint8_t selectedButton)
{
  selectedButton = EEPROM.read(16);
  uint32_t colorInactive = 65535; // Warna default (misalnya putih)
  uint32_t colorActive = 2016;    // Warna aktif (misalnya biru)

  // Perbarui warna untuk masing-masing tombol
  bBar.Set_state0_color_bco0(selectedButton == 1 ? colorActive : colorInactive);
  bKPa.Set_state0_color_bco0(selectedButton == 2 ? colorActive : colorInactive);
  bPSi.Set_state0_color_bco0(selectedButton == 3 ? colorActive : colorInactive);
}

void clearinputnilai()
{
  inputnumber1.setValue(0);
  inputnumber2.setValue(0);
  inputnumber3.setValue(0);
  inputnumber4.setValue(0);
  inputnumber5.setValue(0);
  inputnumber6.setValue(0);
}

void handleReceivedData()
{
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, receivedData);

  const char *device_id = doc["device_id"];
  // const char *serialNumber = doc["serialnumber"];
  const char *source_left = doc["source_left"];
  const char *source_right = doc["source_right"];
  const char *temperature = doc["temperature"];
  const char *flow = doc["flow"];

  const char *serialNumber = doc["serialnumber"];

  // Clear receivedData
  receivedData = "";
}

void bPageloginPopCallback(void *ptr) // menu setting home
{
  t2.disable();
  loginpage.show();
}

void bSubmitPopCallback(void *ptr)
{
  writeEEprom();
}

void bpPasswordPopCallback(void *ptr) // button submit password
{
  uint32_t setpassword;
  EEPROM.get(38, setpassword);

  char passwordValue[6]; // Buffer untuk menyimpan teks password
  int sandi = setpassword;

  password.getText(passwordValue, sizeof(passwordValue)); // Ambil teks dari input Nextion
  Serial.println(setpassword);                            // Cetak teks password untuk debug

  int passwordInt = atoi(passwordValue); // Konversi teks menjadi integer

  if (passwordInt == sandi) // Bandingkan nilai integer
  {
    // t1.disable();
    t2.disable();
    password.setText("");

    settingPage.show();
    readEEprom();
    updateButtonColors(1);
  }
  else
  {
    Serial.println("Password salah"); // Pesan jika password salah
  }
}

void balarmsettingPopCallback(void *ptr) // button back home
{
  alarmpage.show();
}

void bbackPopCallback(void *ptr) // button back home
{
  settingPage.show();
}

void bPagehomePopCallback(void *ptr) // button back home
{
  home.show();
  esp_restart();
  clearinputnilai();
}

void bBarPopCallback(void *ptr)
{
  lastSelectedButton = 1;
  saveButtonState(lastSelectedButton);
  updateButtonColors(lastSelectedButton);
}

void bKPaPopCallback(void *ptr)
{
  lastSelectedButton = 2;
  saveButtonState(lastSelectedButton);
  updateButtonColors(lastSelectedButton);
}

void bPSiPopCallback(void *ptr)
{
  lastSelectedButton = 3;
  saveButtonState(lastSelectedButton);
  updateButtonColors(lastSelectedButton);
}


// Function to update the display based on the gas type
void updateGasUI(int currentIndex, byte gasType)
{
  byte currentIndexByte = (byte)currentIndex;
  readEEPROMValues(&currentIndexByte);
  unitState = EEPROM.read(eepromAddressUnit);

  const char *selectedSatuan;

  // Menentukan satuan berdasarkan unitState yang dibaca dari EEPROM
  if (unitState == 2)
  {
    selectedSatuan = satuan2; // KPa
  }
  else if (unitState == 3)
  {
    selectedSatuan = satuan3; // Psi
  }
  else
  {
    selectedSatuan = satuan; // Default Bar
  }

  // Setel gambar latar belakang
  backgorund[currentIndex].Set_background_image_pic(6);

  // Setel teks gas
  gasText[currentIndex].setText(gas[gasType - 1]);

  // Reset nilai teks satuan
  satuanText[currentIndex][0].setText(selectedSatuan);
  satuanText[currentIndex][1].setText("");
  satuanText[currentIndex][2].setText("");

  // Reset teks status dan supply
  supplyText[currentIndex].setText(ket[0]); // Default ke "Supply"

  switch (gasType)
  {
  case 1: // Oxygen
          // Tampilkan semua nilai

    supplyText[currentIndex].setText(ket[0]);            // Default ke "Supply"
    ketText[currentIndex][0].setText(ket[2]);            // Left Bank
    ketText[currentIndex][1].setText(ket[3]);            // Right Bank
    satuanText[currentIndex][1].setText(selectedSatuan); // Satuan Left Bank
    satuanText[currentIndex][2].setText(selectedSatuan); // Satuan Right Bank
    break;
  case 2: // Nitrous Oxide

    supplyText[currentIndex].setText(ket[0]);            // Default ke "Supply"
    ketText[currentIndex][0].setText(ket[2]);            // Left Bank
    ketText[currentIndex][1].setText(ket[3]);            // Right Bank
    satuanText[currentIndex][1].setText(selectedSatuan); // Satuan Left Bank
    satuanText[currentIndex][2].setText(selectedSatuan);
    break;
  case 3: // Carbondioxide

    supplyText[currentIndex].setText(ket[0]);            // Default ke "Supply"
    ketText[currentIndex][0].setText(ket[2]);            // Left Bank
    ketText[currentIndex][1].setText(ket[3]);            // Right Bank
    satuanText[currentIndex][1].setText(selectedSatuan); // Satuan Left Bank
    satuanText[currentIndex][2].setText(selectedSatuan);
    break;
  case 6: // Nitrogen

    supplyText[currentIndex].setText(ket[0]);            // Default ke "Supply"
    ketText[currentIndex][0].setText(ket[2]);            // Left Bank
    ketText[currentIndex][1].setText(ket[3]);            // Right Bank
    satuanText[currentIndex][1].setText(selectedSatuan); // Satuan Left Bank
    satuanText[currentIndex][2].setText(selectedSatuan);
    break;

  case 4: // Kompressor

    supplyText[currentIndex].setText(ket[0]); // Default ke "Supply"
    break;

  case 5: // Vakum
          // Hanya tampilkan nilai utama

    supplyText[currentIndex].setText(ket[0]); // Default ke "Supply"
    satuanText[currentIndex][0].setText(satuan1);
    break;
  }

  // Setel warna latar belakang dan teks
  uint16_t backgroundColor = 14791;

  nilaiText[currentIndex][0].Set_background_color_bco(backgroundColor);
  nilaiText[currentIndex][1].Set_background_color_bco(backgroundColor);

  valueText[currentIndex].Set_background_color_bco(backgroundColor);

  satuanText[currentIndex][0].Set_background_color_bco(backgroundColor);
  satuanText[currentIndex][1].Set_background_color_bco(backgroundColor);
  satuanText[currentIndex][2].Set_background_color_bco(backgroundColor);

  supplyText[currentIndex].Set_background_color_bco(backgroundColor);

  ketText[currentIndex][0].Set_background_color_bco(backgroundColor);
  ketText[currentIndex][1].Set_background_color_bco(backgroundColor);
}

// Function to clear unused displays
void clearUnusedDisplays(int currentIndex)
{
  for (int i = currentIndex; i < 6; i++)
  {

    gasText[i].setText("-");
    valueText[i].setText("");
    supplyText[i].setText("");
    ketText[i][0].setText("");
    ketText[i][1].setText("");
    satuanText[i][0].setText("");
    satuanText[i][1].setText("");
    satuanText[i][2].setText("");

    gasText[i].Set_font_color_pco(0);
    valueText[i].Set_background_color_bco(0);
    gasText[i].Set_background_color_bco(0);
    satuanText[i][0].Set_background_color_bco(0);
    satuanText[i][1].Set_background_color_bco(0);
    satuanText[i][2].Set_background_color_bco(0);
    supplyText[i].Set_background_color_bco(0);
    ketText[i][0].Set_background_color_bco(0);
    ketText[i][1].Set_background_color_bco(0);
  }
}

void t1Callback()
{
  if (!awaitingResponse)
  {
    // Kirim permintaan data ke slave
    const char *slaveMac = slaveMacs[currentSlaveIndex];
    LoRa.beginPacket();
    LoRa.print(masterAddress); // Kirim alamat master
    LoRa.print(", ");
    LoRa.print(slaveMac); // Kirim MAC slave
    LoRa.endPacket();
    LoRa.receive();

    awaitingResponse = true;
    requestTime = millis();
  }
  else
  {
    // Tunggu respons dari slave
    if (millis() - requestTime < timeout)
    {
      int packetSize = LoRa.parsePacket();
      if (packetSize)
      {
        uint8_t receivedAddress = LoRa.read();

        if (receivedAddress == masterAddress)
        {

          // Membaca data dari slave dan menyimpannya berdasarkan slaveMac
          String mac = "";
          String value1 = "", value2 = "", value3 = "";
          bool parsingMac = true, parsingValue1 = false, parsingValue2 = false, parsingValue3 = false;

          while (LoRa.available())
          {
            char c = (char)LoRa.read();
            Serial.print(c);

            if (c == ',')
            {
              if (parsingMac)
              {
                parsingMac = false;
                parsingValue1 = true;
              }
              else if (parsingValue1)
              {
                parsingValue1 = false;
                parsingValue2 = true;
              }
              else if (parsingValue2)
              {
                parsingValue2 = false;
                parsingValue3 = true;
              }
            }
            else
            {
              if (parsingMac)
              {
                mac += c;
              }
              else if (parsingValue1)
              {
                value1 += c;
              }
              else if (parsingValue2)
              {
                value2 += c;
              }
              else if (parsingValue3)
              {
                value3 += c;
              }
            }
          }
          // Serial.println();

          // Simpan data ke array berdasarkan mac
          for (int i = 0; i < numSlaves; i++)
          {
            if (slaveData[i].mac == mac)
            {
              slaveData[i].supply = value1.toFloat();
              slaveData[i].leftBank = value2.toFloat();
              slaveData[i].rightBank = value3.toFloat();
              Serial.print("Data tersimpan untuk ");
              Serial.println(slaveData[i].mac);
              Serial.print("Nilai 1: ");
              Serial.println(slaveData[i].supply);
              Serial.print("Nilai 2: ");
              Serial.println(slaveData[i].leftBank);
              Serial.print("Nilai 3: ");
              Serial.println(slaveData[i].rightBank);
              break;
            }
          }

          awaitingResponse = false;
          currentSlaveIndex = (currentSlaveIndex + 1) % numSlaves; // Berpindah ke slave berikutnya
        }
      }
    }
    else
    {
      awaitingResponse = false;
      currentSlaveIndex = (currentSlaveIndex + 1) % numSlaves; // Berpindah ke slave berikutnya
    }
  }
}

void t2Callback()
{
    byte numberValue[6];
    readEEPROMValues(numberValue);

    int currentIndex = 0;
    for (int i = 0; i < 6; i++)
    {
        if (numberValue[i] >= 1 && numberValue[i] <= 6)
        {
            int slaveIndex = numberValue[i] - 1; // Sesuaikan index ke 0-based untuk slaveData

            // Set nilai supply
            snprintf((char *)nilai[currentIndex], sizeof(data0), "%.2f", slaveData[slaveIndex].supply);
            valueText[currentIndex].setText(nilai[currentIndex]);

            // Update nilai hanya jika ada leftBank dan rightBank yang relevan
            if (slaveData[slaveIndex].hasMultipleBanks)
            {
                snprintf((char *)data1, sizeof(data1), "%.2f", slaveData[slaveIndex].leftBank);
                snprintf((char *)data2, sizeof(data2), "%.2f", slaveData[slaveIndex].rightBank);
                nilaiText[currentIndex][0].setText(data1);
                nilaiText[currentIndex][1].setText(data2);
            }
            else
            {
                // Jika tidak ada leftBank dan rightBank, kosongkan tampilan
                nilaiText[currentIndex][0].setText(" ");
                nilaiText[currentIndex][1].setText(" ");
            }

            // Tentukan status berdasarkan nilai sensor
            if (slaveData[slaveIndex].supply < 4.00)
            {
                supplyText[currentIndex].Set_background_color_bco(60516); // Low
            }
            else if (slaveData[slaveIndex].supply >= 7.00)
            {
                supplyText[currentIndex].Set_background_color_bco(63488); // High
            }
            else
            {
                supplyText[currentIndex].Set_background_color_bco(2016); // Normal
            }

            currentIndex++;
        }
        else
        {
            // Kosongkan nilai jika tidak ada data yang relevan
            valueText[currentIndex].setText(" ");
            nilaiText[currentIndex][0].setText(" ");
            nilaiText[currentIndex][1].setText(" ");
        }
    }

    // Kosongkan sisa tampilan jika currentIndex tidak mencapai 6
    for (; currentIndex < 6; currentIndex++)
    {
        valueText[currentIndex].setText(" ");
        nilaiText[currentIndex][0].setText(" ");
        nilaiText[currentIndex][1].setText(" ");
    }
}


void readAndProcessEEPROMValues(byte *numberValue, int &currentIndex)
{
  readEEPROMValues(numberValue);
  currentIndex = 0;
  for (int i = 0; i < 6; i++)
  {
    if (numberValue[i] >= 1 && numberValue[i] <= 6)
    {
      updateGasUI(currentIndex, numberValue[i]);
      currentIndex++;
    }
  }
}

void setup()
{
  nexInit();
  clearUnusedDisplays(0);
  Serial.begin(115200);
  EEPROM.begin(350);
  
  runner.init();
  runner.addTask(t1);
  runner.addTask(t2);
  t1.enable();
  t2.enable();
 
  LoRa.setPins(csPin, resetPin, irqPin);

  LoRa.setPins(csPin, resetPin, irqPin);
  if (!LoRa.begin(433E6))
  {
    Serial.println("Gagal menginisialisasi LoRa");
    while (1)
      ;
  }

  Serial.println("LoRa Master Initialized, Ready");

  blogin.attachPop(bpPasswordPopCallback, &blogin);
  bpagelogin.attachPop(bPageloginPopCallback, &bpagelogin);
  bsubmit.attachPop(bSubmitPopCallback, &bsubmit);
  btutup.attachPop(bPagehomePopCallback, &btutup);
  bpagehome.attachPop(bPagehomePopCallback, &bpagehome);
  balarmsetting.attachPop(balarmsettingPopCallback, &balarmsetting);

  bback.attachPop(bbackPopCallback, &bback);
  bBar.attachPop(bBarPopCallback, &bBar);
  bKPa.attachPop(bKPaPopCallback, &bKPa);
  bPSi.attachPop(bPSiPopCallback, &bPSi);

  for (int i = 0; i < numSlaves; i++)
  {
    slaveData[i].mac = slaveMacs[i];

    // Tentukan apakah slave memiliki multiple banks
    slaveData[i].hasMultipleBanks = (i != 3 && i != 4); // 3 dan 4 adalah Kompressor dan Vakum
  }

  byte numberValue[6];
  int currentIndex = 0;
  readAndProcessEEPROMValues(numberValue, currentIndex);

  // Perbarui warna tombol berdasarkan status terakhir
  updateButtonColors(lastSelectedButton);


  for (int i = 0; i < numSlaves; i++)
  {
    slaveData[i].mac = slaveMacs[i];
  }
}

void loop()
{

  nexLoop(nex_listen_list);
  runner.execute();
}
