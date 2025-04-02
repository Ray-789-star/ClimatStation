/* Программа управления станцией контроля микроклимата в помещении */

// Подключение библиотек
#include <Adafruit_GFX.h>           // Ядро графической библиотеки
#include <Adafruit_ST7735.h>        // для дисплея на ST7735
#include <Adafruit_ST7789.h>        // для дисплея на ST7789
#include <SPI.h>                    // Подключение по протоколу SPI
#include "FontsRus/TimesNRCyr10.h"  // отображение символов кириллицы
#include "DHT.h"                    // для работы с датчиком влажности и температуры dht22 ("DHT sensor library by Adafruit")
#include <MQ135.h>                  // для работы с датчиком СО2
#include <Wire.h>                   // подключения по протоколу I2C (SDA, SCL) (Стандартная) (нужна для датчика жестов)
#include "paj7620.h"                // для работы с датчиком распознавания жестов paj7620a ("Gesture PAJ7620 by Seeed Studio")
#include "color_of_led.h"           // настройка цвета светодиодов
#include "images.h"                 // изображения
#include "driver/gpio.h"            // управление включением и отключением пинов

// Режим сна и пробуждения
RTC_DATA_ATTR int bootCount = 0;           // Счетчик пробуждений
#define uS_TO_S_FACTOR 1000000             // коэффициент пересчета микросекунд в секунды 
#define TIME_TO_SLEEP 900                  // время, в течение которого будет спать ESP32 (в секундах) (15 мин)
const unsigned long workTimer = 300000;  // Время работы станции после пробуждения при отсутствии людей в комнате в миллисекундах (5 мин)

// Подключение дисплея 240 х 320 2.4 LCD TFT
#define TFT_CS 14
#define TFT_RST 15
#define TFT_DC 32
#define TFT_MOSI 23  
#define TFT_SCLK 13  
#define TFT_LED 16   // Управление подсветкой экрана
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

#define POWER 4     //   Включение питания датчика DHT-22, дисплея - 5v
#define POWER_2 12  //   Включение питания датчиков газа, пыли, движения  - 5v

// Подключение датчика температуры и влажности dht22
// VCC   3.3v
// GND   GND
#define DHTPIN 25  // DAT - GPIO 26

#define DHTTYPE DHT22      // Назначение типа используемого датчика (DHT 22  (AM2302), AM2321)
DHT dht(DHTPIN, DHTTYPE);  // Инициализация датчика dht22

// Подключение датчика пыли DSW501A
// Оранжевый (GND) - GND
#define pinPM25 34  // Желтый (Vout1 (pm2.5)) \
                    // Белый (VCC) - 5v
#define pinPM1 35   // Красный (Vout2 (pm10)) \
                    // Черный (Control) для настройки pm10 - не используется

// Подключение датчика качества воздуха MQ-135
#define analog_CO2 33  //A0 для считывания уровня CO2
#define digit_CO2 18   //D0 для индикации достижения порогового значения СО2 (LOW)-настраиваемое на сенсоре
//GND - GND, VCC - 5v
MQ135 gasSensor = MQ135(analog_CO2);  // Инициализация объекта датчика

// Подключение датчика движения HC505 ('+' - 5v, '-' - GND)
#define pinPIR 17  // OUT (средний)

// Подключение сенсорной кнопки для пробуждения по касанию
// VCC - 3.3v, GND
#define LedTouch GPIO_NUM_27  // Data

// Подключение датчика жестов VCC - 3.3v, GND - GND, SCL - GPIO 22, SDA GPIO 21, INT Не используется
bool gestureFlag = true;   // Флаг активации датчика жестов
bool gesDownFlag = false;  // Флаг вызова общего экрана по жесту "Вниз"

#define VoltPin 0         // Пин для считывания напряжения аккумулятора
uint8_t analogVolts = 0;  // Заряд аккумулятора
uint8_t maxVolts = 0;     // Максимальное значение заряда аккумулятора из группы замеров

unsigned long timing;            // Переменная хранения точки отсчета для функции millis()
uint8_t count = 0;               // Счетчик неблагоприятных параметров (для работы подсветки)
bool startFlag = false;          // Флаг для запуска заставки
bool valFlag = true;             // Флаг для отображения переменных на дисплее
float t, h, ppm, dust25, dust1;  // Хранение показаний датчиков (температура, влажность, СО2, пыль 2.5 и 1)

