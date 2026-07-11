# DigitalClockS3

A WiFi-connected digital clock and weather station for the **LilyGo T-Display-S3**
(ESP32-S3 with a 170×320 ST7789 colour display). It shows a live clock that stays
accurate over NTP, scrolling news headlines, and a set of two-day
weather-forecast graphs you can page through with the on-board buttons.

![The clock page running on the T-Display-S3: a large seven-segment time with the
unlit segments ghosted, the date top-left, WiFi signal bars top-right, and a
weather line along the bottom showing temperature, forecast wind, and current
wind speed.](docs/screen.jpg)

Everything is rendered into a single off-screen buffer and blitted in one pass,
so the display is flicker-free. Fonts and icons are embedded into the firmware,
so there's no filesystem to prepare.

Weather and news are fetched on background worker tasks, so a slow or failing API
never stalls the clock — the colon keeps blinking and the signal bars stay live
through a fetch. A download arrow appears beside the signal bars while anything
is in flight.

---

## Features

**Clock page**

- Large seven-segment `HH:MM` with an `AM`/`PM` superscript and a colon that
  blinks once a second.
- Date (top-left), WiFi signal-strength bars (top-right). The bars become a red
  X whenever the link is down, so an outage is distinct from a weak signal. A
  download arrow sits to their left while data is being fetched.
- Weather strip along the bottom: condition icon + temperature, a rotating
  forecast stat that fades in and out, and a wind arrow + speed.
- News headlines. When there are unread ones the clock digits shrink to open a
  band beneath them, each headline scrolls through it, and the digits grow back
  when the queue empties. The weather strip is never disturbed.
- Time is synced from NTP once an hour and free-runs from the internal clock in
  between. The display repaints each second to blink the colon, or once a minute
  if `blink_colon` is off.

**Forecast graph pages**

The **KEY** button pages through five graphs, each a smooth line of the hourly
forecast across **today and tomorrow**. They share one frame: a value scale down
the left, hour-of-day ticks along the bottom, a dashed divider at midnight, and a
dated header over each day. Every graph auto-scales its axis to the data in view,
so a flat day and a stormy one both fill the height. A graph turned off in
`settings.ini` is compiled out and skipped in the cycle.

A graph page returns to the clock on its own after `graph_timeout_seconds`
(default 30) so the display doesn't sit on a graph indefinitely; each press of the
KEY button restarts the countdown, and setting it to `0` disables the auto-return.

### Predicted Temperature

The apparent ("feels like") temperature hour by hour, in °C or °F. The line is
tinted by its own value — cooler low, warmer high — so the shape of the day reads
at a glance.

![Predicted Temperature graph: a line rising from about 20°C overnight to a
mid-30s afternoon peak on each of the two days.](docs/graph-temperature.jpg)

### Predicted Air Pressure

Sea-level pressure in millibars or inches of mercury. A steady rise (as here)
suggests settling weather; a fall, the opposite.

![Predicted Air Pressure graph: a line climbing gradually from about 1015 mb to
1025 mb across the two days.](docs/graph-pressure.jpg)

### Predicted Wind Speed

Two lines — sustained **wind** and **gust** — with a legend, in mph or kph.

![Predicted Wind Speed graph: two lines, wind and gust, tracking each other
between roughly 5 and 22 mph, with a legend top-right.](docs/graph-wind.jpg)

### Predicted Chance of Rain

Hourly probability of rain as a percentage. The axis fits the range on show — here
a dry spell with a peak around 20% overnight.

![Predicted Chance of Rain graph: mostly low with a single peak reaching about
20% in the early hours of the second day.](docs/graph-rain.jpg)

### Predicted Chance of Snow

The same, for snow — flat at 0% on a July forecast, and the reason the graph is a
compile-time option rather than always on.

![Predicted Chance of Snow graph: a flat line along 0% across both
days.](docs/graph-snow.jpg)

**Buttons**

| Button | Location | Action |
|--------|----------|--------|
| **BOOT** (GPIO0) | bottom-left | Cycle backlight brightness in 10% steps (wraps 100% → 10%). A pop-up shows the level for 2 s, and the chosen level is remembered across reboots. |
| **KEY** (GPIO14) | bottom-right | Cycle the page: Clock → Temperature → Pressure → Wind → Rain → Snow → Clock. A graph returns to the clock on its own after `graph_timeout_seconds`. |

