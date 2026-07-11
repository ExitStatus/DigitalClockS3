# Changelog

## v1.0.0

First release. A WiFi-connected clock and weather station for the LilyGo
T-Display-S3, with news headlines and two-day forecast graphs.

### Clock

- Large seven-segment `HH:MM`, with unlit segments drawn as a ghost outline.
- 12- or 24-hour format; in 12-hour mode an AM/PM superscript, in 24-hour a
  zero-padded, self-centring `HH:MM`.
- Optional blinking colon (lit on even seconds).
- Segment colours — lit and unlit — settable as `RRGGBB` hex.
- Date top-left, WiFi signal-strength bars top-right.
- Time synced from NTP hourly, free-running from the internal clock between syncs.

### Weather

- Current conditions and a two-day hourly forecast from WeatherAPI, for a fixed
  latitude/longitude (no runtime geocoding).
- Bottom line: condition icon and temperature, a rotating forecast stat that fades
  in and out, and a wind arrow with speed.
- Fetched on a **background worker task**, so a slow or failing API never stalls the
  clock — the colon keeps blinking and the display keeps rendering through a fetch.
- Today's forecast is **kept across failed fetches** for as long as it is still for
  the current day, and dropped once the date rolls over.
- Failed fetches retry every 10 s, up to 15 times, before falling back to the normal
  interval — so a forecast that misses at boot recovers within seconds.
- A **download indicator** appears beside the signal bars whenever data is being
  fetched, and stays lit through a whole retry cycle.

### Forecast graphs

Five pages, each a smooth line of the hourly forecast across today and tomorrow,
with a value scale, hour ticks, a midnight divider and dated headers. Each
auto-scales its axis, and each can be compiled out individually.

- Predicted temperature
- Predicted air pressure
- Predicted wind speed (wind and gust, with a legend)
- Predicted chance of rain
- Predicted chance of snow
- Graph pages return to the clock on their own after a configurable timeout
  (default 30 s).

### News headlines

- Google News RSS for a configurable country. The feed is **streamed and parsed
  incrementally** — never buffered — through a hand-written parser that handles
  CDATA, XML entities and raw UTF-8.
- When headlines are waiting, the clock digits shrink to open a band beneath them
  and each headline scrolls through it; the digits grow back when the queue drains.
  The weather line is never disturbed.
- The last-shown publication date is kept in NVS, so a reboot never replays news you
  have already read.
- Feed items are not in publication order, so the parser sorts them and shows the
  oldest first.

### Display and hardware

- Everything is composed in a single off-screen buffer and blitted in one pass, so
  the display is flicker-free.
- Backlight brightness cycles in 10% steps from the BOOT button, with a pop-up
  showing the level — and the chosen level is **saved in NVS** and restored on boot.
- The WiFi bars become a red X when the link is down, so an outage is distinct from a
  weak signal; a health probe also catches silent dropouts (associated but no IP).
- `Icon` handles PNG decode, cropping to the transparent bounding box, scaling with
  premultiplied alpha, and alpha-blended drawing.

### Configuration

- All user-tunable settings live in `settings.ini` (committed); WiFi credentials, the
  API key and location live in `secrets.ini` (gitignored).
- Settings are **compile-time and validated**: a misspelt value fails the build with a
  message naming the setting, rather than silently falling back to a default.
- Configurable: time format, colon blink, segment colours, temperature/wind/pressure
  units, which reading drives the temperature, which graphs and forecast stats are
  enabled, fade and hold timings, the graph timeout, and all the news options.

### Security

- Core logging is capped at `CORE_DEBUG_LEVEL=3`. Above that, `HTTPClient` logs every
  request URL — and the WeatherAPI key is a query parameter of it, so a higher level
  printed the key in clear text on the serial console.

### Licensing and assets

- Bundled fonts replaced with **Cabin** (SIL OFL), generated from the upstream TTF;
  the previous Arial and Gill Sans conversions were proprietary. Unused faces pruned,
  cutting embedded font data from ~265 KB to ~74 KB.
- `ATTRIBUTION.md` records the fonts, the download icon (CC BY 4.0), library licences
  and data sources.
- Released under the MIT License.

### Documentation

- README covering the features, the settings reference, building and flashing, and
  finding your coordinates — with photos of the running device and each graph.
- The 3D-printed case and stand, with a link to the printable model.
