#ifndef _CONFIG_H
#define _CONFIG_H

// Compile-time configuration, driven by settings.ini via -D flags in
// platformio.ini. Each token in settings.ini (e.g. "c", "mph", "on") is pasted
// onto a prefix to name one of the sentinels below, so `temperature_unit = c`
// arrives here as TEMPERATURE_UNIT=UNIT_c.
//
// The sentinels are deliberately non-zero. An unrecognised token expands to an
// identifier no one defined, and the preprocessor silently evaluates those as 0
// in #if -- so a typo would otherwise pick the first branch instead of failing.
// Giving every valid token a non-zero value lets the validation blocks below
// turn `temperature_unit = celsius` into a build error rather than a
// silently-wrong Celsius build.

#define SW_off     21
#define SW_on      22

#define UNIT_c     31
#define UNIT_f     32

#define UNIT_mph   41
#define UNIT_kph   42

#define UNIT_mb    51
#define UNIT_inhg  52

#define FMT_12     61
#define FMT_24     62

// True when a settings.ini on/off switch is on.
#define ENABLED(x) ((x) == SW_on)

// ---- Validation -----------------------------------------------------------

#if TEMPERATURE_UNIT != UNIT_c && TEMPERATURE_UNIT != UNIT_f
#error "settings.ini: temperature_unit must be 'c' or 'f'"
#endif

#if WIND_UNIT != UNIT_mph && WIND_UNIT != UNIT_kph
#error "settings.ini: wind_unit must be 'mph' or 'kph'"
#endif

#if PRESSURE_UNIT != UNIT_mb && PRESSURE_UNIT != UNIT_inhg
#error "settings.ini: pressure_unit must be 'mb' or 'inhg'"
#endif

#if TIME_FORMAT != FMT_12 && TIME_FORMAT != FMT_24
#error "settings.ini: time_format must be '12' or '24'"
#endif

#if !ENABLED(BLINK_COLON) && BLINK_COLON != SW_off
#error "settings.ini: blink_colon must be 'on' or 'off'"
#endif

// These catch the in-range-but-too-long case, e.g. a stray digit turning F02828
// into F028288. A non-hex character is caught too, but by the preprocessor
// rather than here: 0xGG2828 is not a valid token, and the error ("user-defined
// literal in preprocessor expression") points at the line below without naming
// the setting. Unlike the token switches, there is no way to validate the
// alphabet of a hex literal from inside #if.
#if SEGMENT_ACTIVE_RGB > 0xFFFFFF
#error "settings.ini: segment_active_colour must be RRGGBB hex, no '#' or '0x'"
#endif
#if SEGMENT_INACTIVE_RGB > 0xFFFFFF
#error "settings.ini: segment_inactive_colour must be RRGGBB hex, no '#' or '0x'"
#endif

#if !ENABLED(GRAPH_TEMPERATURE) && GRAPH_TEMPERATURE != SW_off
#error "settings.ini: graph_temperature must be 'on' or 'off'"
#endif
#if !ENABLED(GRAPH_PRESSURE) && GRAPH_PRESSURE != SW_off
#error "settings.ini: graph_pressure must be 'on' or 'off'"
#endif
#if !ENABLED(GRAPH_WIND) && GRAPH_WIND != SW_off
#error "settings.ini: graph_wind must be 'on' or 'off'"
#endif
#if !ENABLED(GRAPH_RAIN) && GRAPH_RAIN != SW_off
#error "settings.ini: graph_rain must be 'on' or 'off'"
#endif
#if !ENABLED(GRAPH_SNOW) && GRAPH_SNOW != SW_off
#error "settings.ini: graph_snow must be 'on' or 'off'"
#endif

#if GRAPH_TIMEOUT_SECONDS < 0 || GRAPH_TIMEOUT_SECONDS > 3600
#error "settings.ini: graph_timeout_seconds must be 0..3600 (0 disables)"
#endif

// Milliseconds on a graph page before it returns to the clock; 0 disables it.
#define GRAPH_TIMEOUT_MS (GRAPH_TIMEOUT_SECONDS * 1000UL)

#if !ENABLED(FORECAST_STAT_MAX_TEMP) && FORECAST_STAT_MAX_TEMP != SW_off
#error "settings.ini: forecast_stat_max_temp must be 'on' or 'off'"
#endif
#if !ENABLED(FORECAST_STAT_RAIN) && FORECAST_STAT_RAIN != SW_off
#error "settings.ini: forecast_stat_rain must be 'on' or 'off'"
#endif
#if !ENABLED(FORECAST_STAT_WIND) && FORECAST_STAT_WIND != SW_off
#error "settings.ini: forecast_stat_wind must be 'on' or 'off'"
#endif

