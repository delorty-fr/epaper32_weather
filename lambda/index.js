import https from 'https';

const OPENW_API_KEY     = "";
const CITY              = "Boston, US";
const WEEK_DAYS_SHORT   = ['Sun','Mon','Tue','Wed','Thu','Fri','Sat'];
const WEEK_DAYS_LONG    = ['Sunday','Monday','Tuesday','Wednesday','Thursday','Friday','Saturday'];
const MONTHS_SHORT      = ['Jan','Feb','Mar','Apr','May','June','Jul','Aug','Sept','Oct','Nov','Dec'];

const CURRENT_WEATHER_URL  = `https://api.openweathermap.org/data/2.5/weather?q=${CITY}&appid=${OPENW_API_KEY}&units=imperial `;
const FORECAST_WEATHER_URL = `https://api.openweathermap.org/data/2.5/forecast?q=${CITY}&appid=${OPENW_API_KEY}&units=imperial `;

const toESTDate = (unix_dt, tz_offset) => new Date((parseInt(unix_dt) + parseInt(tz_offset)) * 1000);
   
const currentWeather = () => {
    return new Promise((resolve, reject) => {
        https.get(CURRENT_WEATHER_URL, (res) => {
          let data = "";
          res.on("data", (chunk) => {
            data += chunk;
          });
         
          res.on("end", () => {
            const openw = JSON.parse(data);
            
            const result = {};
            
            const sunrise   = toESTDate(openw.sys.sunrise, openw.timezone);
            const sunset    = toESTDate(openw.sys.sunset, openw.timezone);
            
            console.log('====')
            console.log(sunrise.getHours() + ':' + sunrise.getMinutes())
            console.log(sunset.getHours() + ':' + sunset.getMinutes())
            console.log('====')
            
            const date      = toESTDate(openw.dt, openw.timezone);
            result.date     = MONTHS_SHORT[date.getMonth()] + ' ' + date.getDate();
            result.wday     = WEEK_DAYS_LONG[date.getDay()];
            result.icon     = openw.weather[0].icon;
            result.temp     = Math.round(openw.main.temp);
            result.minTemp  = Math.round(openw.main.temp_min);
            result.maxTemp  = Math.round(openw.main.temp_max);
            result.flTemp   = Math.round(openw.main.feels_like);
            result.rain     = openw.rain != null;
            result.snow     = openw.snow != null;
            result.sunrise  = sunrise.getHours() + ':' + sunrise.getMinutes();
            result.sunset   = sunset.getHours() + ':' + sunset.getMinutes();
            resolve(result);
          });
        }).on("error", (error) => {
          reject(error);
        });
    });
};


const forecastWeather = () => {
    return new Promise((resolve, reject) => {
        https.get(FORECAST_WEATHER_URL, (res) => { 
          let data = "";
          res.on("data", (chunk) => {
            data += chunk;
          });
          
          res.on("end", () => {
            const openw_forecast = JSON.parse(data);
            const tz_offset = openw_forecast.city.timezone;
            const daysForecast = openw_forecast.list.reduce((acc, f) => {
                const fdate = toESTDate(f.dt, tz_offset);
                const day = WEEK_DAYS_SHORT[fdate.getDay()] + ' ' + fdate.getDate(); 
                if (!acc[day]) {
                  acc[day] = [];
                }
                acc[day].push(f);
                return acc;
            }, {});
            
            let result = {};
           
            // --- NEXT 6 FORECASTS (HOURS) ------------------------------------
            let today_weather = [];
            let lastRain;
            let lastSnow;
            openw_forecast.list.slice(0, 6).forEach(df => {
              let hourly_forecast = {};
              
              const date = toESTDate(df.dt, tz_offset);
              const h_date = date.getHours();
              const m_date = date.getMinutes();
              const h_time = h_date < 10 ? '0' + h_date : '' + h_date;
              const m_time = m_date < 10 ? '0' + m_date : '' + m_date;
              hourly_forecast.time = h_time + ':' + m_time;
              
              hourly_forecast.flTemp  = Math.round(df.main.feels_like);
              hourly_forecast.minTemp = Math.round(df.main.temp_min);
              hourly_forecast.maxTemp = Math.round(df.main.temp_max);
              hourly_forecast.icon    = df.weather[0].icon;
              
              hourly_forecast.rain    = df.rain != null;
              hourly_forecast.snow    = df.snow != null;
              hourly_forecast.pop     = df.pop * 100;
              
              // extend the last rain/snow value to the next ones if pop is > 0 
              // but rain and snow not defined
              
              if(df.pop == null || df.pop <= 0.0) { //should not be < 0, but in case
                lastRain = null;
                lastSnow = null;
              } else {
                if(df.rain != null) lastRain = hourly_forecast.rain;
                if(df.snow != null) lastSnow = hourly_forecast.snow;
                if(df.rain == null && lastRain != null) hourly_forecast.rain = lastRain;
                if(df.snow == null && lastSnow != null) hourly_forecast.snow = lastSnow;
              }
                            
              hourly_forecast.isDay   = df.sys.pod === 'd';
              today_weather.push(hourly_forecast);
            });
            result.today = today_weather;
           
            // --- NEXT DAYS ---------------------------------------------------
           
            let next_days = [];
            Object.entries(daysForecast).slice(1, 4).forEach(([day, df]) => {
              let day_weather = {};
              day_weather.day = day;
              
              let ico = {};
              let ico_max;
              let ico_max_ref;
              
              df.forEach(hf => {
                if(day_weather.minTemp == null || hf.main.temp_min < day_weather.minTemp) day_weather.minTemp = Math.round(hf.main.temp_min);
                if(day_weather.maxTemp == null || hf.main.temp_max > day_weather.maxTemp) day_weather.maxTemp = Math.round(hf.main.temp_max);
                if(day_weather.minFlTemp == null || hf.main.feels_like < day_weather.minFlTemp) day_weather.minFlTemp = Math.round(hf.main.feels_like);
                if(day_weather.maxFlTemp == null || hf.main.feels_like >day_weather. maxFlTemp)day_weather. maxFlTemp = Math.round(hf.main.feels_like);
                
                if(day_weather.maxPop == null || hf.pop > day_weather.maxPop) day_weather.maxPop = Math.round(hf.pop * 100);
                if(day_weather.rain != true) day_weather.rain = hf.rain != null;
                if(day_weather.snow != true) day_weather.snow = hf.snow != null;
                 
                if (!ico[hf.weather[0].icon]) {
                  ico[hf.weather[0].icon] = 1;
                } else {
                  ico[hf.weather[0].icon] = ico[hf.weather[0].icon] + 1;
                }
                
                if(ico_max == null || ico[hf.weather[0].icon] > ico_max) {
                  ico_max_ref = hf.weather[0].icon;
                  ico_max = ico[hf.weather[0].icon];
                }
                
              });
              day_weather.icon = ico_max_ref;
               
              next_days.push(day_weather);
              
            });
           
            result.nexDays = next_days;
            
            result.units = {};
            result.units.temp = "F";
            
            resolve(result);
            
          });
        }).on("error", (error) => {
          reject(error);
        });
    });
};

export const handler = (event) => {
    return new Promise((resolve, reject) => {
     Promise.all([currentWeather(), forecastWeather()]).then((results) => { 
       const statusCode = '200';
       const body = JSON.stringify({'today': results[0], 'forecast': results[1]});
       const headers =  {'Content-Type': 'application/json'};
       const response = { statusCode, body, headers };
       resolve(response);
     }
   )});
};
