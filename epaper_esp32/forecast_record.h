#ifndef FORECAST_RECORD_H_
#define FORECAST_RECORD_H_

#include <Arduino.h>

typedef struct {
  String date;
  String wday;
  String icon;
  int temp;
  int minTemp;
  int maxTemp;
  int flTemp;
  bool rain;
  bool snow;
  String sunrise;
  String sunset;
} Today_record_type;

typedef struct {
  String time;
  int temp;
  int flTemp;
  int minTemp;
  int maxTemp;
  String icon;
  bool rain;
  bool snow;
  int pop;
  bool isDay;
} Today_forecast_type;

typedef struct {
  String day;
  int minTemp;
  int maxTemp;
  int minFlTemp;
  int maxFlTemp;
  int maxPop;
  bool rain;
  bool snow;
  String icon;
} Nextdays_forecast_type;

#endif /* ifndef FORECAST_RECORD_H_ */