#if !ENABLED(NEWS) && NEWS != SW_off
#error "settings.ini: news must be 'on' or 'off'"
#endif
#if NEWS_INTERVAL_MINUTES < 1
#error "settings.ini: news_interval_minutes must be at least 1"
#endif
#if NEWS_DISPLAY_SPEED < 10 || NEWS_DISPLAY_SPEED > 200
#error "settings.ini: news_display_speed must be 10..200 pixels per second"
#endif
#if NEWS_MAX_ITEMS < 1 || NEWS_MAX_ITEMS > 10
#error "settings.ini: news_max_items must be 1..10"
#endif
// news_country is a string, so #if cannot check it; NewsApi.cpp static_asserts
// its length instead.

// ---- Clock ----------------------------------------------------------------

#define CLOCK_USE_24_HOUR   (TIME_FORMAT == FMT_24)
#define CLOCK_BLINK_COLON   ENABLED(BLINK_COLON)

// 24-bit RRGGBB down to the panel's 16-bit 5-6-5, matching TFT_eSPI::color565()
// exactly. Done here so the colours are constants rather than a per-frame call.
#define RGB565(rgb) ((uint16_t)((((rgb) >> 19) & 0x1F) << 11 | \
                                (((rgb) >> 10) & 0x3F) <<  5 | \
                                (((rgb) >>  3) & 0x1F)))

#define SEGMENT_ACTIVE_COLOUR    RGB565(SEGMENT_ACTIVE_RGB)
#define SEGMENT_INACTIVE_COLOUR  RGB565(SEGMENT_INACTIVE_RGB)

// ---- News -----------------------------------------------------------------

#define NEWS_ENABLED         ENABLED(NEWS)
#define NEWS_INTERVAL_MS     (NEWS_INTERVAL_MINUTES * 60000UL)

// ---- Temperature ----------------------------------------------------------

#if TEMPERATURE_UNIT == UNIT_f
    #define TEMP_UNIT_SUFFIX "_f"
    #define TEMP_UNIT_LABEL  "F"
    // Colour-ramp stops and thresholds are authored in Celsius.
    #define TEMP_STOP(c)     ((c) * 9 / 5 + 32)
#else
    #define TEMP_UNIT_SUFFIX "_c"
    #define TEMP_UNIT_LABEL  "C"
    #define TEMP_STOP(c)     (c)
#endif

// WEATHER_TEMP_FIELD_BASE is "temp" or "feelslike"; the unit picks the suffix.
#define WEATHER_CURRENT_TEMP_FIELD WEATHER_TEMP_FIELD_BASE TEMP_UNIT_SUFFIX
#define WEATHER_HOURLY_TEMP_FIELD  "temp" TEMP_UNIT_SUFFIX

// ---- Wind -----------------------------------------------------------------

#if WIND_UNIT == UNIT_kph
    #define WIND_UNIT_SUFFIX "_kph"
    #define WIND_UNIT_LABEL  "kph"
    // Speed thresholds are authored in mph (1 mph = 1.609 kph).
    #define WIND_STOP(mph)   ((mph) * 1609 / 1000)
#else
    #define WIND_UNIT_SUFFIX "_mph"
    #define WIND_UNIT_LABEL  "mph"
    #define WIND_STOP(mph)   (mph)
#endif

#define WEATHER_WIND_FIELD "wind" WIND_UNIT_SUFFIX
#define WEATHER_GUST_FIELD "gust" WIND_UNIT_SUFFIX

// ---- Pressure -------------------------------------------------------------
//
// Hourly pressure is stored as an integer. Millibars are whole numbers already,
// but inches of mercury need two decimals to be readable (29.92), so in that
// mode the series is stored in hundredths and the graph divides by
// PRESSURE_SCALE when it labels the axis.

#if PRESSURE_UNIT == UNIT_inhg
    #define WEATHER_PRESSURE_FIELD "pressure_in"
    #define PRESSURE_UNIT_LABEL    "inHg"
    #define PRESSURE_SCALE         100
    #define PRESSURE_DECIMALS      2
    // Stops and paddings are authored in millibars (1 mb = 0.02953 inHg). The
    // +1 keeps a small padding from rounding down to nothing.
    #define PRESSURE_STOP(mb)      ((mb) * 2953 / 1000)
    #define PRESSURE_DELTA(mb)     ((mb) * 2953 / 1000 + 1)
    #define PRESSURE_PLOT_LEFT     46   // room for "29.92"
#else
    #define WEATHER_PRESSURE_FIELD "pressure_mb"
    #define PRESSURE_UNIT_LABEL    "mb"
    #define PRESSURE_SCALE         1
    #define PRESSURE_DECIMALS      0
    #define PRESSURE_STOP(mb)      (mb)
    #define PRESSURE_DELTA(mb)     (mb)
    #define PRESSURE_PLOT_LEFT     36   // room for "1013"
#endif

#endif // _CONFIG_H
