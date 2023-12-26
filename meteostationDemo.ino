/*
******************************************************************************
  Описание:
  Скетч для прошивки метеостанции с использованием Web страницы GP с доступом
  по локальному адресу mDNS: https://meteost.local/
  Ядро esp32: 2.0.1 ; GyverPortal v3.5.3
  Created by Dmitry Baev
  //**************************************************************************
*/

#include <GyverPortal.h>
GyverPortal ui;

// Задаем статический IP-адрес:
IPAddress local_IP(192, 168, 4, 4);
// Задаем IP-адрес сетевого шлюза:
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 0, 0);

#include <EEPROM.h>

#define AP_SSID "IoTik32_MeteoSt"
#define AP_PASS "Meteo1351"

#include <BlynkSimpleEsp32.h>
#include <Adafruit_ADS1015.h>
#include <Wire.h>

#include <BH1750.h>    // добавляем библиотеку датчика освещенности // adding Light intensity sensor library
BH1750 LightSensor_1;  // BH1750

#include <Adafruit_Sensor.h>  // добавляем библиотеку датчика температуры, влажности и давления // adding Temp Hum Bar sensor library
#include <Adafruit_BME280.h>  // BME280
Adafruit_BME280 bme280;       //

#include "MCP3221.h"   //GUVA
#include <VEML6075.h>  //UV60

// Датчик дождя
#define RAIN_PIN 13
float rain;
// Датчик скорости ветра
#define WINDSPD_PIN 4
float wspeed;

// Датчик направления ветра
#define WINDDIR_PIN 34
float wdir;
String wind_dir_text;
// Периоды для таймеров
#define WINDSPD_UPDATE_TIME 10000
#define RAIN_UPDATE_TIME 10000
BlynkTimer timer_windspd;
BlynkTimer timer_rain;

// Выберите датчик УФ вашей сборки (ненужное занесите в комментарии)
// #define MGS_GUVA 1
//#define MGS_UV60 1
bool Sens_GUVA = false;
//#ifdef MGS_GUVA
const byte DEV_ADDR = 0x4F;  // 0x5С , 0x4D (также попробуйте просканировать адрес: https://github.com/MAKblC/Codes/tree/master/I2C%20scanner)
MCP3221 mcp3221(DEV_ADDR);
//#endif
//#ifdef MGS_UV60
VEML6075 veml6075;
//#endif

// Счетчик импульсов датчика дождя
static volatile uint16_t rain_rate = 0;

// Счетчик импульсов датчика скорости ветра
static volatile uint16_t wind_speed = 0;

void IRAM_ATTR counterRain() {
  rain_rate = rain_rate + 1;
}

// Обработчик прерывания с датчика скорости ветра
void IRAM_ATTR counterWind() {
  wind_speed = wind_speed + 1;
}

const char* namesTemp[] = {
  "Температура (минутная), °C"
};

const char* namesTempDay[] = {
  "Температура (дневная), °C"
};

const char* namesWind[] = {
  "Скорость ветра, м/с"
};

//Переменные для хранения данных с датчиков
int light, temp, hum, press, uv;

struct LoginPass {
  char ssid[20];
  char pass[20];
};
LoginPass lp;

bool chSSID, chPASS, rebutAlert;

byte modWiFi = 1;  // !0 - режим AP, 0 - режим STA

unsigned long t = 0;
//////////////////////////////////////////НАСТРОЙКИ/CONFIGURATION/////////////////////////////////////////////////////////////////

