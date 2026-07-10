#ifndef _WEATHER_API_H
#define _WEATHER_API_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "Interval.h"

// Which WeatherAPI JSON fields to read. Named rather than hardcoded so the
// display units are chosen at build time (see include/Config.h): the API serves
// each quantity in both conventions under a different key.
struct WeatherFields
{
    const char* currentTemp;    // "temp_c" | "feelslike_c" | "temp_f" | "feelslike_f"
    const char* hourlyTemp;     // "temp_c" | "temp_f"
    const char* wind;           // "wind_mph" | "wind_kph"  (current and hourly)
    const char* gust;           // "gust_mph" | "gust_kph"
    const char* pressure;       // "pressure_mb" | "pressure_in"

    // Multiplier applied to the raw pressure before it is stored as an integer.
    // 1 for millibars (already whole); 100 for inches of mercury, which would
    // otherwise round to a useless 29 or 30.
    int pressureScale;
};

// Compact per-day forecast: hourly temperature and rain chance kept in small
// arrays (int8/uint8) to save memory, plus the day's computed maxima. All
// values are in the units named by the WeatherFields the API was built with.
struct DayForecast
{
    bool    valid = false;
    int8_t  maxTemp = 0;         // max hourly temperature (rounded)
    uint8_t maxRain = 0;         // max hourly chance_of_rain (%)
    uint8_t maxWind = 0;         // max hourly wind speed (rounded)
    uint8_t hourCount = 0;
    int8_t   hourTemp[24];       // rounded degrees per hour
    uint8_t  hourRain[24];       // chance_of_rain % per hour
    uint8_t  hourWind[24];       // wind speed per hour
    uint16_t hourPressure[24];   // pressure per hour, scaled by pressureScale
    uint8_t  hourSnow[24];       // chance_of_snow % per hour
    uint8_t  hourGust[24];       // wind gust per hour
};

// Fetches the current temperature from WeatherAPI (api.weatherapi.com) for a
// fixed latitude/longitude supplied at construction. On the first Update() after
// WiFi is up it fetches immediately, then the weather every 10 minutes and the
// forecast every 30 minutes. Which JSON fields are read -- and therefore which
// units the values carry -- is supplied as a WeatherFields.
//
// The HTTP calls are blocking, and a failing one blocks for its whole timeout,
// so they run on a worker task rather than on the caller's thread. Update() only
// posts a request and picks up whatever the worker has finished, and returns at
// once; it never waits for the network.
//
// The split of ownership is what keeps this free of locks in the hot path:
//
//   * The worker owns the fetching. It parses into staging results and never
//     touches the fields the accessors below read.
//   * The caller owns the data. Update() copies a finished result into those
//     fields, on the caller's thread, at a point of its choosing.
//
// So Today(), IconData() and the rest need no synchronisation: nothing mutates
// them while a render is reading them. The one rule this imposes on the caller
// is that Update() must not be called from inside a render -- it is the moment
// the data is allowed to change, and the moment the previous icon buffer is
// freed. Calling it once at the top of loop(), as main.cpp does, satisfies that.
class WeatherApi
{
    public:
        WeatherApi(const char* apiKey, double lat, double lon, const WeatherFields& fields);

        void Begin();                  // starts the worker task
        void Update(bool wifiConnected);   // non-blocking: collect, then maybe dispatch

        // Serialises this object's TLS sessions against other users of the same
        // lock. A live WiFiClientSecure costs tens of KB of heap, so only one may
        // exist at a time; the lock is what guarantees that. Optional: unset, the
        // fetches run exactly as they did before.
        void SetNetworkLock(SemaphoreHandle_t lock) { _netLock = lock; }

        // True while the worker has a fetch in flight, including the time spent
        // waiting for the network lock. A plain bool rather than a locked read:
        // it is one aligned word, written by the worker and read by the loop
        // thread, and a value one frame stale costs nothing here.
        bool Busy() const { return _fetching; }

        // False while there has never been a successful fetch, or once the last
        // successful fetch is older than the staleness window (so stale data is
        // not displayed). Becomes true again as soon as a fresh fetch succeeds.
        bool  HasWeather() const  { return _hasWeather && (millis() - _lastWeatherSuccess) < kWeatherStaleMs; }
        float Temperature() const { return _temp; }
        float WindSpeed() const   { return _windSpeed; }
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
        // Which fetches the worker has been asked for. These accumulate: an
        // interval that fires while a fetch is in flight must not be dropped,
        // and Interval::Ready() re-arms itself the moment it returns true, so
        // the trigger cannot be read a second time.
        enum Request : uint8_t
        {
            ReqWeather  = 1 << 0,
            ReqForecast = 1 << 1,
        };

