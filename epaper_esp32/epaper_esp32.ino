#define DRAW_GRID 0
#define DEBUG 1 //0 off, 1 on

#include <ArduinoJson.h>
#include <Arduino.h>
#include <WiFi.h>
#include "time.h"
#include <SPI.h>
#define  ENABLE_GxEPD2_display 0
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <driver/adc.h>
#include "esp_adc_cal.h"
#include "wx_icons.h"
#include "forecast_record.h"
#include <TinyPICO.h>

TinyPICO tp = TinyPICO();

//### DIM #############################################################################

// Set for landscape mode, don't remove the decimal place!
#define SCREEN_WIDTH  480.0    
#define SCREEN_HEIGHT 280.0

static const uint8_t DIM_FORECAST_SECTION_W = 80;

static const uint8_t GAP    = 5;
static const uint8_t GAP_S  = 2;

enum alignment {LEFT, RIGHT, CENTER};

//### WIFI  ###########################################################################

const char* ssid     = "";
const char* password = "";

//### EPAPER  #########################################################################

// Connections for Adafruit ESP32 Feather - https://learn.adafruit.com/assets/111179
//static const uint8_t EPD_BUSY = 32; // to EPD BUSY
//static const uint8_t EPD_CS   = 15; // to EPD CS
//static const uint8_t EPD_RST  = 27; // to EPD RST
//static const uint8_t EPD_DC   = 33; // to EPD DC
//static const uint8_t EPD_SCK  = 5;  // to EPD CLK
//static const uint8_t EPD_MISO = 19; // Master-In Slave-Out not used, as no data from display
//static const uint8_t EPD_MOSI = 18; // to EPD DIN

// Connections for TinyPICO ESP32 - https://www.tinypico.com/
static const uint8_t EPD_BUSY = 4;  // to EPD BUSY
static const uint8_t EPD_CS   = 14; // to EPD CS
static const uint8_t EPD_RST  = 15; // to EPD RST
static const uint8_t EPD_DC   = 27; // to EPD DC
static const uint8_t EPD_SCK  = 18; // to EPD CLK
static const uint8_t EPD_MISO = 19; // Master-In Slave-Out not used, as no data from display
static const uint8_t EPD_MOSI = 23; // to EPD DIN

GxEPD2_BW<GxEPD2_370_TC1, GxEPD2_370_TC1::HEIGHT> display(GxEPD2_370_TC1(/*CS=D8*/ EPD_CS, /*DC=D3*/ EPD_DC, /*RST=D4*/ EPD_RST, /*BUSY=D2*/ EPD_BUSY));

//### FONTS #############################################################################

// Select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts; 

// Using fonts:
// u8g2_font_helvB08_tf
// u8g2_font_helvB10_tf
// u8g2_font_helvB12_tf
// u8g2_font_helvB14_tf
// u8g2_font_helvB18_tf
// u8g2_font_helvB24_tf

//### WEATHER DATA #######################################################################

#define TODAY_FORECAST_MAX_READING 6
#define NDAYS_FORECAST_MAX_READING 3

Today_record_type       today_wx[1];
Today_forecast_type     today_forecast[TODAY_FORECAST_MAX_READING];
Nextdays_forecast_type  ndays_forecast[NDAYS_FORECAST_MAX_READING];

#include "common.h"

//### DEEP SLEEP #########################################################################

long SleepDuration = 30; // Sleep time in minutes
long StartTime     = 0;