const unsigned long sampleTime = 30000;  // интервал для работы датчика пыли (5-30сек) (Считывать показания не ранее этого промежутка)
float a = 0.0, b = 12500.0;              // Граничные показатели для датчика пыли (min, max)

// Пороговые показатели микроклимата в помещении
// температура
#define T_MIN 20  // Нижний порог температуры 20
#define T_MAX 25  // Верхний порог температуры 25
// влажность
#define H_MIN 40  // Нижний порог влажности
#define H_MAX 60  // Верхний порог влажности
// уровень CO2 (ppm)
//#define CO2_COMF 800       // максимально комфортный для помещений
#define CO2_ACCEPT 700  // максимально допустимый 700
//#define CO2_HARMFUL 1400   // вредный для здоровья и самочувствия (используется для срабатывания цифрового пина)
//#define CO2_LETHAL 100000  // game over

#define DUST_MAX_1 0.30   // Верхний порог содержания частиц pm10   0.30
#define DUST_MAX_25 0.16  // Верхний порог содержания частиц pm2.5   0.16

int display[4] = { 0 };  // массив для переключения страниц дисплея

void setup() {

  // Инициализация монитора порта (для отладки)
  Serial.begin(115200);
  // Режим сна
  ++bootCount;  // Увеличить счетчик количества пробуждений
  // Отправить сообщение о пробуждении в монитор порта
  Serial.println("Boot number: " + String(bootCount));
  // Назначить инициатором пробуждения RTC-таймер
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  // Назначить инициатором пробуждения нажатие на кнопку
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_27, 1);

  delay(10000);  // Пауза перед пуском esp32
  //Инициализация пинов светодиодов как выход
  pinMode(R, OUTPUT);
  pinMode(G, OUTPUT);
  pinMode(B, OUTPUT);
  // Выключение светодиодов
  color_of_led(7);
  // Инициализация пина контроля заряда
  pinMode(VoltPin, INPUT);
  gpio_hold_dis(GPIO_NUM_12);
  gpio_hold_dis(GPIO_NUM_4);
  pinMode(POWER_2, OUTPUT);
  digitalWrite(POWER_2, HIGH);

  // Инициализация датчика движения
  pinMode(pinPIR, INPUT);

  // Если при пробуждении по таймеру зафиксировано движение в помещении
  if (digitalRead(pinPIR) == HIGH) {
    Serial.println("MOTION!");
    start();  // Вызов функции загрузки
  } else {
    gestureFlag = false;
    delay(10000);
    digitalWrite(POWER_2, LOW);
    digitalWrite(POWER, LOW);
    delay(1000);
    gpio_hold_en(GPIO_NUM_12);
    gpio_hold_en(GPIO_NUM_4);
    esp_deep_sleep_start();
  }
  // Инициализация работы датчика температуры и влажности
  dht.begin();
  // Инициализация цифрового пина датчика СО2 как ввода для приема значений
  pinMode(digit_CO2, INPUT);
  // Инициализация пинов датчика пыли как ввода для приема значений
  pinMode(pinPM25, INPUT);
  pinMode(pinPM1, INPUT);
  pinMode(LedTouch, INPUT);
  analogReadResolution(12);
}

void loop() {

  // Демонстрация заряда батареи на экране
  powerBattery();
  // Вызов функции опроса датчика температуры и влажности
  temp_humidity();
  // Вызов функции опроса датчика запыленности
  dustLevel();
  // Вызов функции опроса датчика СО2
  carbonDioxLevel();
  // Включение цвета светодиодов в зависимости значений параметров
  color_of_led(count);

  // Если активировался экран, то вызвать опрос датчика жестов
  if (gestureFlag) {
    gesture();  // Вызов функции опроса датчика жестов
    // Передача значений параметров на дисплей с интервалом 10 секунд
    if (millis() + timing >= 10000 && valFlag) {
      timing = millis();
      printValSecondDisplay();
    }
    // Обнуление счетчика неблагоприятных параметров для работы подсветки
    count = 0;
    // Если нет показаний от датчика движения и система работает больше заданного отрезка перейти в режим сна
    if (digitalRead(pinPIR) == LOW) {
      if (millis() + timing > workTimer) {
        Serial.println("sleep");
        fall_asleep();
        // Если есть движение, но экран не активировался то вызвать функцию активации экрана
      }
    } else if (digitalRead(pinPIR) == HIGH && startFlag == false) {
      Serial.println("MOTION!");
      // Вызов функции опроса датчика жестов
      start();
    }
  }
}

