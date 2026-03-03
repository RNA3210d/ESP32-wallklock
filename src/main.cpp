#include <TFT_eSPI.h>
#include <lvgl.h>
#include <XPT2046_Touchscreen.h>
#include "time.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include <SimpleDHT.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "ui.h"
#include <ESPmDNS.h>
#include <WiFiEnterprise.h>

/*
#include "ui_Screen1.h"
#include "ui_Screen2.h"
#include "ui_Screen3.h"
#include "ui_Screen4.h"
*/

// Touchscreen pins
#define XPT2046_IRQ 36  // T_IRQ
#define XPT2046_MOSI 32 // T_DIN
#define XPT2046_MISO 39 // T_OUT
#define XPT2046_CLK 25  // T_CLK
#define XPT2046_CS 33   // T_CS
SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);
int x, y, z;

float lat = 40.28;
float lon = -74.00;
String apiKey = "";
// String url_aqi = "http://api.openweathermap.org/data/2.5/air_pollution?lat=" + String(lat, 4) + "&lon=" + String(lon, 4) + "&appid=" + apiKey;

float humiditydht = 0.0, temperaturedht = 0.0;
SimpleDHT22 dht22(4);

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 480
#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 7 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

const char *ssid = "iiscwlan";                // Your enterprise WiFi name
const char *username = ""; // Your username
const char *password = "";         // Your password

int ti = 0;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "in.pool.ntp.org", 19800, 40000);
HTTPClient http;
const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
int currentYr = 1970;

const char *apiKey_lfm = "";
const char *lastfmUser = "";

int hr = 0;
int mm = 0;
int ss = 0;
int ms = 0;
int chro = 0;
int curr = 0;
int elapsed = 0;

unsigned long previousMillis = 0;
const long interval = 12900; // AQI, Last.FM

unsigned long previousMillis1 = 0;
const long interval1 = 3400; // DHT22

unsigned long previousMillis2 = 0;
const long interval2 = 900; // CLK

unsigned long previousMillis3 = 0;
const long interval3 = 65000;

unsigned long previousMillis4 = 0;
const long interval4 = 900;

unsigned long previousMillis5 = 0;
const long interval5 = 65000;

String tr, ar;

void initWiFi()
{
  Serial.println("Connecting to WPA2 Enterprise...");

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);          // Important for enterprise stability
  WiFi.disconnect(true);
  delay(1000);

  WiFiEnterprise.begin(ssid, username, password);

  unsigned long startAttempt = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 20000)
  {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\n WiFi Connected!");
    Serial.println("IP Address: " + WiFi.localIP().toString());

    timeClient.begin();
    timeClient.forceUpdate();    // better than update()

    lv_scr_load(ui_Screen1);
  }
  else
  {
    Serial.println("\nEnterprise connection failed");
  }
}

void startchrono(lv_event_t *e)
{
  chro = 2;
  ms = 0;
  mm = 0;
  ss = 0;
  hr = 0;
}
void pschrono(lv_event_t *e)
{
  chro = 0;
}

void fetchNowPlaying()
{

  String url = "http://ws.audioscrobbler.com/2.0/?method=user.getrecenttracks&user=" + String(lastfmUser) + "&api_key=" + String(apiKey_lfm) + "&format=json&limit=1";

  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == 200)
  {
    String payload = http.getString();
    // Allocate a buffer
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
      Serial.println("JSON parse error!");
      return;
    }

    JsonObject track = doc["recenttracks"]["track"][0];
    const char *artist = track["artist"]["#text"];
    const char *title = track["name"];
    bool nowPlaying = track["@attr"]["nowplaying"] == "true";
    if (nowPlaying)
    {
      lv_label_set_text_fmt(ui_arti, "%s", artist);
      lv_label_set_text_fmt(ui_track, "%s", title);
    }
    else
    {
      lv_label_set_text_fmt(ui_track, "Welcome to DM");
      lv_label_set_text_fmt(ui_arti, "IISc Banglore");
    }
  }
  else
  {
    Serial.print("HTTP Error: ");
    Serial.println(httpCode);
  }

  http.end();
}