//#########################################################################################
void setup() {
  if( DEBUG ) {
    Serial.begin(115200);
    Serial.println("Debug enabled");
  } 

  tp.DotStar_SetPixelColor( 0, 0, 255 );
  
  StartTime = millis();
  if (StartWiFi() == WL_CONNECTED) {
    if (DEBUG) Serial.println("Wifi connected");
    InitialiseDisplay(); // Give screen time to initialise by getting weather data
    
    byte Attempts = 1;
    bool RxWeather = false;
    WiFiClient client;   // wifi client object
    while (RxWeather == false && Attempts <= 3) {
      if (RxWeather  == false) RxWeather = obtain_wx_data(client);
      Attempts++;
    }
    StopWiFi();
    
    if (RxWeather) {
      if (DEBUG) Serial.println("Got weather");
      DisplayWeather();
      tp.DotStar_SetPixelColor( 0, 255, 0);
      display.display(false); // Full screen update mode
    } else {
      if (DEBUG) Serial.println("Failed to get weather");
      tp.DotStar_SetPixelColor( 255, 0, 0);
    }
  }
  
  BeginSleep();
}

//#########################################################################################
void BeginSleep() {
  if (DEBUG) Serial.println("Go to sleep");

  tp.DotStar_Clear();
  tp.DotStar_SetPower( false );
  
  display.powerOff();

  esp_sleep_enable_timer_wakeup((SleepDuration * 60) * 1000000LL);
  
#ifdef BUILTIN_LED
  pinMode(BUILTIN_LED, INPUT); // If it's On, turn it off and some boards use GPIO-5 for SPI-SS, which remains low after screen use
  digitalWrite(BUILTIN_LED, HIGH);
#endif

  if (DEBUG) Serial.println("Awake for : " + String((millis() - StartTime) / 1000.0, 3) + "-secs");
  if (DEBUG) Serial.println("Starting deep-sleep period...");

  esp_deep_sleep_start();
}

//#########################################################################################
void DisplayWeather() {
  if (DEBUG) Serial.println("Draw weather");
#if DRAW_GRID
  Draw_Grid();
#endif

  const int wx_x = GAP;
  const int wx_y = GAP;
  
  const int days_forecast_left_margin = ICON_XXS_W;

  const int days_forecast_x = SCREEN_WIDTH - (DIM_FORECAST_SECTION_W * NDAYS_FORECAST_MAX_READING) - days_forecast_left_margin;
  const int days_forecast_y = 0;

  const int hours_forecast_x = 0;
  const int hours_forecast_y = 155;

  display.drawRect(0, 0, days_forecast_x - 1, hours_forecast_y - 1, GxEPD_BLACK);
  display.drawRect(days_forecast_x, 0, SCREEN_WIDTH - days_forecast_x, hours_forecast_y - 1, GxEPD_BLACK);
  display.drawRect(0, hours_forecast_y, SCREEN_WIDTH, SCREEN_HEIGHT - hours_forecast_y, GxEPD_BLACK);

  DrawMainWeatherSection(wx_x, wx_y);                           // current weather section
  DrawDaysForecastSection(days_forecast_x, days_forecast_y);    // Days forecast
  DrawHoursForecastSection(hours_forecast_x, hours_forecast_y); // Hours forecast
}
//#########################################################################################
// Help debug screen layout by drawing a grid of little crosses
void Draw_Grid() {
  int x, y;
  const int grid_step = 10;

  //Draw the screen border so we know how far we can push things out
  display.drawLine(0, 0, SCREEN_WIDTH-1, 0, GxEPD_BLACK);  //across top
  display.drawLine(0, SCREEN_HEIGHT-1, SCREEN_WIDTH-1, SCREEN_HEIGHT-1, GxEPD_BLACK); //across bottom
  display.drawLine(0, 0, 0, SCREEN_HEIGHT-1, GxEPD_BLACK);  //lhs
  display.drawLine(SCREEN_WIDTH-1, 0, SCREEN_WIDTH-1, SCREEN_HEIGHT-1, GxEPD_BLACK);  //rhs

  for( x=grid_step; x<SCREEN_WIDTH; x+=grid_step ) {
    for( y=grid_step; y<SCREEN_HEIGHT; y+=grid_step ) {
      display.drawLine(x-1, y, x+1, y, GxEPD_BLACK);  //Horizontal line
      display.drawLine(x, y-1, x, y+1, GxEPD_BLACK);  //Vertical line
    }
  }
}
//#########################################################################################
void DrawMainWeatherSection(int x, int y) {

  int ref_x = x + 2*GAP;
  int ref_y = y + 2*GAP;

  String date = today_wx[0].date;
  String wday = today_wx[0].wday;
  String icon_ref = today_wx[0].icon;
  
  const unsigned char* icon;
  if     (icon_ref == "01d") icon = WX_ICON_SUN_L;
  else if(icon_ref == "01n") icon = WX_ICON_MOON_L;
  else if(icon_ref == "02d") icon = WX_ICON_CLOUDY_D_L;
  else if(icon_ref == "02n") icon = WX_ICON_CLOUDY_N_L;
  else if(icon_ref == "03d" || icon_ref == "03n") icon = WX_ICON_CLOUD_L;
  else if(icon_ref == "04d" || icon_ref == "04n") icon = WX_ICON_CLOUDS_L;
  else if(icon_ref == "09d" || icon_ref == "09n") icon = WX_ICON_SHOWER_RAIN_L;
  else if(icon_ref == "10d") icon = WX_ICON_RAIN_D_L;
  else if(icon_ref == "10n") icon = WX_ICON_RAIN_N_L;
  else if(icon_ref == "13d" || icon_ref == "13n") icon = WX_ICON_SNOW_L;
  else icon = WX_ICON_UNK_L;

  display.drawBitmap(ref_x, ref_y + GAP, icon, ICON_L_W, ICON_L_H, GxEPD_BLACK);

  DrawTodayWx(x + 3*GAP, ref_y + ICON_L_H + (GAP*4));
  
  ref_x += ICON_L_W + (GAP*4);
  ref_y += GAP;
  
  u8g2Fonts.setFont(u8g2_font_helvB24_tf);
  const int font_h = u8g2Fonts.getFontAscent();
  drawString(ref_x, ref_y, date, LEFT);

  ref_y += font_h + (GAP*2);
  
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(ref_x, ref_y, wday, LEFT);

}

