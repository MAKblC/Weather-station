#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <BH1750FVI.h>
#include <VEML6075.h>

#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <WiFi.h>
#define BLYNK_PRINT Serial

#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

#define WIFI_SSID "XXXXXXXXXXX"
#define WIFI_PASSWORD "XXXXXXXXX"
#define BOT_TOKEN "XXXXXXXXX:XXXXXXXXXXXXXXXX"

#define LED 4 // ЙоТик 32А, 18 - ЙоТик 32В

// Датчик ультрафиолета
VEML6075 veml6075;
// Датчик освещенности
BH1750FVI LightSensor_1;
// Датчик температуры/влажности и атмосферного давления
Adafruit_BME280 bme280;

// Датчик дождя
#define RAIN_PIN 13 // 4 
float rain;
// Датчик скорости ветра
#define WINDSPD_PIN 4 // 13
float wspeed;

// Датчик направления ветра
#define WINDDIR_PIN 34
float wdir;
String wind_dir_text;
// Периоды для таймеров
#define WINDSPD_UPDATE_TIME  10000
#define RAIN_UPDATE_TIME     10000
BlynkTimer timer_windspd;
BlynkTimer timer_rain;

const unsigned long BOT_MTBS = 1000; // период обновления сканирования новых сообщений
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime;

// ссылка для поста фотографии
String test_photo_url = "https://mgbot.ru/upload/logo-r.png";

// отобразить кнопки перехода на сайт с помощью InlineKeyboard
String keyboardJson1 = "[[{ \"text\" : \"Ваш сайт\", \"url\" : \"https://mgbot.ru\" }],[{ \"text\" : \"Перейти на сайт IoTik.ru\", \"url\" : \"https://www.iotik.ru\" }]]";

// Счетчик импульсов датчика дождя
static volatile uint16_t rain_rate = 0;

// Счетчик импульсов датчика скорости ветра
static volatile uint16_t wind_speed = 0;

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
  pinMode(LED, OUTPUT);
  Serial.begin(115200);
  delay(512);
  Serial.println();
  Serial.print("Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

  if (!veml6075.begin())
    Serial.println("VEML6075 not found!");
  bool bme_status = bme280.begin();
  if (!bme_status)
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
  LightSensor_1.begin();
  LightSensor_1.setMode(Continuously_High_Resolution_Mode);

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

void loop()
{
  if (millis() - bot_lasttime > BOT_MTBS)
  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages)
    {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    bot_lasttime = millis();
  }
  timer_windspd.run();
  timer_rain.run();
}

void handleNewMessages(int numNewMessages)
{
  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));
  for (int i = 0; i < numNewMessages; i++)
  {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;
    text.toLowerCase();
    String from_name = bot.messages[i].from_name;
    if (from_name == "") from_name = "Guest";
    if ((text == "/sensors") || (text == "sensors"))
    {
      readSensorWINDDIR();

      veml6075.poll();
      float uva = veml6075.getUVA();
      float uvb = veml6075.getUVB();
      float uv_index = veml6075.getUVIndex();

      float t = bme280.readTemperature();
      float h = bme280.readHumidity();
      float p = bme280.readPressure() / 100.0F;

      float l = LightSensor_1.getAmbientLight();

      String welcome = "Показания датчиков:\n";
      welcome += "Temp: " + String(t, 1) + " C\n";
      welcome += "Hum: " + String(h, 0) + " %\n";
      welcome += "Press: " + String(p, 0) + " hPa\n";
      welcome += "Light: " + String(l, 0) + " Lx\n";
      welcome += "UVA: " + String(uva, 0) + " mkWt/cm2\n";
      welcome += "UVB: " + String(uvb, 0) + " mkWt/cm2\n";
      welcome += "UVIndex: " + String(uv_index, 0) + " \n";
      welcome += "Wind direction: " + wind_dir_text + " " + String(wdir, 1) + " deg\n";
      welcome += "Wind speed: " + String(wspeed, 1) + " m/s\n";
      welcome += "Rain level: " + String(rain, 1) + " mm\n";
      bot.sendMessage(chat_id, welcome, "Markdown");
    }

    if (text == "/photo") { // пост фотографии
      bot.sendPhoto(chat_id, test_photo_url, "а вот и фотка!");
    }
    if ((text == "/light") || (text == "light"))
    {
      digitalWrite(LED, HIGH);
      bot.sendMessage(chat_id, "Светодиод включен", "");
    }
    if ((text == "/off") || (text == "off"))
    {
      digitalWrite(LED, LOW);
      bot.sendMessage(chat_id, "Светодиод выключен", "");
    }

    if (text == "/site") // отобразить кнопки в диалоге для перехода на сайт
    {
      bot.sendMessageWithInlineKeyboard(chat_id, "Выберите действие", "", keyboardJson1);
    }
    if (text == "/options") // клавиатура для управления теплицей
    {
      String keyboardJson = "[[\"/light\", \"/off\"],[\"/sensors\"]]";
      bot.sendMessageWithReplyKeyboard(chat_id, "Выберите команду", "", keyboardJson, true);
    }

    if ((text == "/start") || (text == "start") || (text == "/help") || (text == "help"))
    {
      bot.sendMessage(chat_id, "Привет, " + from_name + "!", "");
      bot.sendMessage(chat_id, "Я контроллер Йотик 32. Команды смотрите в меню слева от строки ввода", "");
      String sms = "Команды:\n";
      sms += "/options - пульт управления\n";
      sms += "/site - перейти на сайт\n";
      sms += "/photo - запостить фото\n";
      sms += "/help - вызвать помощь\n";
      bot.sendMessage(chat_id, sms, "Markdown");
    }
  }
}


