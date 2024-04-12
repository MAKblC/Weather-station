#include "WiFi.h"
#include <HTTPClient.h>
#include "time.h"
const char* ntpServer = "pool.ntp.org";  // отсюда считается текущие дата/время
const long gmtOffset_sec = 10800;
const int daylightOffset_sec = 0;
// WiFi credentials
const char* ssid = "MGBot";              // логин Wi-Fi
const char* password = "Terminator812";  // пароль Wi-Fi
// Google script ID and required credentials
String GOOGLE_SCRIPT_ID = "AKfycbypEpOD8OYJozXTMuHR0OXfrKoc8dQMrg1aisMw8To_JhydYxH-4GLnBE9kGq5jS5nv";  // Script ID
int count = 0;
int wdir = 0;
String wind_dir_text = "";
#include <WiFiClient.h>
#include <SimpleTimer.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>
#include "MCP3221.h"

String urlFinal;
// Выберите датчик УФ вашей сборки (ненужное занесите в комментарии)
#define MGS_GUVA 1
//#define MGS_UV60 1
//VEML6075 veml6075;
// Датчик УФ
const byte DEV_ADDR = 0x4D;  // 0x5С , 0x4D (также попробуйте просканировать адрес: https://github.com/MAKblC/Codes/tree/master/I2C%20scanner)
MCP3221 mcp3221(DEV_ADDR);
bool Sens_GUVA = false;

int uv = 0;
// Датчик освещенности
BH1750 lightMeter;

// Датчик температуры/влажности и атмосферного давления
Adafruit_BME280 bme280;

// Датчик дождя
#define RAIN_PIN 13

// Датчик скорости ветра
#define WINDSPD_PIN 4

// Датчик направления ветра
#define WINDDIR_PIN 34

// Периоды для таймеров
#define BME280_UPDATE_TIME 5000
#define BH1750_UPDATE_TIME 5100
#define WINDDIR_UPDATE_TIME 5200
#define WINDSPD_UPDATE_TIME 10000
#define RAIN_UPDATE_TIME 10000

// Таймеры
SimpleTimer timer_bme280;
SimpleTimer timer_bh1750;
SimpleTimer timer_windspd;
SimpleTimer timer_winddir;
SimpleTimer timer_rain;

// Счетчик импульсов датчика дождя
static volatile uint16_t rain_rate = 0;
static volatile uint16_t wind_speed = 0;

void IRAM_ATTR counterRain() {
  rain_rate = rain_rate + 1;
}

// Обработчик прерывания с датчика скорости ветра
void IRAM_ATTR counterWind() {
  wind_speed = wind_speed + 1;
}

void setup() {
  delay(1000);
  Serial.begin(115200);
  delay(1000);
  // connect to WiFi
  Serial.println();
  Serial.print("Connecting to wifi: ");
  Serial.println(ssid);
  Serial.flush();
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  mcp3221.setVinput(VOLTAGE_INPUT_5V);
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

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    static bool flag = false;
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to obtain time");
      return;
    }
    String urlFinal = "https://script.google.com/macros/s/" + GOOGLE_SCRIPT_ID + "/exec?";

    char timeStringBuff[50];                                                           //50 chars should be enough
    strftime(timeStringBuff, sizeof(timeStringBuff), "%d %m %Y %H:%M:%S", &timeinfo);  // получение даты и времени
    String asString(timeStringBuff);
    asString.replace(" ", "-");
    urlFinal += "time=" + asString;

    Wire.begin(21, 22);     // Инициализация I2C на выводах 0, 2
    Wire.setClock(10000L);  // Снижение тактовой частоты для надежности
    lightMeter.begin();     // Инициализация датчика
    delay(128);
    float l = lightMeter.readLightLevel();
    Serial.println("light: " + String(l, 1));

    urlFinal += "&svet=" + String(l, 1);

    float sensorVoltage;
    float sensorValue;
    sensorValue = mcp3221.getVoltage();
    sensorVoltage = 1000 * (sensorValue / 4096 * 5.0);  // напряжение на АЦП
    uv = 370 * sensorVoltage / 200000;                  // Индекс УФ (эмпирическое измерение)
    Serial.println("UV = " + String(uv) + " index");
    urlFinal += "&uv=" + String(uv);

    bool bme_status = bme280.begin();
    if (!bme_status) {
      Serial.println("Не найден по адресу 0х77, пробую другой...");
      bme_status = bme280.begin(0x76);
      if (!bme_status)
        Serial.println("Датчик не найден, проверьте соединение");
    }
    delay(128);

    float t = bme280.readTemperature();
    Serial.println("temperature: " + String(t, 1));
    urlFinal += "&temp=" + String(t, 1);

    float wspeed = wind_speed / 10.0 * 2.4 / 3.6;
    wind_speed = 0;
    Serial.println("wind speed" + String(wspeed, 1));
    urlFinal += "&speed=" + String(wspeed, 1);

    float rain = rain_rate * 0.2794;
    rain_rate = 0;
    Serial.println("rain rate" + String(rain, 1));
    urlFinal += "&rain=" + String(rain, 1);

    double sensval = analogRead(34) * 5.0 / 1023.0;
    double delta = 0.2;
    wdir = 0;
    wind_dir_text = "";
    if (sensval >= 11.4 && sensval < 12.35) {
      wdir = 0.0;  // N
      wind_dir_text = "Sever";
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
    urlFinal += "&windV=" + String(wdir);
    Serial.println("wind direction" + String(wdir, 1) + String(wind_dir_text));

    float press = bme280.readPressure() / 100.0F / 1.33F;
    urlFinal += "&press=" + String(press, 1);
    float hum = bme280.readHumidity();
    urlFinal += "&hum=" + String(hum, 1);

    //String urlFinal = "https://script.google.com/macros/s/" + GOOGLE_SCRIPT_ID + "/exec?" + "date=" + asString + "&sensor=" + String(count);  // запрос, составленный также, как и в ручном режиме (пункт 8 урока)
    Serial.print("POST data to spreadsheet:");
    Serial.println(urlFinal);
    HTTPClient http;
    http.begin(urlFinal.c_str());
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    int httpCode = http.GET();
    Serial.print("HTTP Status Code: ");
    Serial.println(httpCode);
    //---------------------------------------------------------------------
    //getting response from google sheet
    String payload;
    if (httpCode > 0) {
      payload = http.getString();
      Serial.println("Payload: " + payload);
    }
    //---------------------------------------------------------------------
    http.end();
  }
  delay(1000);
}

void readSensorWINDSPD() {
  float wspeed = wind_speed / 10.0 * 2.4 / 3.6;
  wind_speed = 0;
  Serial.println("wind speed" + String(wspeed, 1));
  urlFinal += "&speed=" + String(wspeed, 1);
}
void readSensorRAIN() {
  float rain = rain_rate * 0.2794;
  rain_rate = 0;
  Serial.println("rain rate" + String(rain, 1));
  urlFinal += "&rain=" + String(rain, 1);
}

String readSensorWINDDIR() {
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
  urlFinal += "&windV=" + String(wind_dir_text);
  Serial.println("wind direction" + String(wdir, 1) + String(wind_dir_text));
}
