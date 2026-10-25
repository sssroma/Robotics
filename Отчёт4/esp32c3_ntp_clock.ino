#include <WiFi.h>
#include <time.h>
#include <U8g2lib.h>
#include "secrets.h"   // WIFI_SSID и WIFI_PASS

const char* TZ_INFO = "<+07>-7";            // Новосибирск, UTC+7 (без перехода на летнее время)
const char* NTP_1   = "ru.pool.ntp.org";
const char* NTP_2   = "pool.ntp.org";

const int SDA_PIN = 8;
const int SCL_PIN = 9;

const int NIGHT_START = 23;                 // с этого часа экран тусклый
const int NIGHT_END   = 7;                  // до этого часа
const int CONTRAST_DAY   = 180;             // 0..255
const int CONTRAST_NIGHT = 1;

const char* DAYS[] = {"Вс", "Пн", "Вт", "Ср", "Чт", "Пт", "Сб"};
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, SCL_PIN, SDA_PIN);

void showMessage(const char* line1, const char* line2 = "") {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_t_cyrillic);
  u8g2.drawUTF8(0, 28, line1);
  u8g2.drawUTF8(0, 44, line2);
  u8g2.sendBuffer();
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);       // лечит нестабильный Wi-Fi на Super Mini
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  showMessage("Подключаюсь к Wi-Fi", WIFI_SSID);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(250);
  }
}

void drawClock(const tm& t) {
  char hh[3], mm[3], date[24];
  snprintf(hh, sizeof(hh), "%02d", t.tm_hour);
  snprintf(mm, sizeof(mm), "%02d", t.tm_min);
  snprintf(date, sizeof(date), "%s, %02d.%02d.%04d",
           DAYS[t.tm_wday], t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);

  bool night = (t.tm_hour >= NIGHT_START || t.tm_hour < NIGHT_END);
  u8g2.setContrast(night ? CONTRAST_NIGHT : CONTRAST_DAY);

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x12_t_cyrillic);
  u8g2.drawUTF8((128 - u8g2.getUTF8Width(date)) / 2, 11, date);

  // Крупное время: двоеточие всегда на одном месте
  u8g2.setFont(u8g2_font_logisoso32_tn);
  u8g2.drawStr(64 - 8 - u8g2.getStrWidth(hh), 54, hh);
  u8g2.drawStr(64 + 8, 54, mm);
  if (t.tm_sec % 2 == 0) {                  // мигающее двоеточие
    u8g2.drawBox(62, 31, 4, 4);
    u8g2.drawBox(62, 43, 4, 4);
  }

  // Полоска секунд внизу
  u8g2.drawBox(0, 60, (t.tm_sec + 1) * 128 / 60, 3);

  u8g2.sendBuffer();
}


void setup() {
  Serial.begin(115200);
  u8g2.begin();
  u8g2.setBusClock(400000);

  connectWiFi();
  if (WiFi.status() != WL_CONNECTED) {
    showMessage("Нет Wi-Fi", "Проверь имя/пароль");
    delay(3000);
  }

  configTzTime(TZ_INFO, NTP_1, NTP_2);      // запуск синхронизации времени
  showMessage("Синхронизация", "времени (NTP)...");
}

void loop() {
  static int lastSec = -1;
  static uint32_t lastReconnect = 0;

  // Если Wi-Fi пропал, пробуем переподключиться раз в 30 секунд
  if (WiFi.status() != WL_CONNECTED && millis() - lastReconnect > 30000) {
    lastReconnect = millis();
    WiFi.reconnect();
  }

  struct tm t;
  if (!getLocalTime(&t, 0)) {               // время ещё не получено
    showMessage("Жду время из сети", "NTP...");
    delay(500);
    return;
  }

  if (t.tm_sec == lastSec) {                // перерисовываем раз в секунду
    delay(20);
    return;
  }
  lastSec = t.tm_sec;
  drawClock(t);
}
