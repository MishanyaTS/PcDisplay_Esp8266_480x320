#include <FS.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <ElegantOTA.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <GyverTimer.h>
#include <EncButton2.h>
#include <DNSServer.h>
#include <WiFiManager.h>
//Инициализация дисплея
File fsUploadFile; //для файловой системы
TFT_eSPI tft = TFT_eSPI(); // создаем экземпляр объекта TFT_eSPI

  #define BTN_PIN 4 //сенсорная кнопка

StaticJsonDocument<1600> owm; //выделение памяти для json файла погоды
const char* ssid = "ssid"; //ssid точки доступа
const char* password = "password"; //пароль
//настройки синхронизации погоды
const String api_1 = "http://api.openweathermap.org/data/2.5/weather?q=";
const String qLocation = "Nizhnevartovsk,ru";  //ваш город
const String api_2 = "&lang=ru&units=metric"; //язык и температура в градусах цельсия
const String api_3 = "&APPID=";
const String api_key = "xxxxx";  //ваш api ключ
//
int timezone = 5; //часовой пояс для синхронизации времени

byte syncerror=0; // Счетчик ошибок синхронизации с LHM. При достижении максимума будет отображать только погоду
volatile byte screen=0;// текущий экран
//обявление переменных для значений датчиков. Можно конечно все птимизировать и убрать их из глобальных, но это как-нибудь в следующих релизах
byte cpu_temp[100];
byte cpu_index=0;
byte gpu_temp[100];
byte gpu_index=0;
int sync_interval=1000;
int i=0;
String responce;
//Sensors init
  String dataServer="http://192.168.0.195:8085/data.json";
  String degree = degree.substring(degree.length()) + "°C";
  String percentage = percentage.substring(percentage.length()) + (char)37;
  float total;
  float vals[32];

//запуск веб сервера для настройки
 ESP8266WebServer server(80);

//NTP настройка
      WiFiUDP ntpUDP;
      NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600*timezone);
       //различные таймеры для опроса датчиков, обновления экраны и погоды
       GTimer refsens(MS);
       GTimer refscreen(MS);
       GTimer refweather(MS);
        //объявление сенсорной кнопки
     EncButton2<EB_BTN> butt1(INPUT, BTN_PIN);  
//небольшая кнопка для обработки нажатия кнопки по прерыванию
 IRAM_ATTR void btn_read(){
   if (butt1.click()) {
    tft.fillScreen(TFT_BLACK);
  screen++;
  if (screen>4) screen=0;
  }
 }
     
void setup(void) {
  // Инициализация дисплея
  tft.init(); // инициализируем дисплей
  tft.setRotation (1);
  attachInterrupt(digitalPinToInterrupt(BTN_PIN), btn_read, RISING); //подключение обработчика прерывания нажатия кнопки
  Serial.println(F("Инициализировано"));
  //заливка экрана черным и печать различной отладочной информации
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(0,0);
  Serial.println("Экран инициализирован");
  
  //Таймеры
  refsens.setInterval(sync_interval); //Интервал обновления экрана датчиков
  refscreen.setInterval(1000); //интервал обновления экрана
  refweather.setInterval(300000);//интервал обновления погоды 5 минут
  //Device init
  Serial.begin(115200);
  Serial.println("Серийный запущен 115200");
  WiFi.begin(ssid, password);
  Serial.println("Подключение");
  tft.println("Подключение...");
  int count = 0;
  while(WiFi.status() != WL_CONNECTED && count < 10) {
    delay(500);
    Serial.print(".");
    count++;
  }
  if (WiFi.status() != WL_CONNECTED) {
  //здесь идет запуск WifiManager. Если ssid был явно не указан в коде программы, то автоматом создается точка достпа PCdisplay при подключении к которой по адресу 192.168.4.1 будет доступен экран конфигурации для его настройки
  WiFiManager wifiManager;
  tft.setCursor(0,20);
  tft.println("Не удалось подкючиться");
  tft.setCursor(0,40);
  tft.println("Подключитесь к PCdisplay");
  tft.setCursor(0,60);
  tft.println("IP адрес: 192.168.4.1");
  wifiManager.autoConnect("PCDisplay");
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0,0);
  Serial.print("Подключен к ");
  Serial.println(WiFi.SSID());
  tft.print("Подключен к ");
  tft.println(WiFi.SSID());
  tft.setCursor(0,20);
  Serial.print("IP адрес: ");
  Serial.println(WiFi.localIP());
  tft.print("IP адрес: ");
  tft.println(WiFi.localIP());
  sendRqs();
 if (WiFi.getMode()==WIFI_STA){
 timeClient.begin();
 Serial.println("Синхронизация времени началась");
 
  server.on("/",handle_index_page);
  server.on("/changeserver",handle_changeserver);
  server.on("/", handle_index_page);
  server.on("/reboot", handle_reboot_page);
  Serial.print("Веб-интерфейс запущен");
  FS_init();
  Serial.println("Файловая система запущена");
  ElegantOTA.begin(&server);    // Обновление прошивки через WEB интерфейс по адресу http://ip.ad.re.ss/update Сам IP можно будет посмотреть на экране устройства при запуске
  server.begin();
  Serial.println("OTA сервер запущен");
  Serial.println("HTTP сервер запущен");
  delay(2000);
  tft.fillScreen(TFT_BLACK);
  hardwareMonitor();} else {
  tft.fillScreen(TFT_BLACK);
  tft.setTextWrap(true);
  }
  }
}

void loop() {
   if (WiFi.getMode()==WIFI_STA){
  butt1.tick(); 
  
  timeClient.update(); //обновление времени по NTP
  server.handleClient(); //работа web сервера
  btn_read(); //проверка нажтия кнопки
   if (refsens.isReady()) 
   { //обновление датчиков
     hardwareMonitor();
     }
 if (refscreen.isReady()){
  ScreenDraw();    
 }
 if (refweather.isReady()){
 sendRqs(); //синхронизация погоды
 }
   }
  }
