#include "WeatherApi.h"
#include "Debug.h"

#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <math.h>
#include <time.h>

namespace
{
    // Holds the network lock for a whole fetch, releasing it however the fetch
    // returns -- and there are several early returns to get wrong by hand.
    //
    // The scope has to be the entire function, not just the HTTP call: the
    // WiFiClientSecure is a local, so its TLS buffers are alive until the
    // function returns. Releasing any sooner would let a second session overlap
    // the tail of this one, which is the whole thing the lock exists to prevent.
    struct NetGuard
    {
        explicit NetGuard(SemaphoreHandle_t lock) : _lock(lock)
        {
            if (_lock) xSemaphoreTake(_lock, portMAX_DELAY);
        }
        ~NetGuard()
        {
            if (_lock) xSemaphoreGive(_lock);
        }

        NetGuard(const NetGuard&) = delete;
        NetGuard& operator=(const NetGuard&) = delete;

        SemaphoreHandle_t _lock;
    };
}

WeatherApi::WeatherApi(const char* apiKey, double lat, double lon, const WeatherFields& fields)
    : _apiKey(apiKey), _fields(fields),
      _lat(String(lat, 4)),          // fixed location, no geocoding needed
      _lon(String(lon, 4)),
      _weatherRefresh(600000, true),   // 10 minutes; fetch immediately on first Update
      _forecastRefresh(1800000, true)  // 30 minutes; fetch immediately on first Update
{
}

void WeatherApi::Begin()
{
    _lock = xSemaphoreCreateMutex();
    if (!_lock)
    {
        DPRINTLN("WeatherApi: could not create mutex");
        return;
    }

    // The intervals are already armed; the worker sleeps until Update() asks
    // for something.
    BaseType_t ok = xTaskCreatePinnedToCore(taskEntry, "weather", kTaskStack,
                                            this, kTaskPriority, &_task, kTaskCore);
    if (ok != pdPASS)
    {
        _task = nullptr;
        DPRINTLN("WeatherApi: could not create worker task");
    }
}

void WeatherApi::taskEntry(void* arg)
{
    static_cast<WeatherApi*>(arg)->taskLoop();
}

// Caller's thread. Returns immediately: it adopts whatever the worker has
// finished, then asks for anything the refresh intervals have made due.
void WeatherApi::Update(bool wifiConnected)
{
    if (!_task)
        return;                     // worker never started; nothing can happen

    collect();

    if (!wifiConnected)
        return;                     // no network, don't bother the worker

    // Ready() re-arms on a true return, so each one that fires must be recorded
    // now -- it cannot be asked again later.
    uint8_t request = 0;
    if (_weatherRefresh.Ready())  request |= ReqWeather;
    if (_forecastRefresh.Ready()) request |= ReqForecast;

    if (request)
        dispatch(request);
}

void WeatherApi::dispatch(uint8_t request)
{
    xSemaphoreTake(_lock, portMAX_DELAY);
    _pending |= request;            // accumulate; a fetch may be in flight
    xSemaphoreGive(_lock);

    xTaskNotifyGive(_task);
}

// Caller's thread. Moves finished results out of the shared slots and applies
// them to the live fields. This is the only place those fields change, and the
// only place the previous icon buffer is freed -- both safe here because no
// render is in progress.
void WeatherApi::collect()
{
    WeatherResult  weather;
    ForecastResult forecast;
    uint8_t done;

    xSemaphoreTake(_lock, portMAX_DELAY);
    done = _done;
    _done = 0;
    if (done & ReqWeather)
    {
        weather = _weatherResult;
        _weatherResult = WeatherResult();   // drop our claim on the icon buffer
    }
    if (done & ReqForecast)
        forecast = _forecastResult;
    xSemaphoreGive(_lock);

    if (done & ReqWeather)  applyWeather(weather);
    if (done & ReqForecast) applyForecast(forecast);
}

