/*****************************************************************
 * SMART FISH FEEDER BERBASIS INTERNET OF THINGS (IoT)
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
 * Deskripsi :
 * Program ini digunakan untuk mengontrol sistem pemberian
 * pakan ikan otomatis menggunakan ESP32-C3, RTC DS3231,
 * sensor ultrasonik HC-SR04, motor servo, buzzer,
 * Telegram Bot, dan Blynk IoT.
 *
 * Fitur :
 * - Jadwal pemberian pakan otomatis
 * - Kontrol pakan manual melalui Telegram
 * - Kontrol pakan manual melalui Blynk
 * - Monitoring level pakan
 * - Notifikasi pakan rendah (<30%)
 * - Tampilan OLED Display
 *
 * Tahun : 2026
 *
 *****************************************************************/

#define BLYNK_TEMPLATE_ID "TMPL6By0k17lB"
#define BLYNK_TEMPLATE_NAME "Smart Fish Feeder"
#define BLYNK_AUTH_TOKEN "uWrESKCBnRGkIVVNa2tbLgiRz-KZpjSU"

#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>
#include <ESP32Servo.h>
#include <EEPROM.h>

#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

#define BOT_TOKEN "8613710547:AAHkoyNteMrCFo3Fb_SxaPyZj4shsigh1kg"
String CHAT_ID = "5841512505";

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

char ssid[] = "sipppppp2";
char pass[] = "99999999";

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

#define SDA_PIN 8
#define SCL_PIN 9
#define TRIGPIN 1
#define ECHOPIN 0
#define BUZZER 10
#define SERVO_PIN 5

#define SERVO_STOP 92
#define SERVO_RUN 120

#define EEPROM_SIZE 64

#define BAR_X 40
#define BAR_Y 40
#define BAR_WIDTH 45
#define BAR_HEIGHT 10

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
RTC_DS3231 rtc;
Servo servo360;

unsigned int jarak, timer, level;

unsigned int SP = 7;

bool buzzerState;

unsigned long lastSensor = 0;
unsigned long lastFeed = 0;
unsigned long lastNotif = 0;

uint8_t Jam1=7, Menit1=0, Porsi1=2, En1=1;
uint8_t Jam2=13, Menit2=20, Porsi2=1, En2=1;
uint8_t Jam3=19, Menit3=0, Porsi3=3, En3=1;

BLYNK_WRITE(V0){ Jam1 = param.asInt(); }
BLYNK_WRITE(V1){ Menit1 = param.asInt(); }
BLYNK_WRITE(V2){ Porsi1 = param.asInt(); }
BLYNK_WRITE(V3){ En1 = param.asInt(); }

BLYNK_WRITE(V4){ Jam2 = param.asInt(); }
BLYNK_WRITE(V5){ Menit2 = param.asInt(); }
BLYNK_WRITE(V6){ Porsi2 = param.asInt(); }
BLYNK_WRITE(V7){ En2 = param.asInt(); }

BLYNK_WRITE(V8){ Jam3 = param.asInt(); }
BLYNK_WRITE(V9){ Menit3 = param.asInt(); }
BLYNK_WRITE(V10){ Porsi3 = param.asInt(); }
BLYNK_WRITE(V11){ En3 = param.asInt(); }

BLYNK_WRITE(V13){
  if(param.asInt() == 1){
    feed(2);
  }
}

void handleTelegram() {

  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

  while (numNewMessages) {

    for (int i = 0; i < numNewMessages; i++) {

      String chat_id = bot.messages[i].chat_id;
      String text = bot.messages[i].text;

      if (chat_id != CHAT_ID) {
        bot.sendMessage(chat_id, "Akses ditolak!", "");
        continue;
      }

      if (text == "/start" || text == "/mulai") {

        String msg = "🤖 Smart Fish Feeder\n\n";
        msg += "/status\n";
        msg += "/feed\n";
        msg += "/level\n";

        bot.sendMessage(chat_id, msg, "");
      }

      else if (text == "/status") {

        String msg = "📊 STATUS FEEDER\n";
        msg += "Jadwal 1: " + String(Jam1) + ":" + String(Menit1) + "\n";
        msg += "Jadwal 2: " + String(Jam2) + ":" + String(Menit2) + "\n";
        msg += "Jadwal 3: " + String(Jam3) + ":" + String(Menit3) + "\n";

        bot.sendMessage(chat_id, msg, "");
      }

      else if (text == "/feed") {

        bot.sendMessage(chat_id, "🍽️ Feeding...", "");

        delay(500);

        feed(2);

        bot.sendMessage(chat_id, "✅ Selesai", "");
      }

      else if (text == "/level") {

        bot.sendMessage(chat_id,
                        "📦 Level pakan: " + String(level) + "%",
                        "");
      }

      else {

        bot.sendMessage(chat_id,
                        "❌ Perintah tidak dikenal",
                        "");
      }
    }

    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }
}