// Чтение датчика скорости ветра
void readSensorWINDSPD()
{
  wspeed = wind_speed / 10.0 * 2.4 / 3.6;
  wind_speed = 0;
}

// Чтение датчика направления ветра
void readSensorWINDDIR()
{
  double sensval = analogRead(34) * 5.0 / 1023.0;
  double delta = 0.2;
  wdir = 0;
  wind_dir_text = "";
  if (sensval >= 11.4 && sensval < 12.35) {
    wdir = 0.0; // N
    wind_dir_text = "N";
  } else if (sensval >= 5 && sensval < 6.45) {
    wdir = 22.5; // NNE
    wind_dir_text = "NNE";
  } else if (sensval >= 6.45 && sensval < 8) {
    wdir = 45.0; // NE
    wind_dir_text = "NE";
  } else if (sensval >= 0.48 && sensval < 0.7) {
    wdir = 67.5; // ENE
    wind_dir_text = "ENE";
  } else if (sensval >= 0.7 && sensval < 1.05) {
    wdir = 90.0; // E
    wind_dir_text = "E";
  } else if (sensval < 0.48) {
    wdir = 112.5; // ESE
    wind_dir_text = "ESE";
  } else if (sensval >= 1.8 && sensval < 2.8) {
    wdir = 135.0; // SE
    wind_dir_text = "SE";
  } else if (sensval >= 1.05 && sensval < 1.8) {
    wdir = 157.5; // SSE
    wind_dir_text = "SSE";
  } else if (sensval >= 3.7 && sensval < 5) {
    wdir = 180.0; // S
    wind_dir_text = "S";
  } else if (sensval >= 2.8 && sensval < 3.7) {
    wdir = 202.5; // SSW
    wind_dir_text = "SSW";
  } else if (sensval >= 9.35 && sensval < 10.2) {
    wdir = 225.0; // SW
    wind_dir_text = "SW";
  } else if (sensval >= 8 && sensval < 9.35) {
    wdir = 247.5; // WSW
    wind_dir_text = "WSW";
  } else if (sensval >= 14.1) {
    wdir = 270.0; // W
    wind_dir_text = "W";
  } else if (sensval >= 12.35 && sensval < 13.1) {
    wdir = 292.5; // WNW
    wind_dir_text = "WNW";
  } else if (sensval >= (13.1) && sensval < 14.1) {
    wdir = 315.0; // NW
    wind_dir_text = "NW";
  } else if (sensval >= 10.2 && sensval < 11.4) {
    wdir = 337.5; // NNW
    wind_dir_text = "NNW";
  }
  // Serial.println("wind direction" + String(wdir, 1) + String(wind_dir_text));
}

// Чтение датчика уровня осадков
void readSensorRAIN()
{
  rain = rain_rate * 0.2794;
  rain_rate = 0;
}