void updateDate()
{
  String dummyTime;
  setTime(timeClient.getEpochTime());
  int currentHour = hour();
  int currentMinute = minute();
  char timeStr[6];
  currentYr = year();
  if(currentYr != 1970)
  {
  sprintf(timeStr, "%02d:%02d", currentHour, currentMinute);
  lv_label_set_text_fmt(ui_time, "%s", timeStr);
  lv_label_set_text_fmt(ui_time1, "%s", timeStr);

  int currentDay = weekday();
  int currentMonth = month();
  
  int currday = day();
  String formattedDate = String(String(currday) + " " + months[currentMonth - 1]) + " " + String(currentYr);
  String weekDays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  String week = weekDays[currentDay - 1];
  lv_label_set_text_fmt(ui_date, "%s", formattedDate);
  lv_label_set_text_fmt(ui_week, "%s", week);
  }
  // lv_label_set_text_fmt(ui_date1, "Date: %s", formattedDate);
}

void ambi()
{
  int err = SimpleDHTErrSuccess;
  if ((err = dht22.read2(&temperaturedht, &humiditydht, NULL)) != SimpleDHTErrSuccess) {
    Serial.print("Read DHT22 failed, err="); Serial.print(SimpleDHTErrCode(err));
    Serial.print(","); Serial.println(SimpleDHTErrDuration(err)); delay(2000);
    return;
  }
  Serial.println(humiditydht);
  Serial.println(temperaturedht);
  lv_label_set_text_fmt(ui_tmpa, "%.1f °C", temperaturedht);
  lv_label_set_text_fmt(ui_rha, "%.1f %%", humiditydht);
  lv_bar_set_value(ui_Bar5, temperaturedht, LV_ANIM_ON);
  lv_bar_set_value(ui_Bar3, humiditydht, LV_ANIM_ON);
  ti++;
  int hd = int(humiditydht);
}
////////////////////////////////////////////////////////////////

void loop()
{
  lv_task_handler(); // let the GUI do its work
  lv_tick_inc(4);    // tell LVGL how much time has passed
  delay(4);
  if (WiFi.status() != WL_CONNECTED)
  {
    initWiFi();
  }
  if (millis() - previousMillis2 >= interval2)
  {
    previousMillis2 = millis();

    updateDate();
  }
  if (lv_scr_act() == ui_Screen1)
  {
    if (millis() - previousMillis >= interval)
    {
      previousMillis = millis();
      if (WiFiEnterprise.isConnected() && currentYr != 1970)
      {
        Serial.println("Still connected to enterprise network");
        Serial.print("IP: ");
        Serial.println(WiFiEnterprise.localIP());
      }
      else
      {
        initWiFi();
        timeClient.update();
        Serial.println(" Attempting NTP resync");
      }
      fetchNowPlaying();
      // getAQIData();
    }
    if (millis() - previousMillis1 >= interval1)
    {
      previousMillis1 = millis();
      updateDate();
      ambi();
    }
  }
  if (lv_scr_act() == ui_Screen3)
  {
    if (chro == 1 || chro == 2)
    {
      if (chro == 2)
      {
        curr = millis();
        chro = 1;
      }
      elapsed = millis() - curr;

      int hh = (elapsed / 3600000) % 24;
      int mm = (elapsed / 60000) % 60;
      int ss = (elapsed / 1000) % 60;
      // int ms = elapsed % 1000;

      lv_label_set_text_fmt(ui_Label8, "%02d : %02d : %02d", hh, mm, ss);
    }
  }
}
///////////////////////////////////////////////////////////////

void setup()
{
  Serial.begin(9600);
  lv_init();
  // touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  // touchscreen.begin(touchscreenSPI);
  // touchscreen.setRotation(0);
  lv_display_t *disp;
  disp = lv_tft_espi_create(SCREEN_WIDTH, SCREEN_HEIGHT, draw_buf, sizeof(draw_buf));
  lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);
  // lv_indev_t *indev = lv_indev_create();
  // lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  //  Set the callback function to read Touchscreen input
  // lv_indev_set_read_cb(indev, touchscreen_read);

  analogWrite(22, 190);
  //dht.setup(4, DHTesp::DHT22);
  ui_init();
  initWiFi();
}