//Функция работы датчика влажности и температуры
void temp_humidity() {

  t = dht.readTemperature();  // Считывание температуры в градусах Цельсия
  h = dht.readHumidity();     // Считывание начение влажности
  // Если значение температуры в норме увеличить счетчик, иначе присвоить 1 первому элементу массива
  if (t > T_MIN && t < T_MAX) count++;
  else display[0] = 1;
  // Если значение влажности в норме увеличить счетчик, иначе присвоить 2 второму элементу массива
  if (h > H_MIN && h < H_MAX) count++;
  else display[1] = 2;
}

// Функции для работы датчика пыли
float calc_low_ratio(float lowPulse) {
  return lowPulse / (sampleTime * 10.0);
}

float calc_c_mgm3(float lowPulse) {
  float r = calc_low_ratio(lowPulse);
  float c_mgm3 = 1.1 * pow(r, 3) - 3.8 * pow(r, 2) + 520 * r;
  float mgm3 = max(a, c_mgm3);
  return mgm3;
}

void dustLevel() {
  static unsigned long t_start = millis();
  static float lowPM25, lowPM1 = 0;
  if (millis() + t_start >= sampleTime) {
    lowPM25 += pulseIn(pinPM25, HIGH) / 1000.0;
    lowPM1 += pulseIn(pinPM1, HIGH) / 1000.0;

    dust25 = calc_c_mgm3(lowPM25);
    dust1 = calc_c_mgm3(lowPM1);
    // Если значение запыленности в норме увеличить счетчик, иначе присвоить 2 второму элементу массива
    if (dust25 < DUST_MAX_25 && dust1 < DUST_MAX_1) count++;
    else display[2] = 3;
  }
}
// Функция для работы датчика пыли
void carbonDioxLevel() {
  if (millis() + timing >= 500) {
    timing = millis();
    ppm = gasSensor.getPPM();  // чтение данных концентрации CO2
    // Если значение уровня СО2 в норме увеличить счетчик, иначе присвоить 2 второму элементу массива
    if (ppm < CO2_ACCEPT) count++;
    else display[3] = 4;
  }
}

// Функция запуска станции при пробуждении по касанию RGB Touch
void start() {

  unsigned long timing;
  bool b = true;
  uint8_t a = 0;
  startFlag = true;
  // Инициализация дисплея
  tft.init(240, 320);

  //Инициализация пинов светодиодов как выход
  pinMode(R, OUTPUT);
  pinMode(G, OUTPUT);
  pinMode(B, OUTPUT);

  // Включение синей подсветки светодиодов
  color_of_led(5);

  // Включение подсветки дисплея
  pinMode(POWER, OUTPUT);
  digitalWrite(POWER, HIGH);

  // Заливка дисплея черным
  tft.fillScreen(ST77XX_WHITE);
  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, HIGH);

  // Вывод на дисплей страницы загрузки
  hellowDisplay();

  // Инициализация датчика жестов
  uint8_t error = 0;      // Переменная для обработки ошибок инициализации датчика жестов
  error = paj7620Init();  // Инициализировать регистры Paj7620
  if (error) {
    Serial.print("INIT ERROR,CODE:");
    Serial.println(error);
  } else {
    Serial.println("INIT OK");
    gestureFlag = true;
  }
  Serial.println("Please input your gestures:\n");

  // Мигание дисплеем
  while (a < 7) {
    if (millis() - timing > 500) {
      tft.invertDisplay(b);
      b = !b;
      timing = millis();
      a++;
    }
  }
  handSideDisplay();
  handDownDisplay();
  handUpDisplay();
  secondPageDisplay();
}