// On failure retry a few times after a short wait, then fall back to the normal
// refresh interval. RetryIn() applies only to the next cycle, so calling it
// again on each failure keeps the short cadence going.
void WeatherApi::applyWeather(const WeatherResult& r)
{
    if (!r.ok)
    {
        if (r.iconPng)
            free(r.iconPng);        // downloaded, but the fetch failed after it

        if (_weatherRetries < kMaxRetries)
        {
            _weatherRetries++;
            _weatherRefresh.RetryIn(kFetchRetryMs);
            DPRINTF("WeatherApi: weather fetch failed, retry %d/%d shortly\n", _weatherRetries, kMaxRetries);
        }
        else
        {
            _weatherRetries = 0;    // give up short retries; resume normal interval
            DPRINTLN("WeatherApi: weather retries exhausted; waiting for normal interval");
        }
        return;
    }

    _temp = r.temp;
    _windSpeed = r.windSpeed;
    _windDegree = r.windDegree;
    _hasWeather = true;
    _weatherRetries = 0;
    _lastWeatherSuccess = millis();

    // Swap in the new icon and release the old one. Nothing is decoding it: the
    // consumer only reads IconData() after Update() has returned.
    if (r.iconPng)
    {
        if (_iconPng)
            free(_iconPng);
        _iconPng = r.iconPng;
        _iconLen = r.iconLen;
        _iconVersion++;
    }
}

void WeatherApi::applyForecast(const ForecastResult& r)
{
    if (!r.ok)
    {
        if (_forecastRetries < kMaxRetries)
        {
            _forecastRetries++;
            _forecastRefresh.RetryIn(kFetchRetryMs);
            DPRINTF("WeatherApi: forecast fetch failed, retry %d/%d shortly\n", _forecastRetries, kMaxRetries);
        }
        else
        {
            _forecastRetries = 0;
            DPRINTLN("WeatherApi: forecast retries exhausted; waiting for normal interval");
        }
        return;
    }

    _today = r.today;
    _tomorrow = r.tomorrow;
    _forecastRetries = 0;
}

// A forecast is shown while it is still for the current local day. On a failed
// fetch the stored _today/_tomorrow are left untouched (see applyForecast), so
// this keeps yesterday's-fetched-but-still-for-today data on screen; once the
// date rolls over without a fresh fetch, today's forecast has become yesterday's
// and is dropped.
bool WeatherApi::HasForecast() const
{
    if (!_today.valid || !_tomorrow.valid)
        return false;

    // No date to judge by -- the clock is not set yet, or the API omitted the
    // date. Show what we have rather than hide data we cannot prove is stale.
    time_t now = time(nullptr);
    if (_today.dateEpoch == 0 || now < kClockSetEpoch)
        return true;

    // WeatherAPI's date_epoch is UTC midnight of the forecast date, so gmtime_r
    // recovers that calendar date directly; the current moment is compared in
    // local time. Mixing the two this way makes the rollover happen at local
    // midnight regardless of the device's timezone -- localtime_r on both would
    // shift the forecast's day by one for any location west of UTC.
    struct tm nowTm, fcTm;
    time_t fc = (time_t)_today.dateEpoch;
    localtime_r(&now, &nowTm);
    gmtime_r(&fc, &fcTm);
    return nowTm.tm_year == fcTm.tm_year && nowTm.tm_yday == fcTm.tm_yday;
}

// Worker thread. Sleeps until asked, then does the blocking HTTP work and parks
// the result. Loops rather than returning to the notify, so a request that
// arrived mid-fetch is served without waiting for another notification.
void WeatherApi::taskLoop()
{
    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        for (;;)
        {
            uint8_t request;

            xSemaphoreTake(_lock, portMAX_DELAY);
            request = _pending;
            _pending = 0;
            xSemaphoreGive(_lock);

            if (!request)
                break;              // nothing left to do; go back to sleep

            WeatherResult  weather;
            ForecastResult forecast;

            _fetching = true;
            if (request & ReqWeather)  weather.ok  = fetchWeather(weather);
            if (request & ReqForecast) forecast.ok = fetchForecast(forecast);
            _fetching = false;

            xSemaphoreTake(_lock, portMAX_DELAY);
            if (request & ReqWeather)
            {
                // A result the caller never collected still owns its buffer.
                // Forget the URL too, so the discarded icon is fetched again
                // rather than skipped as unchanged.
                if (_weatherResult.iconPng)
                {
                    free(_weatherResult.iconPng);
                    _lastIconUrl = "";
                }
                _weatherResult = weather;
            }
            if (request & ReqForecast)
                _forecastResult = forecast;
            _done |= request;
            xSemaphoreGive(_lock);

            DPRINTF("WeatherApi: worker stack headroom %u bytes\n",
                    (unsigned)(uxTaskGetStackHighWaterMark(nullptr) * sizeof(StackType_t)));
        }
    }
}