//#########################################################################################
void DrawHoursForecastSection(int x, int y) {
  int size = sizeof(today_forecast) / sizeof(today_forecast[0]);
  for (int index = 0; index < size; index++) {
    String time     = today_forecast[index].time;
    String temp     = String(today_forecast[index].minTemp) + "°";
    bool rain       = today_forecast[index].rain;
    bool snow       = today_forecast[index].snow;
    int pop         = today_forecast[index].pop;
    String icon_ref = today_forecast[index].icon;
    DrawForecast(x + (DIM_FORECAST_SECTION_W*index), y, index, time, temp, NULL, rain, snow, pop, icon_ref, false, DIM_FORECAST_SECTION_W, 3*GAP);
  }
}

void DrawDaysForecastSection(int x, int y) {
  int size = sizeof(ndays_forecast) / sizeof(ndays_forecast[0]);
  for (int index  = 0; index < size; index++) {
    String time     = ndays_forecast[index].day;
    String temp     = String(ndays_forecast[index].minTemp) + "/" + String(ndays_forecast[index].maxTemp) + "°";
    String flTemp   = String(ndays_forecast[index].minFlTemp) + "/" + String(ndays_forecast[index].maxFlTemp) + "°";
    bool rain       = ndays_forecast[index].rain;
    bool snow       = ndays_forecast[index].snow;
    int pop         = ndays_forecast[index].maxPop;
    String icon_ref = ndays_forecast[index].icon;
    DrawForecast(x + (DIM_FORECAST_SECTION_W*index), y, index, time, temp, &flTemp, rain, snow, pop, icon_ref, true, DIM_FORECAST_SECTION_W, 3*GAP);
  }
}

