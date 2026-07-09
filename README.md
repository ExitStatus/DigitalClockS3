# DigitalClockS3

A WiFi-connected digital clock and weather station for the **LilyGo T-Display-S3**
(ESP32-S3 with a 170×320 ST7789 colour display). It shows a live clock that stays
accurate over NTP, and a set of two-day weather-forecast graphs you can page
through with the on-board buttons.

Everything is rendered into a single off-screen buffer and blitted in one pass,
so the display is flicker-free. Fonts are embedded into the firmware, so there's
no filesystem to prepare.

---

## Features

**Clock page**

- Large seven-segment `HH:MM` with an `AM`/`PM` superscript.
- Date (top-left), WiFi signal-strength bars (top-right).
- Weather strip along the bottom: condition icon + temperature, a rotating
  forecast stat that fades in and out, and a wind arrow + speed.
- Time is synced from NTP once an hour and free-runs from the internal
  clock in between; the display repaints once a minute.

**Forecast graph pages** — each a smooth, colour-coded line graph of the hourly
forecast for **today + tomorrow**, with a temperature/value scale, hour-of-day
ticks, a day divider, and dated headers:

1. **Predicted Temperature**
2. **Predicted Air Pressure**
3. **Predicted Wind Speed** — two lines, wind and gust, with a legend
4. **Predicted Chance of Rain**
5. **Predicted Chance of Snow**

**Buttons**

| Button | Location | Action |
|--------|----------|--------|
| **BOOT** (GPIO0) | bottom-left | Cycle backlight brightness in 10% steps (wraps 100% → 10%). A pop-up shows the level for 2s. |
| **KEY** (GPIO14) | bottom-right | Cycle the page: Clock → Temperature → Pressure → Wind → Rain → Snow → Clock. |

---

## Building & flashing