void build() {
  GP.BUILD_BEGIN(1900);  // ширина колонки

  GP.THEME(GP_DARK);

  if (modWiFi == 0) {
    GP.UPDATE("lightS,humS,uvS,prS,rainS,flS,TempS,SpWind");
    GP.TITLE("Школьная метеостанция ЙоТик М2");
    GP.HR();


    M_GRID(
      M_BOX(GP_CENTER, GP.AJAX_PLOT("Temp", namesTemp, 1, 24, 60000, 350););
      M_BOX(GP_CENTER, GP.AJAX_PLOT("TempDay", namesTempDay, 1, 24, 3600000, 350);););

    M_GRID(
      M_BLOCK_TAB(
        "Освещённость", "", GP_YELLOW_B,
        M_BOX(GP_CENTER, GP.LABEL("0", "lightS", GP_YELLOW_B, 80, 1); GP.LABEL("Люкс");););

      GP_MAKE_BLOCK_TAB(
        "Влажность", "", "#00e1ff",
        M_BOX(GP_CENTER, GP.LABEL("0", "humS", "#00e1ff", 80, 1); GP.LABEL("%");););

      GP_MAKE_BLOCK_TAB(
        "Индекс Ультрафиолета", "", "#c0f",
        M_BOX(GP_CENTER, GP.LABEL("0", "uvS", "#c0f", 80, 1);););

      GP_MAKE_BLOCK_TAB(
        "Давление", "", "#ff002f",
        M_BOX(GP_CENTER, GP.LABEL("0", "prS", "#ff002f", 80, 1); GP.LABEL("мм рт.ст.");););

    );

    M_GRID(
      M_TABLE(
        M_TD(
          GP_MAKE_BLOCK_TAB(
            "Осадки", "", "#00ffb3",
            M_BOX(GP_CENTER, GP.LABEL("0", "rainS", "#00ffb3", 80, 1); GP.LABEL("мм");););

          GP_MAKE_BLOCK_TAB(
            "Направление ветра", "", "#ffc800",
            M_BOX(GP_CENTER, GP.LABEL("0", "flS", "#ffc800", 80, 1);););

        ););
      M_BOX(GP_CENTER, GP.AJAX_PLOT("Wind", namesWind, 1, 24, 60000, 330););

    );

    M_SPOILER(
      "IP адрес",
      GP.LABEL(WiFi.localIP().toString()););

  } else {
    GP.UPDATE("lightS,humS,uvS,prS,rainS,flS,TempS,SpWind,checkSSID,checkPASS");
    GP.TITLE("Школьная метеостанция ЙоТик М2");
    GP.HR();
    GP.ALERT("rebut", "IoTik перезагружается");

    M_GRID(
      M_BLOCK_TAB(
        "Температура", "", GP_ORANGE_B,
        M_BOX(GP_CENTER, GP.LABEL("0", "TempS", GP_ORANGE_B, 80, 1); GP.LABEL(" °C");););

      M_BLOCK_TAB(
        "Скорость ветра", "", GP_GRAY_B,
        M_BOX(GP_CENTER, GP.LABEL("0", "SpWind", GP_GRAY_B, 80, 1); GP.LABEL(" м/с");););

    );

    M_GRID(
      M_BLOCK_TAB(
        "Освещённость", "", GP_YELLOW_B,
        M_BOX(GP_CENTER, GP.LABEL("0", "lightS", GP_YELLOW_B, 80, 1); GP.LABEL("Люкс");););

      GP_MAKE_BLOCK_TAB(
        "Влажность", "", "#00e1ff",
        M_BOX(GP_CENTER, GP.LABEL("0", "humS", "#00e1ff", 80, 1); GP.LABEL("%");););

      GP_MAKE_BLOCK_TAB(
        "Индекс Ультрафиолета", "", "#c0f",
        M_BOX(GP_CENTER, GP.LABEL("0", "uvS", "#c0f", 80, 1);););

      GP_MAKE_BLOCK_TAB(
        "Давление", "", "#ff002f",
        M_BOX(GP_CENTER, GP.LABEL("0", "prS", "#ff002f", 80, 1); GP.LABEL("мм рт.ст.");););

    );

    M_GRID(

      GP_MAKE_BLOCK_TAB(
        "Осадки", "", "#00ffb3",
        M_BOX(GP_CENTER, GP.LABEL("0", "rainS", "#00ffb3", 80, 1); GP.LABEL("мм");););

      GP_MAKE_BLOCK_TAB(
        "Направление ветра", "", "#ffc800",
        M_BOX(GP_CENTER, GP.LABEL("0", "flS", "#ffc800", 80, 1);););

    );

    M_SPOILER("Подключение к Wi-Fi",
              GP_MAKE_BOX(GP_CENTER, GP.LABEL("Логин"); GP.TEXT("ssidT", "Логин"); GP.CHECK("checkSSID", chSSID, GP_GREEN, true););
              GP_MAKE_BOX(GP_CENTER, GP.LABEL("Пароль"); GP.TEXT("passT", "Пароль"); GP.CHECK("checkPASS", chPASS, GP_GREEN, true););
              GP.BUTTON("connect", "Подключиться");
              GP.UPDATE_CLICK("rebut", "connect");
              GP.HR();
              GP.LABEL("Адрес после подключения");
              GP.LABEL_BLOCK("http://meteost.local/");

    );
  }

  GP.BUILD_END();
}

