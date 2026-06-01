/*****************************************************************
 * WATER QUALITY MONITORING SYSTEM (WQMS)
 * SISTEM MONITORING KUALITAS AIR KOLAM IKAN BERBASIS IoT
 *
 * Nama  : Yusuf Hidayat
 * NIM   : 1152625002
 * Prodi : Teknik Informatika
 * Kampus: Institut Teknologi Indonesia
 * Tahun : 2026
 *
 * Judul Skripsi :
 * Perancangan dan Implementasi Sistem Kolam Ikan Pintar
 * Berbasis Internet of Things (IoT) dengan Analisis Kinerja
 * Real-Time
 *
 * Perangkat:
 * - ESP32
 * - Sensor TDS/EC
 * - Sensor Suhu DS18B20
 * - Sensor pH
 * - OLED Display
 * - Blynk IoT
 * - Telegram Bot
 *
 * Fitur:
 * - Membaca nilai TDS, EC, suhu, dan pH
 * - Menampilkan data pada OLED Display
 * - Mengirim data ke Blynk IoT
 * - Mengirim data ke Telegram Bot
 * - Memberikan notifikasi jika kualitas air tidak normal
 *
 * Tahun : 2026
 *****************************************************************/

#define BLYNK_TEMPLATE_ID "TMPL6Q7xqwBpW"
#define BLYNK_TEMPLATE_NAME "Water Quality Monitoring"
#define BLYNK_AUTH_TOKEN "BOt3INOpO-8bxVSAhElDYkQb6SmsjYuF"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <BlynkSimpleEsp32.h>
#include <UniversalTelegramBot.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <math.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define BOTtoken "8637819559:AAF1pAQORtI-qSg_O3NxOtnmeNa5E8Ogio8"

String chat_id = "5841512505";

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

char ssid[] = "sipppppp2";
char pass[] = "99999999";
char auth[] = BLYNK_AUTH_TOKEN;

#define TDS_PIN 34
#define PH_PIN 35
#define ONE_WIRE_BUS 15

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

float aref = 3.3;

float waterTemp = 25.0;
float voltage = 0;

float ec_us = 0;

int tds = 0;

float phValue = 0;

float calibrationFactor = 1.19;

float PH4_VOLT  = 3.00;
float PH7_VOLT  = 2.50;
float PH10_VOLT = 2.00;

float phAdjust = -5.47;

void setup() {

  Serial.begin(115200);

  analogSetAttenuation(ADC_11db);
  analogReadResolution(12);

  pinMode(TDS_PIN, INPUT);
  pinMode(PH_PIN, INPUT);

  sensors.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {

    Serial.println("OLED gagal!");

    while (1);
  }

  display.clearDisplay();
  display.setTextColor(WHITE);

  display.setTextSize(2);
  display.setCursor(20, 0);
  display.print("WQMS");

  display.setTextSize(1);
  display.setCursor(10, 30);
  display.print("Water Monitoring");

  display.display();

  client.setInsecure();

  Blynk.begin(auth, ssid, pass);

  delay(2000);

  bot.sendMessage(
    chat_id,
    "✅ Monitoring Air Aktif\nKetik /cek",
    ""
  );
}

void loop() {

  Blynk.run();

  bacaSensor();

  bacaTelegram();

  delay(1000);
}

void bacaSensor() {

  sensors.requestTemperatures();

  waterTemp = sensors.getTempCByIndex(0) - 0.7;

  if (waterTemp == -127) {

    waterTemp = 25.0;
  }

  int adcValue = 0;

  for (int i = 0; i < 100; i++) {

    adcValue += analogRead(TDS_PIN);

    delay(5);
  }

  adcValue /= 100;

  voltage = adcValue * aref / 4095.0;


  float compensationCoefficient =
    1.0 + 0.02 * (waterTemp - 25.0);

  float compensationVoltage =
    voltage / compensationCoefficient;


  float tdsValue =
    (133.42 * compensationVoltage * compensationVoltage * compensationVoltage
     - 255.86 * compensationVoltage * compensationVoltage
     + 857.39 * compensationVoltage)
    * 0.5;

  tdsValue = tdsValue * calibrationFactor;

  tds = (int)tdsValue;

  ec_us = tds * 2.002;

  int phADC = 0;

  for (int i = 0; i < 20; i++) {

    phADC += analogRead(PH_PIN);

    delay(10);
  }

  phADC /= 20;

  float phVoltage =
    phADC * 3.3 / 4095.0;

  if (phVoltage >= PH7_VOLT) {

    phValue =
      7.0 + ((phVoltage - PH7_VOLT) * (4.0 - 7.0)) / (PH4_VOLT - PH7_VOLT);

  } else {

    phValue =
      7.0 + ((phVoltage - PH7_VOLT) * (10.0 - 7.0)) / (PH10_VOLT - PH7_VOLT);

  }

  if (phValue < 0) {
    phValue = 0;
  }

  if (phValue > 14) {
    phValue = 14;
  }

  phValue = phValue + phAdjust;

  if (phValue < 0) {
    phValue = 0;
  }

  if (phValue > 14) {
    phValue = 14;
  }

  Serial.println("====================");

  Serial.print("TDS : ");
  Serial.print(tds);
  Serial.println(" ppm");

  Serial.print("EC  : ");
  Serial.print(ec_us);
  Serial.println(" uS/cm");

  Serial.print("Temp: ");
  Serial.print(waterTemp);
  Serial.println(" C");

  Serial.print("pH  : ");
  Serial.println(phValue, 2);

  display.clearDisplay();

  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print("TDS : ");
  display.print(tds);
  display.print(" ppm");

  display.setCursor(0, 18);
  display.print("EC  : ");
  display.print(ec_us, 0);
  display.print(" uS/cm");

  display.setCursor(0, 36);
  display.print("Temp: ");
  display.print(waterTemp, 1);
  display.print(" C");

  display.setCursor(0, 54);
  display.print("pH  : ");
  display.print(phValue, 2);

  display.display();

   Blynk.virtualWrite(V0, tds);

  Blynk.virtualWrite(V1, ec_us);

  Blynk.virtualWrite(V2, waterTemp);

  Blynk.virtualWrite(V3, phValue);
}

void kirimTelegram() {

  String pesan = "📊 MONITORING AIR\n\n";

  pesan += "TDS : ";
  pesan += String(tds);
  pesan += " ppm\n";

  pesan += "EC  : ";
  pesan += String(ec_us, 0);
  pesan += " uS/cm\n";

  pesan += "Temp: ";
  pesan += String(waterTemp, 1);
  pesan += " C\n";

  pesan += "pH  : ";
  pesan += String(phValue, 2);

  bot.sendMessage(chat_id, pesan, "");
}

void bacaTelegram() {

  int jumlahPesan =
    bot.getUpdates(bot.last_message_received + 1);

  while (jumlahPesan) {

    for (int i = 0; i < jumlahPesan; i++) {

      String text = bot.messages[i].text;

      text.trim();

      String from =
        bot.messages[i].chat_id;

      if (text == "/start") {

        bot.sendMessage(
          from,
          "💧 Smart Water Monitoring\n\n/cek",
          ""
        );
      }

      else if (text == "/cek") {

        kirimTelegram();
      }

      else {

        bot.sendMessage(
          from,
          "Gunakan /cek",
          ""
        );
      }
    }

    jumlahPesan =
      bot.getUpdates(bot.last_message_received + 1);
  }
}