![The brightness pop-up: a "60%" overlay with a progress bar, shown briefly over
the clock while the BOOT button steps the backlight.](docs/brightness.jpg)

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
- **`DEBUG`** — adds `-D DEBUG` and prints diagnostics over the serial monitor
  (`pio device monitor`). Handy when troubleshooting WiFi, the weather fetch, or
  the news parser.

  Core logging is capped at `CORE_DEBUG_LEVEL=3` (INFO) deliberately. At level 4
  and above `HTTPClient` logs every request URL, and the WeatherAPI key is a query
  parameter of that URL — so raising it prints your API key in clear text on the
  serial console and into any captured log.

---

## Configuration

Settings are compile-time `build_flags`. The **secret / personal** ones (WiFi
credentials, API key, location) live in `secrets.ini`, which is **not** committed
to git. The **non-sensitive** ones live in `settings.ini`, which is. `platformio.ini`
pulls both in through `extra_configs` and references their values as
`${secrets.*}` and `${settings.*}`.

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

These are **compile-time** settings, so changing one means reflashing. Every
token is validated in `include/Config.h`: a misspelt value (`temperature_unit =
celsius`) fails the build with a message naming the setting, rather than quietly
falling back to a default.

```ini
[settings]
time_format = 12                   ; 12 | 24
blink_colon = on                   ; on | off

segment_active_colour   = F02828   ; RRGGBB
segment_inactive_colour = 0A0A0A   ; RRGGBB

weather_temp_field = feelslike     ; temp | feelslike
temperature_unit   = c             ; c | f
wind_unit          = mph           ; mph | kph
pressure_unit      = mb            ; mb | inhg

graph_temperature = on             ; on | off
graph_pressure    = on             ; on | off
graph_wind        = on             ; on | off
graph_rain        = on             ; on | off
graph_snow        = on             ; on | off
graph_timeout_seconds = 30         ; seconds on a graph before returning to clock; 0 = off

forecast_stat_max_temp = on        ; on | off
forecast_stat_rain     = on        ; on | off
forecast_stat_wind     = on        ; on | off
forecast_fade_ms   = 1000
forecast_hold_ms   = 8000

news                  = on         ; on | off
news_country          = GB         ; ISO 3166-1 alpha-2
news_interval_minutes = 30
news_display_speed    = 60         ; pixels per second
news_max_items        = 5
```

#### Clock

- **`time_format`** — `12` or `24`. In 12-hour mode the hour drops its leading
  zero and an AM/PM superscript sits to the right of the time. In 24-hour mode
  the hour is zero-padded (`09:05`) and the superscript disappears, so `HH:MM`
  centres on its own.
- **`blink_colon`** — `on` (the default) or `off`. On blinks the `:` between the
  hours and minutes: lit on even seconds, dark on odd, so a full cycle takes two
  seconds. Off leaves it lit permanently. Note that the clock page would
  otherwise repaint only on the minute rollover; blinking makes it repaint every
  second, which is the cost of the effect.
- **`segment_active_colour` / `segment_inactive_colour`** — the colours of the
  seven-segment digits, as `RRGGBB` hex with **no** leading `#` or `0x`. Every
  segment is drawn every time: lit ones in the active colour, unlit ones in the
  inactive colour, which is what gives the digits their "ghost" outline. Set the
  inactive colour to `000000` to remove the ghosting. The AM/PM superscript and
  the colon both take the active colour.

  The panel is 16-bit, so each colour is quantised on the way to the display —
  red and blue to 5 bits, green to 6. Two hex values within a few units of each
  other can land on the same on-screen colour.

#### Units

Each quantity picks its own unit, so a mixed convention like the UK's (Celsius,
mph, millibars) is expressible — the defaults above are exactly that.

- **`weather_temp_field`** — which reading drives the temperature: `feelslike`
  for the apparent "feels like" temperature, or `temp` for the raw air
  temperature. Combined with `temperature_unit` this names the WeatherAPI field
  to read. The weather is fetched directly from WeatherAPI for your coordinates —
  the device does **not** geocode anything at runtime.
- **`temperature_unit`** — `c` or `f`. Sets the letter drawn beside the degree
  ring and rescales the temperature graph's colour ramp.
- **`wind_unit`** — `mph` or `kph`. Also converts the wind arrow's colour
  thresholds (green below 10 mph, amber at 25, red above 40).
- **`pressure_unit`** — `mb` or `inhg`. In `inhg` the pressure graph labels its
  axis to two decimals (`29.92`); in `mb` it stays whole numbers (`1013`).