void action() {
  if (modWiFi == 0) {

    if (ui.update("Temp")) ui.answer(temp);
    if (ui.update("TempDay")) ui.answer(temp);
    if (ui.update("Wind")) ui.answer(wspeed);
    if (ui.update("lightS")) ui.answer(light);
    if (ui.update("prS")) ui.answer(press);
    if (ui.update("humS")) ui.answer(hum);
    if (ui.update("uvS")) ui.answer(uv);
    if (ui.update("rainS")) ui.answer(rain);
    if (ui.update("flS")) ui.answer(wind_dir_text);
    if (ui.update("TempS")) ui.answer(temp);
    if (ui.update("SpWind")) ui.answer(wspeed);

  } else {

    if (ui.click()) {
      if (ui.click("ssidT")) {
        ui.copyStr("ssidT", lp.ssid);
        chSSID = true;
      }
      if (ui.click("passT")) {
        ui.copyStr("passT", lp.pass);
        chPASS = true;
      }
      if (ui.click("connect")) {
        EEPROM.put(0, lp);  // сохраняем
        EEPROM.commit();    // записываем
        rebutAlert = 1;
      }
    }

    if (ui.update("lightS")) ui.answer(light);
    if (ui.update("prS")) ui.answer(press);
    if (ui.update("humS")) ui.answer(hum);
    if (ui.update("uvS")) ui.answer(uv);
    if (ui.update("rainS")) ui.answer(rain);
    if (ui.update("flS")) ui.answer(wind_dir_text);
    if (ui.update("TempS")) ui.answer(temp);
    if (ui.update("SpWind")) ui.answer(wspeed);

    if (ui.update()) {
      ui.updateBool("checkSSID", chSSID);
      ui.updateBool("checkPASS", chPASS);

      if (rebutAlert) {
        rebutAlert = 0;
        ui.answer(1);
        modWiFi = 0;
        EEPROM.put(99, modWiFi);
        EEPROM.commit();
        unsigned long t = millis();
        while (t + 2500 > millis()) {
          ui.tick();
        }
        ESP.restart();
      }
    }
  }
}


void setup() {
  Wire.begin();

  Serial.begin(115200);

  EEPROM.begin(100);
  EEPROM.get(0, lp);
  EEPROM.get(99, modWiFi);

  pinMode(4, OUTPUT);
  pinMode(18, OUTPUT);
  delay(100);

  if (modWiFi == 0) {
    Serial.println("WiFi mode: " + String(lp.ssid));
    Serial.println("ssid: " + String(lp.pass));
    WiFi.mode(WIFI_STA);
    WiFi.begin(lp.ssid, lp.pass);
    while (WiFi.status() != WL_CONNECTED) {
      digitalWrite(4, 0);
      digitalWrite(18, 0);
      delay(200);
      digitalWrite(4, 1);
      digitalWrite(18, 1);
      Serial.print(".");
      delay(300);
      if (millis() >= 30000) {
        modWiFi = 1;
        EEPROM.put(99, modWiFi);
        EEPROM.commit();
        EEPROM.get(99, modWiFi);
        Serial.println(modWiFi);
        delay(250);
        ESP.restart();
      }
    }
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi: " + String(AP_SSID));
    Serial.println("ssid: " + String(AP_PASS));
    WiFi.mode(WIFI_AP);
    Serial.print("Setting soft-AP configuration ... ");
    Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");
    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.println(WiFi.softAPIP());
  }

  if (!veml6075.begin()) {
    Serial.println("VEML6075 not found!");

    mcp3221.setVinput(VOLTAGE_INPUT_5V);
    Sens_GUVA = true;
  }


  LightSensor_1.begin();  // запуск датчика освещенности // turn the light intensity sensor on

bool bme_status = bme280.begin();
  if (!bme_status) {
    Serial.println("Не найден по адресу 0х77, пробую другой...");
    bme_status = bme280.begin(0x76);
    if (!bme_status)
      Serial.println("Датчик не найден, проверьте соединение");
  }

  // подключаем конструктор и запускаем
  ui.attachBuild(build);
  ui.attach(action);
  ui.start("meteost");
  Serial.println("Веб страница запущена");

  // Инициализация входов датчиков дождя и скорости ветра
  pinMode(RAIN_PIN, INPUT_PULLUP);
  pinMode(WINDSPD_PIN, INPUT_PULLUP);

  // Инициализация прерываний на входах с импульсных датчиков
  attachInterrupt(digitalPinToInterrupt(RAIN_PIN), counterRain, FALLING);
  attachInterrupt(digitalPinToInterrupt(WINDSPD_PIN), counterWind, FALLING);

  // Инициализация таймеров
  timer_windspd.setInterval(WINDSPD_UPDATE_TIME, readSensorWINDSPD);
  timer_rain.setInterval(RAIN_UPDATE_TIME, readSensorRAIN);
}