//#########################################################################################
void DrawForecast(int x, int y, int index, String time, String temp, String *flTemp, bool rain, bool snow, int pop, String icon_ref, boolean shiftLegendIcons, int forecastDim, int valuesTopMargin) {

  const int DIM_FORECAST_SECTION_W_MID = forecastDim / 2;
  const int ICON_LEGEND_LEFT_MARGIN  = 2;
  const int ICON_LEGEND_TOP_MARGIN   = 1;
  
  int ref_x = x;
  int ref_y = y;

  int icon_ref_x = x + GAP + ICON_LEGEND_LEFT_MARGIN;

  if(shiftLegendIcons) {
    ref_x += ICON_XXS_W;
  }

  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  uint8_t font_h = u8g2Fonts.getFontAscent();

  drawString(ref_x + DIM_FORECAST_SECTION_W_MID, ref_y, time, CENTER);
  
  ref_y += font_h + GAP;
  
  display.drawLine(x, ref_y, ref_x + forecastDim, ref_y, GxEPD_BLACK);
  
  ref_y += GAP;

  const unsigned char* icon;
  if     (icon_ref == "01d") icon = WX_ICON_SUN_M;
  else if(icon_ref == "01n") icon = WX_ICON_MOON_M;
  else if(icon_ref == "02d") icon = WX_ICON_CLOUDY_D_M;
  else if(icon_ref == "02n") icon = WX_ICON_CLOUDY_N_M;
  else if(icon_ref == "03d" || icon_ref == "03n") icon = WX_ICON_CLOUD_M;
  else if(icon_ref == "04d" || icon_ref == "04n") icon = WX_ICON_CLOUDS_M;
  else if(icon_ref == "09d" || icon_ref == "09n") icon = WX_ICON_SHOWER_RAIN_M;
  else if(icon_ref == "10d") icon = WX_ICON_RAIN_D_M;
  else if(icon_ref == "10n") icon = WX_ICON_RAIN_N_M;
  else if(icon_ref == "13d" || icon_ref == "13n") icon = WX_ICON_SNOW_M;
  else icon = WX_ICON_UNK_M;
  
  display.drawBitmap(ref_x + DIM_FORECAST_SECTION_W_MID - (ICON_M_W/2), ref_y, icon, ICON_M_W, ICON_M_H, GxEPD_BLACK);
  
  const int rain_snow_x = ref_x + ICON_LEGEND_LEFT_MARGIN;
  const int rain_snow_y = ref_y + GAP_S;
  if(rain || (!rain && !snow && pop > 0)) display.drawBitmap(rain_snow_x, ref_y + GAP_S, RAIN_ICON_XXS, ICON_XXS_W, ICON_XXS_H, GxEPD_BLACK);
  if(snow) display.drawBitmap(rain_snow_x, rain_snow_y + ICON_XXS_H, SNOW_ICON_XXS, ICON_XXS_W, ICON_XXS_H, GxEPD_BLACK);
  
  ref_y += ICON_M_H + GAP;

  if(index == 0) {
    display.drawBitmap(x + ICON_LEGEND_LEFT_MARGIN, ref_y + ICON_LEGEND_TOP_MARGIN, TEMP_ICON_XS, ICON_XS_W, ICON_XS_H, GxEPD_BLACK);
  }

  drawString(ref_x + DIM_FORECAST_SECTION_W_MID, ref_y, temp, CENTER);

  if(flTemp != NULL) {
    ref_y += font_h + valuesTopMargin;
    if(index == 0) {
      display.drawBitmap(x + ICON_LEGEND_LEFT_MARGIN, ref_y + ICON_LEGEND_TOP_MARGIN, FEELS_LIKE_ICON_XS, ICON_XS_W, ICON_XS_H, GxEPD_BLACK);
    }
    drawString(ref_x + DIM_FORECAST_SECTION_W_MID, ref_y, *flTemp, CENTER);
  } 

  ref_y += font_h + valuesTopMargin;
  
  // POP
  
  if(index == 0) {
    display.drawBitmap(x + ICON_LEGEND_LEFT_MARGIN, ref_y + ICON_LEGEND_TOP_MARGIN, POP_ICON_XS, ICON_XS_W, ICON_XS_H, GxEPD_BLACK);
  }

  if(pop > 0) {
    drawString(ref_x + DIM_FORECAST_SECTION_W_MID, ref_y, String(pop) + "%", CENTER);
  }
  
}