#### Graph pages

The clock is always the first page. Each graph switched on here joins the cycle
that the bottom-right button steps through, in the order listed. A graph
switched off is **compiled out entirely**, so turning one off skips its page and
reclaims its flash rather than showing a blank screen.

**`graph_timeout_seconds`** is how long a graph page stays up before returning to
the clock on its own (default 30). Each KEY-button press restarts the countdown, so
it fires only after the page has been left untouched. Set it to `0` to disable the
auto-return and leave a graph up until the button is pressed again.

#### Forecast stats

`forecast_stat_max_temp`, `forecast_stat_rain` and `forecast_stat_wind` choose
which stats fade in and out along the bottom of the clock page. Each one enabled
joins the rotation; turn all three off to leave the bottom line to the current
temperature and wind alone.

`forecast_fade_ms` and `forecast_hold_ms` control that rotation: `fade` is the
fade in/out duration and `hold` is how long each stat stays fully visible, both
in milliseconds.

#### News headlines

Headlines come from Google News' RSS feed for one country. Only titles and
publication dates are read. The publication date of the last headline shown is
kept in NVS, so a reboot does not replay news you have already seen.

- **`news`** — `on` or `off`. Off compiles the whole feature out; the clock page
  is then exactly as it was before.
- **`news_country`** — an ISO 3166-1 alpha-2 country code (`GB`, `US`, `AU`, `IE`).
  The feed is always requested in English, so a country that publishes no English
  edition will give thin results.
- **`news_interval_minutes`** — how often to check for new headlines. The *first*
  check is deliberately held back to five minutes after boot, so the clock and
  weather have the network and the screen to themselves on a cold start.
- **`news_display_speed`** — the ticker's scroll speed, in pixels per second. This
  sets the reading pace, not the time on screen: a long headline simply takes
  longer to cross. At `60` a typical headline is visible for around 14 seconds.
- **`news_max_items`** — how many unseen headlines to show per check. This also
  caps the very first run, where every item in the feed is unseen and would
  otherwise queue up for a quarter of an hour.

Google appends the publisher to every title (`... - BBC News`); that suffix is
stripped before display.

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
platformio.ini      Board, build flags, library deps, embedded fonts/images (committed)
settings.ini        User-tunable settings (committed)
secrets.ini         Your WiFi/API key/location (gitignored; copy from the example)
secrets.ini.example Template for secrets.ini
include/            Shared headers (Config.h, Debug.h, Font.h)
fonts/              Embedded .vlw smooth fonts (Cabin; see fonts/README.md)
images/             Embedded .png icons (see ATTRIBUTION.md)
lib/
  WifiManager/      Non-blocking WiFi station manager with reconnect/escalation
  NtpClient/        SNTP time sync (hourly) with local free-running clock
  WeatherApi/       WeatherAPI current + 2-day hourly forecast, on a worker task
  NewsApi/          Google News RSS, streamed and parsed incrementally, on a worker task
  Icon/             PNG decode, transparent-border crop, scale, alpha-blended draw
  SevenSegment/     Seven-segment digit renderer
  Interval/         millis()-based interval timer
src/
  main.cpp          Setup, render loop, buttons, page cycle
  TimePanel / DatePanel / SignalBars / WeatherPanel / ForecastPanel /
  WindPanel / WeatherIcon     Clock-page panels
  NewsTicker.*      Shrinks the clock and scrolls a headline beneath it
  HourlyGraph.*     Reusable 2-day hourly line-graph module
  WeatherGraphs.*   Builds each forecast graph page on HourlyGraph
  BrightnessOverlay.*  The brightness pop-up
```

Both API clients share a mutex around their TLS sessions. A live
`WiFiClientSecure` costs tens of KB of heap, and two at once would not fit.

---

## Libraries

Pulled in automatically by PlatformIO (`lib_deps`):

- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) — display driver
- [OneButton](https://github.com/mathertel/OneButton) — button click handling
- [PNGdec](https://github.com/bitbank2/PNGdec) — decodes the weather condition icon
  and the embedded status icons
- [ArduinoJson](https://arduinojson.org/) — parses the WeatherAPI responses

The news feed is not JSON, and is far too large to hold in memory, so it is
streamed straight through a small hand-written parser rather than a library.

---

## License

Released under the [MIT License](LICENSE).

Bundled icons and other third-party assets carry their own terms — see
[ATTRIBUTION.md](ATTRIBUTION.md).