void setup() {

  Serial.begin(115200);

  EEPROM.begin(EEPROM_SIZE);

  Wire.begin(SDA_PIN, SCL_PIN);

  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  display.clearDisplay();
  display.setTextColor(WHITE);

  pinMode(TRIGPIN, OUTPUT);
  pinMode(ECHOPIN, INPUT);
  pinMode(BUZZER, OUTPUT);

  rtc.begin();

  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  servo360.attach(SERVO_PIN);
  servo360.write(SERVO_STOP);

  display.setCursor(0,0);
  display.setTextSize(1);
  display.println("SMART FISH FEEDER");
  display.println("Connecting WiFi...");
  display.display();

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  client.setInsecure();

  bot.sendMessage(CHAT_ID,
                  "✅ Smart Fish Feeder Aktif!\nKetik /start",
                  "");
}

void loop() {

  Blynk.run();

  handleTelegram();

  DateTime now = rtc.now();

  if(millis() - lastSensor > 1000){

    lastSensor = millis();

    digitalWrite(TRIGPIN, LOW);
    delayMicroseconds(2);

    digitalWrite(TRIGPIN, HIGH);
    delayMicroseconds(10);

    digitalWrite(TRIGPIN, LOW);

    timer = pulseIn(ECHOPIN, HIGH, 30000);

    jarak = timer / 58;

     if(jarak > 7) jarak = 7;

    level = map(jarak, 7, 0, 0, 100);

    if(level < 0) level = 0;
    if(level > 100) level = 100;

      if(level < 30){

      buzzerState = !buzzerState;

      digitalWrite(BUZZER, buzzerState);

    } else {

      digitalWrite(BUZZER, LOW);
    }

    display.clearDisplay();

    display.setTextSize(1);
    display.setCursor(12,0);
    display.println("SMART FISH FEEDER");

    display.setTextSize(2);
    display.setCursor(12,15);

    display.printf("%02d:%02d:%02d",
                   now.hour(),
                   now.minute(),
                   now.second());

    drawBar(level);

    Blynk.virtualWrite(V12, level);

    if(level < 30 && millis() - lastNotif > 60000){

      bot.sendMessage(CHAT_ID,
                      "⚠️ Pakan Hampir Habis!",
                      "");

      lastNotif = millis();
    }
  }

  if(now.dayOfTheWeek() != 0){

    if(millis() - lastFeed > 60000){

      if(En1 &&
         now.hour()==Jam1 &&
         now.minute()==Menit1){

        feed(Porsi1);

        lastFeed = millis();
      }

      if(En2 &&
         now.hour()==Jam2 &&
         now.minute()==Menit2){

        feed(Porsi2);

        lastFeed = millis();
      }

      if(En3 &&
         now.hour()==Jam3 &&
         now.minute()==Menit3){

        feed(Porsi3);

        lastFeed = millis();
      }
    }
  }
}

void feed(int porsi){

  servo360.write(SERVO_RUN);

  delay(1500 * porsi);

  servo360.write(SERVO_STOP);
}

void drawBar(int percent){

  display.setTextSize(1);

  display.setCursor(0, BAR_Y+1);
  display.print("Pakan");

  display.drawRect(BAR_X,
                   BAR_Y,
                   BAR_WIDTH,
                   BAR_HEIGHT,
                   WHITE);

  int fill = map(percent,
                 0,
                 100,
                 0,
                 BAR_WIDTH-2);

  display.fillRect(BAR_X+1,
                   BAR_Y+1,
                   fill,
                   BAR_HEIGHT-2,
                   WHITE);

  display.setCursor(BAR_X + BAR_WIDTH + 5,
                    BAR_Y + 1);

  display.print(percent);
  display.print("%");

  display.display();
}