This is a [PlatformIO](https://platformio.org/) project. With PlatformIO
installed (CLI or the VS Code extension):

```sh
# Build and upload the release firmware to the board over USB
pio run -e RELEASE -t upload

# Or the debug build, which enables verbose serial logging at 115200 baud
pio run -e DEBUG -t upload
```

Before flashing, copy `secrets.ini.example` to `secrets.ini` and set your WiFi,
weather API key, and location (see **Configuration** below).

There are two build environments:

- **`RELEASE`** — optimised, no serial logging. Use this for normal operation.
- **`DEBUG`** — adds `-D DEBUG`, verbose ESP32 core logging, and prints
  diagnostics over the serial monitor (`pio device monitor`). Handy when
  troubleshooting WiFi or the weather fetch.

---

## Configuration

Settings are compile-time `build_flags`. The **secret / personal** ones (WiFi
credentials, API key, location) live in a separate `secrets.ini` that is **not**
committed to git; `platformio.ini` (which *is* committed) references them with
`${secrets.*}`. The remaining, non-sensitive flags stay in `platformio.ini`.

### Secrets (`secrets.ini`)

The repo ships a template. To set up your build:

```sh
cp secrets.ini.example secrets.ini
```

Then edit `secrets.ini` with your own values:

```ini
[secrets]
wifi_ssid       = YourNetworkName
wifi_password   = YourWiFiPassword
weather_api_key = your_weatherapi_key
location_lat    = 51.5074
location_lon    = -0.1278
```

- **`wifi_ssid` / `wifi_password`** — your 2.4 GHz network name and password.
  (The ESP32-S3 does not join 5 GHz networks.)
- **`weather_api_key`** — a free API key from
  [weatherapi.com](https://www.weatherapi.com/). Sign up and copy the key from
  your account dashboard.
- **`location_lat` / `location_lon`** — the latitude and longitude of the
  location you want the weather for, in **decimal degrees**. Positive is
  north / east; negative is south / west. See
  [**Finding your location**](#finding-your-location) below.

`secrets.ini` is listed in `.gitignore`, so your credentials never enter version
control. `platformio.ini` pulls it in via `extra_configs` in its `[platformio]`
section. If `secrets.ini` is missing, the build will fail with an
unresolved-interpolation error — copy the example first.

### Settings (`settings.ini`)

The non-secret, user-tunable settings live in `settings.ini`, which
`platformio.ini` also pulls in via `extra_configs`. Unlike `secrets.ini`, it is
committed to the repo — the values are shared defaults rather than credentials,
so you can edit them in place and rebuild.

```ini
[settings]
weather_temp_field = feelslike_c
forecast_fade_ms   = 1000
forecast_hold_ms   = 8000
```

- **`weather_temp_field`** — which field of WeatherAPI's `current` object to show
  as the temperature. Use `feelslike_c` for the "feels like" temperature or
  `temp_c` for the raw air temperature. (Use the `_f` variants for Fahrenheit,
  e.g. `temp_f`.) The weather is fetched directly from WeatherAPI for your
  coordinates — the device does **not** geocode anything at runtime.
- **`forecast_fade_ms` / `forecast_hold_ms`** — control the rotating forecast stat
  on the clock page. `fade` is the fade in/out duration and `hold` is how long
  each stat stays fully visible, both in milliseconds.

### Display setup (do not change)

The block of `-DTFT_*`, `-DST7789_DRIVER`, `-DLOAD_*`, etc. flags is the
**TFT_eSPI driver configuration for the T-Display-S3** (pin mapping, panel
driver, fonts). It reproduces the board's official `Setup206` inline so it
survives a library reinstall. Leave these alone unless you are porting to
different hardware.

---

## Finding your location

`location_lat` and `location_lon` (in `secrets.ini`) are plain decimal-degree
coordinates. The easiest way to look them up is
**[GeoNames](https://www.geonames.org/)**:

1. Go to <https://www.geonames.org/> and use the search box at the top.
2. Search for your **town, city, or postcode** (e.g. `London` or a postcode).
3. Click the matching result in the list.
4. The place's detail page shows its **latitude and longitude** in decimal
   degrees near the top (and on the map).
5. Copy those two numbers into `location_lat` and `location_lon` in
   `secrets.ini`, then rebuild and flash.

For example, central London (UK) is about `51.5074` latitude, `-0.1278`
longitude — the longitude is negative because it is **west** of the prime
meridian.

> Any source of decimal-degree coordinates works — right-clicking a spot in
> Google Maps also shows the lat/lon — but GeoNames is convenient for looking up
> a place by name or postcode.

---

## Project layout

```
platformio.ini      Board, build flags, library deps, embedded fonts (committed)
settings.ini        User-tunable settings (committed)
secrets.ini         Your WiFi/API key/location (gitignored; copy from the example)
secrets.ini.example Template for secrets.ini
include/            Shared headers (Debug.h, Font.h)
fonts/              Embedded .vlw fonts (Arial, Gill Sans)
lib/
  WifiManager/      Non-blocking WiFi station manager with reconnect/escalation
  NtpClient/        SNTP time sync (hourly) with local free-running clock
  WeatherApi/       WeatherAPI current + 2-day hourly forecast fetch
  SevenSegment/     Seven-segment digit renderer
  Interval/         millis()-based interval timer
src/
  main.cpp          Setup, render loop, buttons, page cycle
  TimePanel / DatePanel / SignalBars / WeatherPanel / ForecastPanel /
  WindPanel / WeatherIcon     Clock-page panels
  HourlyGraph.*     Reusable 2-day hourly line-graph module
  WeatherGraphs.*   Builds each forecast graph page on HourlyGraph
  BrightnessOverlay.*  The brightness pop-up
```

---

## Libraries

Pulled in automatically by PlatformIO (`lib_deps`):

- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) — display driver
- [OneButton](https://github.com/mathertel/OneButton) — button click handling
- [PNGdec](https://github.com/bitbank2/PNGdec) — decodes the weather condition icon
- [ArduinoJson](https://arduinojson.org/) — parses the WeatherAPI responses

---

## License

Released under the [MIT License](LICENSE).