//#########################################################################################
void DrawTodayWx(int x, int y) {
  
  int ref_x = x;
  int ref_y = y;

  display.drawBitmap(ref_x, ref_y, TEMP_ICON_M, ICON_M_W, ICON_M_H, GxEPD_BLACK);

  ref_x += ICON_M_W;
  
  String temps = String(today_wx[0].temp) + "°";
  u8g2Fonts.setFont(u8g2_font_helvB24_tf);
  drawString(ref_x, ref_y - GAP, temps, LEFT);

  // Save coord for later
  const int ref_x_save = ref_x + u8g2Fonts.getUTF8Width(temps.c_str()) + 2*GAP;
  const int ref_y_save = ref_y;

  ref_y += u8g2Fonts.getFontAscent();
  u8g2Fonts.setFont(u8g2_font_helvB24_tf );
  ref_x += u8g2Fonts.getUTF8Width(temps.c_str());
  
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  String temp_unit = "F";
  drawString(ref_x - u8g2Fonts.getUTF8Width(temp_unit.c_str()), ref_y - u8g2Fonts.getFontAscent() - GAP_S, temp_unit, LEFT);

  ref_x += 4*GAP;
  ref_y = ref_y_save;

  display.drawBitmap(ref_x, ref_y, FEELS_LIKE_ICON_M, ICON_M_W, ICON_M_H, GxEPD_BLACK);
  
  ref_x += ICON_M_W;
  
  u8g2Fonts.setFont(u8g2_font_helvB24_tf);
 
  String flTemp = String(today_wx[0].flTemp) + "°";
  drawString(ref_x, ref_y - GAP, flTemp, LEFT);
  
  ref_x += u8g2Fonts.getUTF8Width(flTemp.c_str());
  ref_y += u8g2Fonts.getFontAscent();  
  
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(ref_x - u8g2Fonts.getUTF8Width(temp_unit.c_str()), ref_y - u8g2Fonts.getFontAscent() - GAP_S, temp_unit, LEFT);

  ref_x = x + GAP;
  ref_y += 2*GAP;

}

//#########################################################################################
void DrawSunriseSunset(int x, int y) {

  int ref_x = x;
  int ref_y = y;

  display.drawBitmap(ref_x, ref_y, SUNRISE_ICON_S, ICON_S_W, ICON_S_H, GxEPD_BLACK);

  ref_x += ICON_S_W + GAP_S;
  
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  
  String sun_str = today_wx[0].sunrise;
  drawString(ref_x, ref_y, sun_str, LEFT);

  ref_x += u8g2Fonts.getUTF8Width(sun_str.c_str()) + 2*GAP;
  
  display.drawBitmap(ref_x, ref_y, SUNSET_ICON_S, ICON_S_W, ICON_S_H, GxEPD_BLACK);

  ref_x += ICON_S_W + GAP_S;
  
  sun_str = today_wx[0].sunset;
  drawString(ref_x, ref_y, sun_str, LEFT);
}

//#########################################################################################
uint8_t StartWiFi() {
  if (DEBUG) Serial.println("Connecting to: " + String(ssid));
  IPAddress dns(8, 8, 8, 8); // Google DNS
  WiFi.disconnect();
  WiFi.mode(WIFI_STA); // switch off AP
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  unsigned long start = millis();
  uint8_t connectionStatus;
  bool AttemptConnection = true;
  while (AttemptConnection) {
    connectionStatus = WiFi.status();
    if (millis() > start + 15000) { // Wait 15-secs maximum
      AttemptConnection = false;
    }
    if (connectionStatus == WL_CONNECTED || connectionStatus == WL_CONNECT_FAILED) {
      AttemptConnection = false;
    }
    delay(50);
  }
  if (connectionStatus == WL_CONNECTED) {
    if (DEBUG) Serial.println("WiFi connected at: " + WiFi.localIP().toString());
  }
  else log_e("WiFi connection *** FAILED ***");
  return connectionStatus;
}