bool WeatherApi::fetchWeather(WeatherResult& out)
{
    // Covers downloadIcon() too, which is only ever called from here -- so that
    // function must not take the lock itself; the mutex is not recursive.
    NetGuard guard(_netLock);

    String url = "https://api.weatherapi.com/v1/current.json?key=" + String(_apiKey) +
                 "&q=" + _lat + "," + _lon;
    DPRINTLN("WeatherApi: fetching weather");

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    if (!http.begin(client, url))
        return false;

    http.setTimeout(8000);
    int code = http.GET();
    if (code != HTTP_CODE_OK)
    {
        DPRINTF("WeatherApi: weather HTTP %d\n", code);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    DynamicJsonDocument doc(2048);
    DeserializationError err = deserializeJson(doc, payload);
    if (err)
    {
        DPRINTF("WeatherApi: JSON error %s\n", err.c_str());
        return false;
    }

    JsonVariant field = doc["current"][_fields.currentTemp];
    if (field.isNull())
    {
        DPRINTF("WeatherApi: field '%s' not found in response\n", _fields.currentTemp);
        return false;
    }

    out.temp = field.as<float>();
    out.windSpeed = doc["current"][_fields.wind] | 0.0f;
    out.windDegree = doc["current"]["wind_degree"] | 0;
    DPRINTF("WeatherApi: %s = %.1f, wind %.0f (%s) @ %d deg\n",
            _fields.currentTemp, out.temp, out.windSpeed, _fields.wind, out.windDegree);

    // Download the condition icon PNG if its URL changed. The URL is
    // protocol-relative ("//cdn.weatherapi.com/..."), so prefix https:.
    const char* icon = doc["current"]["condition"]["icon"] | "";
    if (strlen(icon) > 0)
    {
        String iconUrl = icon;
        if (iconUrl.startsWith("//"))
            iconUrl = "https:" + iconUrl;

        // _lastIconUrl is worker-only state, so it needs no guarding. It is
        // updated on a successful download even though the caller has yet to
        // adopt the buffer -- if the caller drops it, the buffer is freed, not
        // leaked, and the icon is simply re-downloaded on the next change.
        if (iconUrl != _lastIconUrl && downloadIcon(iconUrl, out.iconPng, out.iconLen))
            _lastIconUrl = iconUrl;
    }

    return true;
}

// Parse one forecastday's hourly array into the compact structure and compute
// the day's max temperature and max rain chance.
static void parseDay(JsonArrayConst hours, DayForecast& out, const WeatherFields& f)
{
    out = DayForecast();

    int n = 0;
    int8_t maxT = -128;
    uint8_t maxR = 0;
    uint8_t maxW = 0;

    for (JsonObjectConst h : hours)
    {
        if (n >= 24)
            break;

        float  t    = h[f.hourlyTemp] | 0.0f;
        int    rain = h["chance_of_rain"] | 0;
        int    snow = h["chance_of_snow"] | 0;
        int    wind = (int)lroundf(h[f.wind] | 0.0f);
        int    gust = (int)lroundf(h[f.gust] | 0.0f);
        int    pres = (int)lroundf((h[f.pressure] | 0.0f) * f.pressureScale);
        int8_t ti   = (int8_t)lroundf(t);

        out.hourTemp[n]     = ti;
        out.hourRain[n]     = (uint8_t)rain;
        out.hourSnow[n]     = (uint8_t)snow;
        out.hourWind[n]     = (uint8_t)wind;
        out.hourGust[n]     = (uint8_t)gust;
        out.hourPressure[n] = (uint16_t)pres;
        if (ti > maxT)   maxT = ti;
        if (rain > maxR) maxR = (uint8_t)rain;
        if (wind > maxW) maxW = (uint8_t)wind;
        n++;
    }

    out.hourCount = n;
    out.maxTemp = maxT;
    out.maxRain = maxR;
    out.maxWind = maxW;
    out.valid = (n > 0);
}

bool WeatherApi::fetchForecast(ForecastResult& out)
{
    NetGuard guard(_netLock);

    String url = "https://api.weatherapi.com/v1/forecast.json?key=" + String(_apiKey) +
                 "&q=" + _lat + "," + _lon + "&days=2";
    DPRINTLN("WeatherApi: fetching forecast");

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    if (!http.begin(client, url))
        return false;

    http.setTimeout(10000);
    int code = http.GET();
    if (code != HTTP_CODE_OK)
    {
        DPRINTF("WeatherApi: forecast HTTP %d\n", code);
        http.end();
        return false;
    }

    // getString() decodes chunked transfer encoding (the raw stream would carry
    // chunk markers that break JSON parsing). The body is large (~50 KB) but a
    // filter keeps only the hourly temp/rain fields, so the parsed doc stays tiny.
    String payload = http.getString();
    http.end();

    StaticJsonDocument<640> filter;
    filter["forecast"]["forecastday"][0]["date_epoch"] = true;   // which day each block is for
    filter["forecast"]["forecastday"][0]["hour"][0][_fields.hourlyTemp] = true;
    filter["forecast"]["forecastday"][0]["hour"][0]["chance_of_rain"] = true;
    filter["forecast"]["forecastday"][0]["hour"][0]["chance_of_snow"] = true;
    filter["forecast"]["forecastday"][0]["hour"][0][_fields.wind] = true;
    filter["forecast"]["forecastday"][0]["hour"][0][_fields.gust] = true;
    filter["forecast"]["forecastday"][0]["hour"][0][_fields.pressure] = true;

    DynamicJsonDocument doc(8192);
    DeserializationError err =
        deserializeJson(doc, payload, DeserializationOption::Filter(filter));

    if (err)
    {
        DPRINTF("WeatherApi: forecast JSON error %s\n", err.c_str());
        return false;
    }

    JsonArrayConst days = doc["forecast"]["forecastday"];
    if (days.size() < 2)
        return false;

    parseDay(days[0]["hour"], out.today, _fields);
    parseDay(days[1]["hour"], out.tomorrow, _fields);
    out.today.dateEpoch    = days[0]["date_epoch"] | 0u;
    out.tomorrow.dateEpoch = days[1]["date_epoch"] | 0u;

    DPRINTF("WeatherApi: forecast today %d/%d%%/%d  tomorrow %d/%d%%/%d\n",
            out.today.maxTemp, out.today.maxRain, out.today.maxWind,
            out.tomorrow.maxTemp, out.tomorrow.maxRain, out.tomorrow.maxWind);
#ifdef DEBUG
    {
        time_t d = (time_t)out.today.dateEpoch;
        struct tm t; gmtime_r(&d, &t);   // date_epoch is UTC midnight of the forecast date
        char buf[16]; strftime(buf, sizeof(buf), "%Y-%m-%d", &t);
        DPRINTF("WeatherApi: forecast is for %s (epoch %u)\n", buf, (unsigned)out.today.dateEpoch);
    }
#endif
    return out.today.valid && out.tomorrow.valid;
}

// Worker thread. On success `buf` is a fresh malloc'd buffer whose ownership
// passes to the caller of this function; it is never installed directly.
bool WeatherApi::downloadIcon(const String& url, uint8_t*& buf, size_t& len)
{
    DPRINTF("WeatherApi: downloading icon %s\n", url.c_str());

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    if (!http.begin(client, url))
        return false;

    http.setTimeout(8000);
    int code = http.GET();
    if (code != HTTP_CODE_OK)
    {
        DPRINTF("WeatherApi: icon HTTP %d\n", code);
        http.end();
        return false;
    }

    int size = http.getSize();
    if (size <= 0 || size > 16384)       // sanity bound; these icons are ~1-2 KB
    {
        DPRINTF("WeatherApi: icon size %d rejected\n", size);
        http.end();
        return false;
    }

    uint8_t* tmp = (uint8_t*)malloc(size);
    if (!tmp)
    {
        http.end();
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    int read = 0;
    uint32_t start = millis();
    while (read < size && millis() - start < 8000)
    {
        int avail = stream->available();
        if (avail > 0)
            read += stream->readBytes(tmp + read, min(avail, size - read));
        else
            delay(1);
    }
    http.end();

    if (read != size)
    {
        free(tmp);
        return false;
    }

    buf = tmp;
    len = (size_t)size;
    DPRINTF("WeatherApi: icon downloaded (%d bytes)\n", size);
    return true;
}
