#include "WeatherApi.h"
#include "Debug.h"

#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <math.h>

WeatherApi::WeatherApi(const char* apiKey, double lat, double lon, const char* tempField)
    : _apiKey(apiKey), _tempField(tempField),
      _lat(String(lat, 4)),          // fixed location, no geocoding needed
      _lon(String(lon, 4)),
      _weatherRefresh(600000, true),   // 10 minutes; fetch immediately on first Update
      _forecastRefresh(1800000, true)  // 30 minutes; fetch immediately on first Update
{
}

void WeatherApi::Begin()
{
    // Intervals are armed; the work happens in Update() once WiFi is connected.
}

void WeatherApi::Update(bool wifiConnected)
{
    if (!wifiConnected)
        return;                     // no network, nothing to do

    if (_weatherRefresh.Ready())
        tryWeather();

    if (_forecastRefresh.Ready())
        tryForecast();
}

// Fetch the current weather; on failure retry a few times after a short wait,
// then fall back to the normal refresh interval. RetryIn() applies only to the
// next cycle, so calling it again each failure keeps the short cadence going.
void WeatherApi::tryWeather()
{
    if (fetchWeather())
    {
        _weatherRetries = 0;
        _lastWeatherSuccess = millis();
    }
    else if (_weatherRetries < kMaxRetries)
    {
        _weatherRetries++;
        _weatherRefresh.RetryIn(kFetchRetryMs);
        DPRINTF("WeatherApi: weather fetch failed, retry %d/%d shortly\n", _weatherRetries, kMaxRetries);
    }
    else
    {
        _weatherRetries = 0;      // give up short retries; resume normal interval
        DPRINTLN("WeatherApi: weather retries exhausted; waiting for normal interval");
    }
}

void WeatherApi::tryForecast()
{
    if (fetchForecast())
    {
        _forecastRetries = 0;
        _lastForecastSuccess = millis();
    }
    else if (_forecastRetries < kMaxRetries)
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
}

bool WeatherApi::fetchWeather()
{
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

    JsonVariant field = doc["current"][_tempField];
    if (field.isNull())
    {
        DPRINTF("WeatherApi: field '%s' not found in response\n", _tempField);
        return false;
    }

    _temp = field.as<float>();
    _windMph = doc["current"]["wind_mph"] | 0.0f;
    _windDegree = doc["current"]["wind_degree"] | 0;
    _hasWeather = true;
    DPRINTF("WeatherApi: %s = %.1f, wind %.0f mph @ %d deg\n",
            _tempField, _temp, _windMph, _windDegree);

    // Download the condition icon PNG if its URL changed. The URL is
    // protocol-relative ("//cdn.weatherapi.com/..."), so prefix https:.
    const char* icon = doc["current"]["condition"]["icon"] | "";
    if (strlen(icon) > 0)
    {
        String iconUrl = icon;
        if (iconUrl.startsWith("//"))
            iconUrl = "https:" + iconUrl;

        if (iconUrl != _lastIconUrl && downloadIcon(iconUrl))
        {
            _lastIconUrl = iconUrl;
            _iconVersion++;
        }
    }

    return true;
}

// Parse one forecastday's hourly array into the compact structure and compute
// the day's max temperature and max rain chance.
static void parseDay(JsonArrayConst hours, DayForecast& out)
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

        float  t    = h["temp_c"] | 0.0f;
        int    rain = h["chance_of_rain"] | 0;
        int    snow = h["chance_of_snow"] | 0;
        int    wind = (int)lroundf(h["wind_mph"] | 0.0f);
        int    gust = (int)lroundf(h["gust_mph"] | 0.0f);
        int    pres = (int)lroundf(h["pressure_mb"] | 0.0f);
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

bool WeatherApi::fetchForecast()
{
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
    filter["forecast"]["forecastday"][0]["hour"][0]["temp_c"] = true;
    filter["forecast"]["forecastday"][0]["hour"][0]["chance_of_rain"] = true;
    filter["forecast"]["forecastday"][0]["hour"][0]["chance_of_snow"] = true;
    filter["forecast"]["forecastday"][0]["hour"][0]["wind_mph"] = true;
    filter["forecast"]["forecastday"][0]["hour"][0]["gust_mph"] = true;
    filter["forecast"]["forecastday"][0]["hour"][0]["pressure_mb"] = true;

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

    parseDay(days[0]["hour"], _today);
    parseDay(days[1]["hour"], _tomorrow);

    DPRINTF("WeatherApi: forecast today %dC/%d%%/%dmph  tomorrow %dC/%d%%/%dmph\n",
            _today.maxTemp, _today.maxRain, _today.maxWind,
            _tomorrow.maxTemp, _tomorrow.maxRain, _tomorrow.maxWind);
    return _today.valid && _tomorrow.valid;
}

bool WeatherApi::downloadIcon(const String& url)
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

    int len = http.getSize();
    if (len <= 0 || len > 16384)         // sanity bound; these icons are ~1-2 KB
    {
        DPRINTF("WeatherApi: icon size %d rejected\n", len);
        http.end();
        return false;
    }

    uint8_t* buf = (uint8_t*)malloc(len);
    if (!buf)
    {
        http.end();
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    int read = 0;
    uint32_t start = millis();
    while (read < len && millis() - start < 8000)
    {
        int avail = stream->available();
        if (avail > 0)
            read += stream->readBytes(buf + read, min(avail, len - read));
        else
            delay(1);
    }
    http.end();

    if (read != len)
    {
        free(buf);
        return false;
    }

    if (_iconPng)
        free(_iconPng);
    _iconPng = buf;
    _iconLen = len;
    DPRINTF("WeatherApi: icon downloaded (%d bytes)\n", len);
    return true;
}