        // A finished fetch, waiting for the caller to adopt it. The icon buffer's
        // ownership passes with the struct: whoever holds it last frees it.
        struct WeatherResult
        {
            bool     ok = false;
            float    temp = 0.0f;
            float    windSpeed = 0.0f;
            int      windDegree = 0;
            uint8_t* iconPng = nullptr;   // non-null only when the icon changed
            size_t   iconLen = 0;
        };

        struct ForecastResult
        {
            bool ok = false;
            DayForecast today;
            DayForecast tomorrow;
        };

        static void taskEntry(void* arg);
        void taskLoop();                  // worker thread
        void dispatch(uint8_t request);   // caller -> worker
        void collect();                   // worker -> caller; adopts finished results

        void applyWeather(const WeatherResult& r);     // caller thread
        void applyForecast(const ForecastResult& r);   // caller thread

        // Worker thread. Each writes its result into `out` rather than into the
        // members, so nothing the accessors expose is touched off-thread.
        bool fetchWeather(WeatherResult& out);
        bool fetchForecast(ForecastResult& out);
        bool downloadIcon(const String& url, uint8_t*& buf, size_t& len);

        // Retry/staleness tuning
        static const uint32_t kFetchRetryMs    = 30000;    // short wait between failed attempts
        static const uint8_t  kMaxRetries      = 3;        // short retries before resuming normal interval
        static const uint32_t kWeatherStaleMs  = 1800000;  // 30 min: hide current weather if older
        static const uint32_t kForecastStaleMs = 5400000;  // 90 min: hide forecast if older

        // Worker task. The stack has to carry a TLS handshake (mbedTLS is
        // stack-hungry) and there is no PSRAM, so this comes out of internal
        // SRAM; a DEBUG build logs the high-water mark after each fetch.
        // Pinned to core 0 alongside the WiFi stack, leaving core 1 -- where the
        // Arduino loop() runs -- free to keep rendering.
        static const uint32_t   kTaskStack    = 10240;
        static const UBaseType_t kTaskPriority = 1;   // same as loopTask; below WiFi
        static const BaseType_t  kTaskCore     = 0;

        const char*  _apiKey;
        WeatherFields _fields;

        String _lat;                 // fixed location, formatted from the ctor args
        String _lon;

        bool  _hasWeather = false;
        float _temp = 0.0f;
        float _windSpeed = 0.0f;
        int   _windDegree = 0;

        uint8_t  _weatherRetries = 0;
        uint32_t _lastWeatherSuccess = 0;    // millis() of the last good weather fetch
        uint8_t  _forecastRetries = 0;
        uint32_t _lastForecastSuccess = 0;   // millis() of the last good forecast fetch

        uint8_t* _iconPng = nullptr;   // heap buffer holding the icon PNG
        size_t   _iconLen = 0;
        uint32_t _iconVersion = 0;

        String   _lastIconUrl;         // worker only: skip re-downloading an unchanged icon

        DayForecast _today;
        DayForecast _tomorrow;

        Interval _weatherRefresh;   // 10 minutes
        Interval _forecastRefresh;  // 30 minutes

        // Held by the worker for the length of a fetch, and never at the same time
        // as _lock. Lock order is netLock first, then _lock, always released
        // before the other is taken -- so no cycle exists to deadlock on.
        SemaphoreHandle_t _netLock = nullptr;

        // Written by the worker, read by the loop thread. See Busy().
        volatile bool _fetching = false;

        // ---- shared between the caller and the worker, guarded by _lock ------
        // Held only for the handover: a few words in dispatch(), a struct copy in
        // collect(). Never held across a fetch, and never across a render.
        SemaphoreHandle_t _lock = nullptr;
        TaskHandle_t      _task = nullptr;

        uint8_t _pending = 0;      // requested, not yet picked up by the worker
        uint8_t _done    = 0;      // finished, not yet adopted by the caller
        WeatherResult  _weatherResult;
        ForecastResult _forecastResult;
};

#endif // _WEATHER_API_H
