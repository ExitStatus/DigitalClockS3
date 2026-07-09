#ifndef _WEATHER_API_H
#define _WEATHER_API_H

#include <Arduino.h>

#include "Interval.h"

// Compact per-day forecast: hourly temperature and rain chance kept in small
// arrays (int8/uint8) to save memory, plus the day's computed maxima.
struct DayForecast
{
    bool    valid = false;
    int8_t  maxTemp = 0;         // max hourly temp_c (rounded, deg C)
    uint8_t maxRain = 0;         // max hourly chance_of_rain (%)
    uint8_t maxWind = 0;         // max hourly wind_mph (rounded)
    uint8_t hourCount = 0;
    int8_t   hourTemp[24];       // rounded deg C per hour
    uint8_t  hourRain[24];       // chance_of_rain % per hour
    uint8_t  hourWind[24];       // wind mph per hour
    uint16_t hourPressure[24];   // pressure mb (hPa) per hour
    uint8_t  hourSnow[24];       // chance_of_snow % per hour
    uint8_t  hourGust[24];       // wind gust mph per hour
};

// Fetches the current temperature from WeatherAPI (api.weatherapi.com) for a
// fixed latitude/longitude supplied at construction. On the first Update() after
// WiFi is up it fetches immediately, then the weather every 10 minutes and the
// forecast every 30 minutes. Which temperature field is read from the API's
// "current" object is configurable (e.g. "temp_c" or "feelslike_c"). All network
// calls are blocking.
class WeatherApi
{
    public:
        WeatherApi(const char* apiKey, double lat, double lon, const char* tempField);

        void Begin();
        void Update(bool wifiConnected);

        // False while there has never been a successful fetch, or once the last
        // successful fetch is older than the staleness window (so stale data is
        // not displayed). Becomes true again as soon as a fresh fetch succeeds.
        bool  HasWeather() const  { return _hasWeather && (millis() - _lastWeatherSuccess) < kWeatherStaleMs; }
        float Temperature() const { return _temp; }
        float WindMph() const     { return _windMph; }
        int   WindDegree() const  { return _windDegree; }

        // Raw bytes of the current condition icon PNG (downloaded from the API).
        // IconVersion() changes whenever a new icon has been fetched, so a
        // consumer can re-decode only when it actually changes.
        bool           HasIcon() const     { return _iconPng != nullptr && _iconLen > 0; }
        const uint8_t* IconData() const    { return _iconPng; }
        size_t         IconLength() const  { return _iconLen; }
        uint32_t       IconVersion() const { return _iconVersion; }

        // Forecast stats (computed from the stored hourly data). Hidden when
        // stale, resumed when a fresh forecast is fetched.
        bool HasForecast() const         { return _today.valid && _tomorrow.valid && (millis() - _lastForecastSuccess) < kForecastStaleMs; }
        int  MaxTempToday() const        { return _today.maxTemp; }
        int  MaxTempTomorrow() const     { return _tomorrow.maxTemp; }
        int  RainChanceToday() const     { return _today.maxRain; }
        int  RainChanceTomorrow() const  { return _tomorrow.maxRain; }
        int  MaxWindToday() const        { return _today.maxWind; }
        int  MaxWindTomorrow() const     { return _tomorrow.maxWind; }

        // Raw hourly forecast for each day (for the forecast graph). Valid only
        // while HasForecast() is true.
        const DayForecast& Today() const    { return _today; }
        const DayForecast& Tomorrow() const { return _tomorrow; }

    private:
        bool fetchWeather();           // query WeatherAPI and read the configured field
        bool fetchForecast();          // query the 2-day hourly forecast
        bool downloadIcon(const String& url);

        void tryWeather();             // fetch with short-retry / staleness handling
        void tryForecast();

        // Retry/staleness tuning
        static const uint32_t kFetchRetryMs    = 30000;    // short wait between failed attempts
        static const uint8_t  kMaxRetries      = 3;        // short retries before resuming normal interval
        static const uint32_t kWeatherStaleMs  = 1800000;  // 30 min: hide current weather if older
        static const uint32_t kForecastStaleMs = 5400000;  // 90 min: hide forecast if older

        const char* _apiKey;
        const char* _tempField;

        String _lat;                 // fixed location, formatted from the ctor args
        String _lon;

        bool  _hasWeather = false;
        float _temp = 0.0f;
        float _windMph = 0.0f;
        int   _windDegree = 0;

        uint8_t  _weatherRetries = 0;
        uint32_t _lastWeatherSuccess = 0;    // millis() of the last good weather fetch
        uint8_t  _forecastRetries = 0;
        uint32_t _lastForecastSuccess = 0;   // millis() of the last good forecast fetch

        uint8_t* _iconPng = nullptr;   // heap buffer holding the icon PNG
        size_t   _iconLen = 0;
        uint32_t _iconVersion = 0;
        String   _lastIconUrl;         // skip re-downloading an unchanged icon

        DayForecast _today;
        DayForecast _tomorrow;

        Interval _weatherRefresh;   // 10 minutes
        Interval _forecastRefresh;  // 30 minutes
};

#endif // _WEATHER_API_H