static uint32_t tmr;
byte rebCount = 0;

void loop() {

  ui.tick();

  if (millis() - tmr >= 500) {
    tmr = millis();
    if (modWiFi == 0) Serial.println("IP: " + WiFi.localIP().toString());
    light = LightSensor_1.readLightLevel();
    // Вывод измеренных значений в терминал
    Serial.println(String(light) + " lx");
    temp = bme280.readTemperature();
    Serial.println("Air temperature = " + String(temp) + " *C");
    hum = bme280.readHumidity();
    Serial.println("Air humidity = " + String(hum) + " %");
    press = bme280.readPressure() / 100.0F / 1.33F;
    Serial.println("Air press = " + String(press) + " hPa");
    if (Sens_GUVA == false) {
      veml6075.poll();
      uv = veml6075.getUVIndex();
      float uva = veml6075.getUVA();
      float uvb = veml6075.getUVB();
      float uv_index = veml6075.getUVIndex();
      Serial.println("UVA " + String(uva, 1));
      Serial.println("UVB " + String(uvb, 1));
      Serial.println("UVI " + String(uv_index, 1));      
    } else {
      float sensorVoltage;
      float sensorValue;
      sensorValue = mcp3221.getVoltage();
      sensorVoltage = 1000 * (sensorValue / 4096 * 5.0);  // напряжение на АЦП
      uv = 370 * sensorVoltage / 200000;                  // Индекс УФ (эмпирическое измерение)
      Serial.println("UV = " + String(uv) + " index");
    }

    Serial.println("UV = " + String(uv) + " index");
    Serial.println(String(wspeed) + " m/s");
    Serial.println(wind_dir_text);
    Serial.println(rain);
    readSensorWINDDIR();
  }

  timer_windspd.run();
  timer_rain.run();

  if (modWiFi == 0)
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.mode(WIFI_STA);
      WiFi.begin(lp.ssid, lp.pass);
      while (WiFi.status() != WL_CONNECTED) {
        digitalWrite(4, 0);
        digitalWrite(18, 0);
        delay(200);
        digitalWrite(4, 1);
        digitalWrite(18, 1);
        Serial.print(".");
        delay(300);
      }
    }
}

// Чтение датчика скорости ветра
void readSensorWINDSPD() {
  wspeed = wind_speed / 10.0 * 2.4 / 3.6;
  wind_speed = 0;
}

// Чтение датчика направления ветра
void readSensorWINDDIR() {
  double sensval = analogRead(34) * 5.0 / 1023.0;
  double delta = 0.2;
  wdir = 0;
  wind_dir_text = "";
  if (sensval >= 11.4 && sensval < 12.35) {
    wdir = 0.0;  // N
    wind_dir_text = "Север";
  } else if (sensval >= 6.45 && sensval < 8) {
    wdir = 45.0;  // NE
    wind_dir_text = "Северо-восток";
  } else if (sensval >= 0.7 && sensval < 1.05) {
    wdir = 90.0;  // E
    wind_dir_text = "Восток";
  } else if (sensval >= 1.8 && sensval < 2.8) {
    wdir = 135.0;  // SE
    wind_dir_text = "Юго-восток";
  } else if (sensval >= 3.7 && sensval < 5) {
    wdir = 180.0;  // S
    wind_dir_text = "Юг";
  } else if (sensval >= 9.35 && sensval < 10.2) {
    wdir = 225.0;  // SW
    wind_dir_text = "Юго-запад";
  } else if (sensval >= 14.1) {
    wdir = 270.0;  // W
    wind_dir_text = "Запад";
  } else if (sensval >= (13.1) && sensval < 14.1) {
    wdir = 315.0;  // NW
    wind_dir_text = "Северо-запад";
  }
  // Serial.println("wind direction" + String(wdir, 1) + String(wind_dir_text));
}

// Чтение датчика уровня осадков
void readSensorRAIN() {
  rain = rain_rate * 0.2794;
  rain_rate = 0;
}