// Функция: Стартовая заставка
void hellowDisplay() {
  unsigned long timing;
  uint8_t a = 0;
  testlines(ST77XX_YELLOW);
  tft.setFont(&TimesNRCyr10pt8b);
  tft.setCursor(31, 155);
  tft.setTextColor(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.println("ПРИВЕТ!");
  a = 0;
  while (a < 5) {
    if (millis() - timing > 2000) {
      a++;
      timing = millis();
    }
  }
}
// Заставка перед включением спящего режима
void fall_asleep() {

  tft.fillScreen(ST77XX_WHITE);
  testlines(ST77XX_YELLOW);
  delay(500);
  tft.setFont(&TimesNRCyr10pt8b);
  tft.setCursor(55, 155);
  tft.setTextColor(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.println("ПОКА!");
  delay(2000);

  testlines(ST77XX_WHITE);
  tft.setCursor(55, 155);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.println("ПОКА!");
  digitalWrite(TFT_LED, LOW);
  digitalWrite(POWER_2, LOW);
  digitalWrite(POWER, LOW);
  delay(1000);
  gpio_hold_en(GPIO_NUM_12);
  gpio_hold_en(GPIO_NUM_4);
  esp_deep_sleep_start();
}

// Отображение жеста влево-вправо на дисплее
void handSideDisplay() {

  unsigned long timing;
  bool b = true;
  uint8_t a = 0;

  tft.fillScreen(ST77XX_YELLOW);

  tft.setFont(&TimesNRCyr10pt8b);
  tft.setCursor(70, 70);
  tft.setTextColor(ST77XX_BLACK);
  tft.setTextSize(1);
  tft.println("ЛИСТАТЬ!");
  tft.drawBitmap(56, 180, *epd_bitmap_handV128, 128, 128, ST77XX_BLACK);
  tft.cp437(true);
  tft.setFont();

  while (a < 10) {
    if (millis() - timing > 500) {
      tft.setTextSize(10);
      if (a % 2) {
        tft.setTextColor(ST77XX_GREEN);
        tft.setCursor(40, 80);
        tft.write(0x1b);
        tft.setTextColor(ST77XX_BLACK);
        tft.setCursor(140, 80);
        tft.write(0x1a);
      } else {
        tft.setTextColor(ST77XX_BLACK);
        tft.setCursor(40, 80);
        tft.write(0x1b);
        tft.setTextColor(ST77XX_GREEN);
        tft.setCursor(140, 80);
        tft.write(0x1a);
      }
      timing = millis();
      a++;
    }
  }
}
// Отображение жеста вниз на дисплее
void handDownDisplay() {

  unsigned long timing;
  bool b = true;
  uint8_t a = 0;
  tft.fillScreen(ST77XX_YELLOW);

  tft.setFont(&TimesNRCyr10pt8b);
  tft.setCursor(60, 70);
  tft.setTextColor(ST77XX_BLACK);
  tft.setTextSize(1);
  tft.println("ВЕРНУТЬСЯ!");

  tft.drawBitmap(60, 180, *epd_bitmap_hand128, 128, 128, ST77XX_BLACK);
  tft.cp437(true);
  tft.setFont();

  while (a < 10) {
    if (millis() - timing > 500) {
      tft.setCursor(102, 80);
      tft.setTextSize(10);
      if (a % 2)
        tft.setTextColor(ST77XX_BLACK);
      else
        tft.setTextColor(ST77XX_GREEN);
      tft.write(0x19);
      timing = millis();
      a++;
    }
  }
}

// Отображение жеста вверх на дисплее
void handUpDisplay() {

  unsigned long timing;
  bool b = true;
  uint8_t a = 0;

  tft.fillScreen(ST77XX_YELLOW);
  tft.setFont(&TimesNRCyr10pt8b);
  tft.setCursor(90, 70);
  tft.setTextColor(ST77XX_BLACK);
  tft.setTextSize(1);
  tft.println("СПАТЬ!");

  tft.drawBitmap(56, 180, *epd_bitmap_hand128, 128, 128, ST77XX_BLACK);
  tft.cp437(true);
  tft.setFont();

  while (a < 10) {
    if (millis() - timing > 500) {
      tft.setCursor(102, 80);
      tft.setTextSize(10);
      if (a % 2)
        tft.setTextColor(ST77XX_BLACK);
      else
        tft.setTextColor(ST77XX_GREEN);
      tft.write(0x18);
      timing = millis();
      a++;
    }
  }
}

// Функция отображения всех параметров на дисплее
void secondPageDisplay() {

  valFlag = true;

  tft.fillScreen(ST77XX_WHITE);

  tft.setTextColor(ST77XX_RED);
  tft.setTextSize(1);
  tft.setFont(&TimesNRCyr10pt8b);  // выбор шрифта

  tft.setCursor(45, 30);
  tft.println("ТЕМПЕРАТУРА");
  tft.fillRoundRect(1, 48, 240, 30, 8, ST77XX_GREEN);

  tft.setCursor(55, 110);
  tft.println("ВЛАЖНОСТЬ");
  tft.fillRoundRect(1, 123, 240, 30, 8, ST77XX_GREEN);

  tft.setCursor(35, 185);
  tft.println("ЗАПЫЛЕННОСТЬ");
  tft.fillRoundRect(1, 198, 240, 30, 8, ST77XX_GREEN);
  tft.setCursor(10, 220);
  tft.println("pm2.5:");
  tft.setCursor(125, 220);
  tft.println("pm10:");

  tft.setCursor(30, 260);
  tft.println("CARBON DIOXIDE");
  tft.fillRoundRect(1, 273, 240, 30, 8, ST77XX_GREEN);
}

// Функция отображения значений всех параметров на дисплее

void printValSecondDisplay() {
  static float t1, h1, ppm1, dust1_1, dust25_1;
  tft.setFont(&TimesNRCyr10pt8b);
  tft.setTextColor(ST77XX_BLUE);

  if (t != t1 || !gesDownFlag) {
    tft.fillRoundRect(1, 48, 240, 30, 8, ST77XX_GREEN);
    tft.setCursor(100, 70);
    tft.print(t, 2);
    Serial.println(t);
  }
  t1 = t;

  if (h != h1 || !gesDownFlag) {
    tft.fillRoundRect(1, 123, 240, 30, 8, ST77XX_GREEN);
    tft.setCursor(100, 145);
    tft.print(h, 2);
    Serial.println(h);
  }
  h1 = h;

  static bool flag;

  tft.fillRoundRect(1, 198, 240, 30, 8, ST77XX_GREEN);
  tft.setFont(&TimesNRCyr10pt8b);
  tft.setTextColor(ST77XX_RED);
  if (flag) {
    tft.setCursor(55, 220);
    tft.println("pm2.5:");
    tft.setCursor(120, 220);
    tft.setTextColor(ST77XX_BLUE);
    tft.print(dust25, 5);
    Serial.println(dust25);
  } else {
    tft.setCursor(55, 220);
    tft.println("pm10:");
    tft.setCursor(120, 220);
    tft.setTextColor(ST77XX_BLUE);
    tft.print(dust1, 5);
  }
  flag = !flag;

  if (ppm != ppm1 || !gesDownFlag) {
    tft.fillRoundRect(1, 273, 240, 30, 8, ST77XX_GREEN);
    tft.setTextColor(ST77XX_BLUE);
    tft.setCursor(90, 295);
    tft.print(ppm, 2);
  }
  ppm1 = ppm;
  gesDownFlag = true;
}

// Функция выбора страниц дисплея в зависимости от жеста
void displaySelection(int x) {
  switch (x) {
    case 1:
      tempPageDisplay();
      break;
    case 2:
      humiPageDisplay();
      break;
    case 3:
      dustPageDisplay();
      break;
    case 4:
      CarbOxPageDisplay();
      break;
  }
}

// Функция отображения экрана температуры
void tempPageDisplay() {
  valFlag = false;
  tft.fillScreen(ST77XX_WHITE);
  tft.setFont();
  tft.setTextColor(0xF800);
  tft.drawBitmap(5, 5, *epd_bitmap_temperature, 100, 104, 0xF800);
  tft.setCursor(125, 60);
  tft.setTextSize(4);
  tft.print(t, 1);
  Serial.println(t);

  tft.setFont(&TimesNRCyr10pt8b);
  tft.setTextSize(1);
  tft.setTextWrap(true);

  if (t < T_MIN) {
    tft.setCursor(55, 150);
    tft.println("ПРОХЛАДНО!\n   * Закройте окно или\n    включите обогреватель\n");
    tft.println("   * Укройтесь пледом и\n    выпейте чашечку\n    горячего ароматного\n    какао)))");
  }
  if (t > T_MAX) {

    tft.setCursor(10, 150);
    tft.println(" СТАНОВИТСЯ ЖАРКО\n\n  *Откройте окно для\n проветривания или\n включите вентилятор\n");
    tft.println("  *Не забывайте пить\n больше воды");
  }
}

// Функция отображения экрана влажности (вызывается жестом "в сторону")
void humiPageDisplay() {
  valFlag = false;
  tft.fillScreen(ST77XX_WHITE);
  tft.setFont();
  tft.setTextColor(0xF800);
  tft.drawBitmap(5, 5, *epd_bitmap_humidity, 100, 111, 0xF800);
  tft.setCursor(125, 60);
  tft.setTextSize(4);
  tft.print(h, 1);
  Serial.println(h);

  tft.setFont(&TimesNRCyr10pt8b);
  tft.setTextSize(1);
  tft.setTextWrap(true);

  if (h < H_MIN) {
    tft.setCursor(5, 150);
    tft.println("ДОБРО ПОЖАЛОВАТЬ\n             В САХАРУ )))\n");
    tft.println("* Включите увлажнитель воздуха, ЕСЛИ есть)))\n\n* Поставьте в комнате\n емкость с водой");
  }
  if (h > H_MAX) {
    tft.setCursor(50, 150);
    tft.println("    ВЫСОКАЯ\n            ВЛАЖНОСТЬ");
    tft.println("* Откройте окно для\n проветривания\nи прибавьте отопление\n");
    tft.println("* Уберите из комнаты\n емкость с водой");
  }
}

// Функция отображения экрана запыленности
void dustPageDisplay() {
  valFlag = false;
  tft.fillScreen(ST77XX_WHITE);
  tft.setFont();
  tft.setTextColor(0xF800);
  tft.drawBitmap(5, 5, *epd_bitmap_dust, 100, 100, 0xF800);
  if (dust25 >= DUST_MAX_25 && dust1 <= DUST_MAX_1) {
    tft.setCursor(125, 30);
    tft.setTextSize(3);
    tft.print("pm25:");
    tft.setCursor(125, 60);
    tft.setTextSize(3);
    tft.print(dust25, 3);
  } else if (dust25 <= DUST_MAX_25 && dust1 >= DUST_MAX_1) {
    tft.setCursor(125, 30);
    tft.setTextSize(3);
    tft.print("pm10:");
    tft.setCursor(125, 60);
    tft.setTextSize(3);
    tft.print(dust1, 3);
  } else if (dust25 >= DUST_MAX_25 && dust1 >= DUST_MAX_1) {
    bool a = 0;
    if (millis() + timing >= 10000) {
      a = !a;
      if (a) {
        tft.setCursor(125, 30);
        tft.setTextSize(3);
        tft.print("pm10:");
        tft.setCursor(125, 60);
        tft.setTextSize(3);
        tft.print(dust1, 3);
      } else {
        tft.setCursor(125, 30);
        tft.setTextSize(3);
        tft.print("pm25:");
        tft.setCursor(125, 60);
        tft.setTextSize(3);
        tft.print(dust25, 3);
        timing = millis();
      }
    }
  }

  tft.setFont(&TimesNRCyr10pt8b);
  tft.setTextSize(1);
  tft.setTextWrap(true);

  tft.setCursor(10, 150);
  tft.println("   ОТКУДА СТОЛЬКО\n               ПЫЛИ!?\n");
  tft.println("* Прогуляйтесь, пусть\n пыль осядет, а затем\n - ВЛАЖНАЯ уборка");
}

// Функция отображения экрана свежести
void CarbOxPageDisplay() {
  valFlag = false;
  tft.fillScreen(ST77XX_WHITE);
  tft.setFont();
  tft.setTextColor(0xF800);
  tft.drawBitmap(5, 5, *epd_bitmap_carbon, 100, 101, 0xF800);
  tft.setCursor(125, 60);
  tft.setTextSize(3);
  tft.print(ppm, 1);
  Serial.println(ppm);

  tft.setFont(&TimesNRCyr10pt8b);
  tft.setTextSize(1);
  tft.setTextWrap(true);

  if (ppm > CO2_ACCEPT) {
    tft.setCursor(10, 150);
    tft.println("     ГДЕ КИСЛОРОД?\n     МЫ НА ВЕНЕРЕ?)))\n");
    tft.println("* Откройте окно для\n проветривания\n\n* Погуляйте на свежем\n воздухе\n");
  }
}

// Функция работы датчика жестов
void gesture() {
  static int counter;

  //Опрос датчика жестов
  unsigned long timing;
  uint8_t data = 0, data1 = 0, error;

  error = paj7620ReadReg(0x43, 1, &data);  // Прочитать Bank_0_Reg_0x43/0x44 для получения результата жеста.
  if (!error) {
    switch (data)  // При обнаружении различных жестов переменной «data» будут присвоены различные значения с помощью paj7620ReadReg(0x43, 1, &data).
    {
      case GES_RIGHT_FLAG:
        Serial.println("Right");
        if (count != 4) {
          do {
            gesDownFlag = false;
            counter++;
            Serial.println(counter);
            if (counter > 3) counter = 0;
            if (display[counter])
              displaySelection(display[counter]);
          } while (display[counter] == 0);
        }
        break;

      case GES_LEFT_FLAG:
        Serial.println("Left");
        if (count != 4) {
          do {
            gesDownFlag = false;
            counter--;
            Serial.println(counter);
            if (counter < 0) counter = 3;
            if (display[counter])
              displaySelection(display[counter]);
          } while (display[counter] == 0);
        }
        break;

      case GES_UP_FLAG:
        Serial.println("Up");
        gesDownFlag = false;
        Serial.println("sleep");
        fall_asleep();
        break;

      case GES_DOWN_FLAG:
        if (gesDownFlag == false) {
          Serial.println("Down");
          gesDownFlag = true;
          secondPageDisplay();
        } else
          Serial.println("Down - second recall");
        break;

      default:
        break;
    }
  }
  if (millis() - timing >= 50) {
    timing = millis();
  }
}

// Рисует линии из углов при старте и засыпании
void testlines(uint16_t color) {
  for (int16_t x = 0; x < tft.width(); x += 5) {
    tft.drawLine(0, tft.height() - 1, x, 0, color);
    delay(0);
  }
  for (int16_t x = 0; x < tft.width(); x += 5) {
    tft.drawLine(tft.width() - 1, 0, x, tft.height() - 1, color);
    delay(0);
  }
  for (int16_t x = tft.width() - 1; x > 0; x -= 5) {
    tft.drawLine(tft.width() - 1, tft.height() - 1, x, 0, color);
    delay(0);
  }
  for (int16_t x = tft.width() - 1; x > 0; x -= 5) {
    tft.drawLine(0, 0, x, tft.height() - 1, color);
    delay(0);
  }
}
// функция расчета и отображения на дисплее заряда батареи
void powerBattery() {
  uint8_t a = 0;

  if (millis() + timing > 5000) {
    analogVolts = analogReadMilliVolts(VoltPin) / 37;

    analogVolts = analogVolts + (analogVolts / 3);
    if (analogVolts >= 100)
      analogVolts = 100;
    if (maxVolts < analogVolts)
      maxVolts = analogVolts;
  }

  if (maxVolts >= 90)
    a = 3;
  else if (maxVolts < 90 && maxVolts >= 80)
    a = 2;
  else if (maxVolts < 80 && maxVolts >= 70)
    a = 1;
  else {

    uint8_t b = 0;

    while (b < 16) {
      if (millis() - timing > 500) {
        if (b % 2)
          color_of_led(6);
        else
          color_of_led(7);
        b++;
        timing = millis();
      }
    }
  }
  for (uint8_t i = 0; i <= a; i++) {
    tft.drawBitmap(215, 5, battery_icons[i], 24, 16, 1);
  }
}