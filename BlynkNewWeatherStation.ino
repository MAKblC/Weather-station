/*
Не забудьте поправить настройки и адреса устройств в зависимости от комплектации!  
*/

//Необходимые библиотеки для Blynk  и датчиков
#define BLYNK_PRINT Serial
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>
#include <VEML6075.h>
#include "MCP3221.h"

// Выберите датчик УФ вашей сборки (ненужное занесите в комментарии)
// #define MGS_GUVA 1
#define MGS_UV60 1

// Датчик УФ
#ifdef MGS_GUVA
const byte DEV_ADDR = 0x4F;  // 0x5С , 0x4D (также попробуйте просканировать адрес: https://github.com/MAKblC/Codes/tree/master/I2C%20scanner)
MCP3221 mcp3221(DEV_ADDR);
#endif
#ifdef MGS_UV60
VEML6075 veml6075;
#endif

#define BLYNK_TEMPLATE_ID "XXXXXXXX"
#define BLYNK_TEMPLATE_NAME "XXXXXXXX"
#define BLYNK_AUTH_TOKEN "XXXXXXXXXXXXXXXXXXXXXX"

char ssid[] = "XXXXXX"; // логин Wi-Fi
char pass[] = "XXXXXXXXX"; // пароль Wi-Fi
char auth[] = BLYNK_AUTH_TOKEN;

// Датчик освещенности
BH1750 lightMeter;

// Датчик температуры/влажности и атмосферного давления
Adafruit_BME280 bme280;

// Датчик дождя
#define RAIN_PIN 13 //4 (в зависимости от сборки)

// Датчик скорости ветра
#define WINDSPD_PIN 4 // 13 (в зависимости от сборки)

// Датчик направления ветра
#define WINDDIR_PIN 34

// Периоды для таймеров
#define BME280_UPDATE_TIME   5000
#define BH1750_UPDATE_TIME   5100
#define WINDDIR_UPDATE_TIME  5200
#define WINDSPD_UPDATE_TIME  10000
#define RAIN_UPDATE_TIME     10000

// Таймеры
BlynkTimer timer_bme280;
BlynkTimer timer_bh1750;
BlynkTimer timer_windspd;
BlynkTimer timer_winddir;
BlynkTimer timer_rain;

// Счетчик импульсов датчика дождя
static volatile uint16_t rain_rate = 0;

// Счетчик импульсов датчика скорости ветра
static volatile uint16_t wind_speed = 0;

// Параметры сенсоров для IoT сервера
#define sensorCount 7
char* sensorNames[] = {"air_temp", "air_hum", "air_press", "sun_light", "wind_spd", "wind_dir", "rain"};
float sensorValues[sensorCount];
// Номера датчиков
#define air_temp     0x00
#define air_hum      0x01
#define air_press    0x02
#define sun_light    0x03
#define wind_spd     0x04
#define wind_dir     0x05
#define rain         0x06

// Обработчик прерывания с датчика осадков
void IRAM_ATTR counterRain()
{
  rain_rate = rain_rate + 1;
}

// Обработчик прерывания с датчика скорости ветра
void IRAM_ATTR counterWind()
{
  wind_speed = wind_speed + 1;
}

void setup()
{
  // Инициализация последовательного порта
  Serial.begin(115200);
  delay(512);
  
  #ifdef MGS_UV60
  if (!veml6075.begin())
    Serial.println("VEML6075 not found!");
#endif
#ifdef MGS_GUVA
  mcp3221.setVinput(VOLTAGE_INPUT_5V);
#endif
    
  // Инициализация Blynk и Wi-Fi
  Serial.println();
    Serial.print("Connecting to ");
  Serial.println(ssid);
  Blynk.begin(auth, ssid, pass, "blynk.cloud", 80);

  // Инициализация входов датчиков дождя и скорости ветра
  pinMode(RAIN_PIN, INPUT_PULLUP);
  pinMode(WINDSPD_PIN, INPUT_PULLUP);

  // Инициализация прерываний на входах с импульсных датчиков
  attachInterrupt(digitalPinToInterrupt(RAIN_PIN), counterRain, FALLING);
  attachInterrupt(digitalPinToInterrupt(WINDSPD_PIN), counterWind, FALLING);

  // Однократный опрос датчиков

  readSensorBME280();
  readSensorBH1750();
  readSensorWINDSPD();
  readSensorWINDDIR();
  readSensorRAIN();

  // Вывод в терминал данных с датчиков
  printAllSensors();

  // Инициализация таймеров
  timer_bme280.setInterval(BME280_UPDATE_TIME, readSensorBME280);
  timer_bh1750.setInterval(BH1750_UPDATE_TIME, readSensorBH1750);
  timer_windspd.setInterval(WINDSPD_UPDATE_TIME, readSensorWINDSPD);
  timer_winddir.setInterval(WINDDIR_UPDATE_TIME, readSensorWINDDIR);
  timer_rain.setInterval(RAIN_UPDATE_TIME, readSensorRAIN);
}

void loop()
{
  Blynk.run();
  timer_bme280.run();
  timer_bh1750.run();
  timer_windspd.run();
  timer_winddir.run();
  timer_rain.run();
}

// Чтение датчика BH1750
void readSensorBH1750()
{
  Wire.begin(21, 22);        // Инициализация I2C на выводах 0, 2
  Wire.setClock(10000L);   // Снижение тактовой частоты для надежности
  lightMeter.begin();           // Инициализация датчика
  delay(128);
  sensorValues[sun_light] = lightMeter.readLightLevel();
  Blynk.virtualWrite(V3, sensorValues[sun_light]); delay(25);
  Serial.println("light");
}

