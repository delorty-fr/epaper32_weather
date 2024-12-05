#ifndef COMMON_H_
#define COMMON_H_

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "forecast_record.h"
//#include "common_functions.h"

//#########################################################################################
bool DecodeCurrentWeather(WiFiClient& json) {
  Serial.print(F("\nCreating object...and "));
  DynamicJsonDocument doc(35 * 1024);
  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return false;
  }
  JsonObject root = doc.as<JsonObject>();

  today_wx[0].date    = root["today"]["date"].as<const char*>();
  today_wx[0].wday    = root["today"]["wday"].as<const char*>();
  today_wx[0].icon    = root["today"]["icon"].as<const char*>();
  today_wx[0].temp    = root["today"]["temp"].as<int>();
  today_wx[0].minTemp = root["today"]["minTemp"].as<int>();
  today_wx[0].maxTemp = root["today"]["maxTemp"].as<int>();
  today_wx[0].flTemp  = root["today"]["flTemp"].as<int>();
  today_wx[0].rain    = root["today"]["rain"].as<bool>();
  today_wx[0].snow    = root["today"]["snow"].as<bool>();
  today_wx[0].sunrise = root["today"]["sunrise"].as<const char*>();
  today_wx[0].sunset  = root["today"]["sunset"].as<const char*>();

  JsonArray today_forecast_data = root["forecast"]["today"];
  for (byte i = 0; i < TODAY_FORECAST_MAX_READING; i++) {
    today_forecast[i].time    = today_forecast_data[i]["time"].as<const char*>();
    today_forecast[i].temp    = today_forecast_data[i]["temp"].as<int>();
    today_forecast[i].flTemp  = today_forecast_data[i]["flTemp"].as<int>();
    today_forecast[i].minTemp = today_forecast_data[i]["minTemp"].as<int>();
    today_forecast[i].maxTemp = today_forecast_data[i]["maxTemp"].as<int>();
    today_forecast[i].icon    = today_forecast_data[i]["icon"].as<const char*>();
    today_forecast[i].rain    = today_forecast_data[i]["rain"].as<bool>();
    today_forecast[i].snow    = today_forecast_data[i]["snow"].as<bool>();
    today_forecast[i].pop     = today_forecast_data[i]["pop"].as<int>();
    today_forecast[i].isDay   = today_forecast_data[i]["isDay"].as<bool>();
  }

  JsonArray nextdays_forecast_data = root["forecast"]["nexDays"];
  for (byte i = 0; i < NDAYS_FORECAST_MAX_READING; i++) {
    ndays_forecast[i].day       = nextdays_forecast_data[i]["day"].as<const char*>();
    ndays_forecast[i].minTemp   = nextdays_forecast_data[i]["minTemp"].as<int>();
    ndays_forecast[i].maxTemp   = nextdays_forecast_data[i]["maxTemp"].as<int>();
    ndays_forecast[i].minFlTemp = nextdays_forecast_data[i]["minFlTemp"].as<int>();
    ndays_forecast[i].maxFlTemp = nextdays_forecast_data[i]["maxFlTemp"].as<int>();
    ndays_forecast[i].maxPop    = nextdays_forecast_data[i]["maxPop"].as<int>();
    ndays_forecast[i].rain      = nextdays_forecast_data[i]["rain"].as<bool>();
    ndays_forecast[i].snow      = nextdays_forecast_data[i]["snow"].as<bool>();
    ndays_forecast[i].icon      = nextdays_forecast_data[i]["icon"].as<const char*>();
  }
  
  return true;
}

//#########################################################################################

// RAII wrapper for HTTPClient
class HTTPClientWrapper {
  private:
    HTTPClient& _http;
  public:
    HTTPClientWrapper(HTTPClient& http) : _http(http) {};
    ~HTTPClientWrapper() {
      _http.end();  
    };
};

const String LAMBDA_API_KEY      = "";
const String LAMBDA_WEATHER_URL  = "";
bool obtain_wx_data(WiFiClient& client) {
  Serial.print(F("Get Weather data..."));
  client.stop(); // close connection before sending a new request
  HTTPClient http;
  HTTPClientWrapper wrapper(http);
   
  http.begin(LAMBDA_WEATHER_URL);
  http.addHeader("X-Api-Key", LAMBDA_API_KEY);
  http.addHeader("Accept", "application/json");
  http.addHeader("Content-Type", "application/json");
  
  int httpCode = http.GET();
  if(httpCode == HTTP_CODE_OK) {
    if (!DecodeCurrentWeather(http.getStream())) return false;
    client.stop();
    http.end();
    return true;
  }
  else
  {
    Serial.printf("connection failed, error: %s", http.errorToString(httpCode).c_str());
    client.stop();
    http.end();
    return false;
  }
  http.end();
  return true;
}
#endif /* ifndef COMMON_H_ */