//#########################################################################################
void StopWiFi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
}

//#########################################################################################
// y - the top pixel of the string (font)
// text - the string to print/display
// x - depends on the align argument:
//  align:
//    - LEFT : x is the lhs starting point of the string
//    - CENTER : x is the dead centre of the string
//    - RIGHT: x is the rhs pixel of the string
void drawString(int x, int y, String text, alignment align) {
  uint16_t w, h;
  display.setTextWrap(false);
  w = u8g2Fonts.getUTF8Width(text.c_str());
  h = u8g2Fonts.getFontAscent() + abs(u8g2Fonts.getFontDescent());

  if (align == RIGHT)  x = x - w;
  if (align == CENTER) x = x - w / 2;
  u8g2Fonts.setCursor(x, y + h);
  u8g2Fonts.print(text);
}

//#########################################################################################
void drawStringMaxWidth(int x, int y, int text_width, String text, alignment align) {
  int max_lines = 2;
  int lines_done = 0;
  int first_char = 0;
  int last_char = text.length()+1;  //+1 as in substring 'to' is exclusive
  int stringwidth, charheight, descender;
  int printat_x;

  if (DEBUG) Serial.println("drawStringMaxWidth([" + text + "])");

  descender = abs(u8g2Fonts.getFontDescent());  //abs() as descender tends to be negative
  charheight = u8g2Fonts.getFontAscent();
  // If we have a descender value, add it again for the inter-line gap
  if (descender > 0 ) charheight += descender * 2;
  // otherwise, add a quarter of the char height as the inter-line gap
  else charheight += charheight/4;

  if (DEBUG) Serial.println("charheight:" + String(charheight));

  while( (lines_done < max_lines) && (first_char <= text.length()) && (last_char >= first_char) ) {
    if (DEBUG) Serial.println(" Line:" + String(lines_done) + ". Check if we need to trim the string");
    while( (stringwidth = u8g2Fonts.getUTF8Width(text.substring(first_char, last_char).c_str())) > text_width ) {
      last_char--;
      if (DEBUG) Serial.println("  trim to " + String(last_char));
    }

    if (align == RIGHT)  printat_x = x - stringwidth;
    if (align == CENTER) printat_x = x - (stringwidth / 2);
    if (align == LEFT) printat_x = x;

    if (DEBUG) Serial.println(" Print from " + String(first_char) + " to " + String(last_char) );
    if (DEBUG) Serial.println(" Print [" + text.substring(first_char, last_char) + "]");
    u8g2Fonts.setCursor(printat_x, y + (lines_done * charheight));
    u8g2Fonts.println(text.substring(first_char, last_char));
    first_char = last_char;   //last_char in substring is exclusive, so no '+1' on it.
    last_char = text.length();
    lines_done++;
  }

  if (lines_done >= max_lines) if (DEBUG) Serial.println("Stopped at line limit");
  if (first_char >= text.length() ) if (DEBUG) Serial.println("Stopped as string all printed");
  if (DEBUG) Serial.println("drawStringMaxWidth() done");
}

//#########################################################################################
void InitialiseDisplay() {
  display.init(115200, true, 2, false);
  SPI.end();
  SPI.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
  display.setRotation(3);
  
  u8g2Fonts.begin(display); // connect u8g2 procedures to Adafruit GFX
  u8g2Fonts.setFontMode(1);                  // use u8g2 transparent mode (this is default)
  u8g2Fonts.setFontDirection(0);             // left to right (this is default)
  u8g2Fonts.setForegroundColor(GxEPD_BLACK); // apply Adafruit GFX color
  u8g2Fonts.setBackgroundColor(GxEPD_WHITE); // apply Adafruit GFX color
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  
  display.fillScreen(GxEPD_WHITE);
  display.setFullWindow();
}

//#########################################################################################
void loop() {
   // this will never run!
}