// Чтение датчика BME280, VEML6075 и отправка данных на сервер
void readSensorBME280()
{
  #ifdef MGS_UV60
  veml6075.poll();
  float uva = veml6075.getUVA();
  float uvb = veml6075.getUVB();
  float uv_index = veml6075.getUVIndex();
  Blynk.virtualWrite(V6, uva); delay(25);
  Blynk.virtualWrite(V7, uvb); delay(25);
  Blynk.virtualWrite(V8, uv_index); delay(25);
  #endif
  #ifdef MGS_GUVA
  float sensorVoltage;
  float sensorValue;
  float UV_index;
  sensorValue = mcp3221.getVoltage();
  sensorVoltage = 1000 * (sensorValue / 4096 * 5.0); // напряжение на АЦП
  UV_index = 370 * sensorVoltage / 200000; // Индекс УФ (эмпирическое измерение)
  Blynk.virtualWrite(V6, sensorVoltage); delay(25);
  Blynk.virtualWrite(V8, UV_index); delay(25);
  #endif
  float p = 0;
  Wire.begin(21, 22);         // Инициализация I2C на выводах 4, 5
  Wire.setClock(10000L);    // Снижение тактовой частоты для надежности
  bme280.begin();           // Инициализация датчика
  delay(128);
  sensorValues[air_temp] = bme280.readTemperature();
  sensorValues[air_hum] = bme280.readHumidity();
  sensorValues[air_press] = bme280.readPressure() * 7.5006 / 1000.0;
  Blynk.virtualWrite(V0, sensorValues[air_temp]); delay(25);
  Blynk.virtualWrite(V1, sensorValues[air_hum]); delay(25);
  Blynk.virtualWrite(V2, sensorValues[air_press]); delay(25);
  Serial.println("temperature");
  Serial.println("humidity");
  Serial.println("pressure");
}

// Чтение датчика скорости ветра
void readSensorWINDSPD()
{
  sensorValues[wind_spd] = wind_speed / 10.0 * 2.4 / 3.6;
  wind_speed = 0;
  Blynk.virtualWrite(V5, sensorValues[wind_spd]); delay(25);
  Serial.println("wind speed");
}

// Чтение датчика направления ветра
void readSensorWINDDIR()
{
  double sensval = analogRead(34) * 5.0 / 1023.0; // может потребоваться калибровка, если значения не совпадают
  double delta = 0.2;
  String wind_dir_text = "";
  if (sensval >= 11.4 && sensval < 12.35) {
    sensorValues[wind_dir] = 0.0; // N
    wind_dir_text = "N";
  } else if (sensval >= 5 && sensval < 6.45) {
    sensorValues[wind_dir] = 22.5; // NNE
    wind_dir_text = "NNE";
  } else if (sensval >= 6.45 && sensval < 8) {
    sensorValues[wind_dir] = 45.0; // NE
    wind_dir_text = "NE";
  } else if (sensval >= 0.48 && sensval < 0.7) {
    sensorValues[wind_dir] = 67.5; // ENE
    wind_dir_text = "ENE";
  } else if (sensval >= 0.7 && sensval < 1.05) {
    sensorValues[wind_dir] = 90.0; // E
    wind_dir_text = "E";
  } else if (sensval < 0.48) {
    sensorValues[wind_dir] = 112.5; // ESE
    wind_dir_text = "ESE";
  } else if (sensval >= 1.8 && sensval < 2.8) {
    sensorValues[wind_dir] = 135.0; // SE
    wind_dir_text = "SE";
  } else if (sensval >= 1.05 && sensval < 1.8) {
    sensorValues[wind_dir] = 157.5; // SSE
    wind_dir_text = "SSE";
  } else if (sensval >= 3.7 && sensval < 5) {
    sensorValues[wind_dir] = 180.0; // S
    wind_dir_text = "S";
  } else if (sensval >= 2.8 && sensval < 3.7) {
    sensorValues[wind_dir] = 202.5; // SSW
    wind_dir_text = "SSW";
  } else if (sensval >= 9.35 && sensval < 10.2) {
    sensorValues[wind_dir] = 225.0; // SW
    wind_dir_text = "SW";
  } else if (sensval >= 8 && sensval < 9.35) {
    sensorValues[wind_dir] = 247.5; // WSW
    wind_dir_text = "WSW";
  } else if (sensval >= 14.1) {
    sensorValues[wind_dir] = 270.0; // W
    wind_dir_text = "W";
  } else if (sensval >= 12.35 && sensval < 13.1) {
    sensorValues[wind_dir] = 292.5; // WNW
    wind_dir_text = "WNW";
  } else if (sensval >= (13.1) && sensval < 14.1) {
    sensorValues[wind_dir] = 315.0; // NW
    wind_dir_text = "NW";
  } else if (sensval >= 10.2 && sensval < 11.4) {
    sensorValues[wind_dir] = 337.5; // NNW
    wind_dir_text = "NNW";
  }
  Blynk.virtualWrite(V9, sensorValues[wind_dir]); delay(25);
  Serial.println("wind direction");
  Serial.println(sensval);
}

// Чтение датчика уровня осадков
void readSensorRAIN()
{
  sensorValues[rain] = rain_rate * 0.2794;
  rain_rate = 0;
  Blynk.virtualWrite(V4, sensorValues[rain]); delay(25);
  Serial.println("rain rate");
}

// Print sensors data to terminal
void printAllSensors()
{
  for (int i = 0; i < sensorCount; i++)
  {
    Serial.print(sensorNames[i]);
    Serial.print(" = ");
    Serial.println(sensorValues[i]);
  }
  Serial.println();
}
