#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#include "Config.h"
#include "Font.h"
#include "Icon.h"
#include "Interval.h"
#include "DatePanel.h"
#include "NtpClient.h"
#include "WeatherApi.h"
#include "SignalBars.h"
#include "TimePanel.h"
#include "WeatherPanel.h"
#include "WeatherIcon.h"
#include "ForecastPanel.h"
#include "WindPanel.h"
#include "WifiManager.h"
#include "BrightnessOverlay.h"
#include "WeatherGraphs.h"

#if NEWS_ENABLED
#include "NewsApi.h"
#include "NewsTicker.h"
#endif

#include <OneButton.h>
#include "Debug.h"

#define PIN_LCD_BL 38

// The T-Display-S3's two on-board push buttons (active-low).
#define PIN_BUTTON_1 0    // BOOT button (also the strapping/boot pin)
#define PIN_BUTTON_2 14   // KEY button
static const int kBacklightFreq    = 5000;
static const int kBacklightChannel = 0;
static const int kBacklightBits    = 8;

// Backlight brightness, in 10% steps (10..100), cycled by the bottom-left button.
static int backlightPercent = 50;

static void applyBacklight()
{
    ledcWrite(kBacklightChannel, backlightPercent * 255 / 100);   // 0..255 duty
}

// One step brighter; after 100% wrap back around to 10%.
static void cycleBacklight()
{
    backlightPercent += 10;
    if (backlightPercent > 100)
        backlightPercent = 10;
    applyBacklight();
}

TFT_eSPI    tft   = TFT_eSPI();
TFT_eSprite frame = TFT_eSprite(&tft);   // full-screen offscreen buffer

// Top-right status geometry
static const int kStatusMargin  = 6;    // gap from top/right edges
static const int kStatusHeight  = 24;   // height of the status row
static const int kSignalWidth   = 22;   // width of the signal bar graph
static const int kSignalHeight  = 18;   // height of the signal bar graph
static const int kStatusGap     = 6;    // gap between items in the status row

// Which JSON fields WeatherAPI is asked for, and hence the units everything
// downstream carries. Derived from settings.ini via Config.h.
static const WeatherFields kWeatherFields = {
    WEATHER_CURRENT_TEMP_FIELD,
    WEATHER_HOURLY_TEMP_FIELD,
    WEATHER_WIND_FIELD,
    WEATHER_GUST_FIELD,
    WEATHER_PRESSURE_FIELD,
    PRESSURE_SCALE,
};

// The rotating bottom-line stats enabled in settings.ini.
static const uint8_t kForecastStats =
      (ENABLED(FORECAST_STAT_MAX_TEMP) ? StatMaxTemp : 0)
    | (ENABLED(FORECAST_STAT_RAIN)     ? StatRain    : 0)
    | (ENABLED(FORECAST_STAT_WIND)     ? StatWind    : 0);

WifiManager    wifi(WIFI_SSID, WIFI_PASSWORD);
SignalBars     signalBars(kSignalWidth, kSignalHeight);
NtpClient      ntp;
WeatherApi     weather(WEATHER_API_KEY, LOCATION_LAT, LOCATION_LON, kWeatherFields);
DatePanel      datePanel(kStatusMargin, kStatusMargin + kStatusHeight / 2);   // left, centred on the icon row
TimePanel      timePanel(CLOCK_USE_24_HOUR, CLOCK_BLINK_COLON,
                         SEGMENT_ACTIVE_COLOUR, SEGMENT_INACTIVE_COLOUR);
WeatherIcon    weatherIcon(&tft);
WeatherPanel   weatherPanel(kStatusMargin);   // bottom-left; baseline passed at render
ForecastPanel  forecastPanel(FORECAST_FADE_MS, FORECAST_HOLD_MS, kForecastStats);   // shares the bottom baseline
WindPanel      windPanel;   // bottom-right, same baseline

#if NEWS_ENABLED
// Half the weather refresh interval (10 min), so the first headline fetch never
// lands in the burst of weather traffic at boot. Only a stagger: the network
// lock below is what actually guarantees one TLS session at a time.
static const uint32_t kNewsFirstFetchMs = 300000;
static const uint32_t kNewsOpenMs       = 400;   // clock shrink/grow duration

NewsApi    news(NEWS_COUNTRY, NEWS_MAX_ITEMS, NEWS_INTERVAL_MS, kNewsFirstFetchMs);
NewsTicker newsTicker(NEWS_DISPLAY_SPEED, kNewsOpenMs);

// Shared by every worker that opens a TLS connection. A live WiFiClientSecure
// costs tens of KB of heap and two at once would not fit.
static SemaphoreHandle_t netLock = nullptr;
#endif

static const int kBottomMargin = 8;   // gap from the bottom edge to the weather baseline

// On-board buttons. OneButton(pin, activeLow, pullupActive): both buttons pull
// to GND when pressed, so activeLow=true with the internal pull-up enabled.
OneButton      button1(PIN_BUTTON_1, true, true);
OneButton      button2(PIN_BUTTON_2, true, true);

BrightnessOverlay brightnessOverlay(2000);   // centred HUD, visible for 2 s

// Which page is on screen; the KEY button cycles through them in order. The
// clock is always present; each graph joins the cycle only if settings.ini
// enables it, so a disabled page is skipped rather than shown blank.
enum class View { Clock, Temperature, Pressure, Wind, Rain, Snow };

static const View kPages[] = {
    View::Clock,
#if ENABLED(GRAPH_TEMPERATURE)
    View::Temperature,
#endif
#if ENABLED(GRAPH_PRESSURE)
    View::Pressure,
#endif
#if ENABLED(GRAPH_WIND)
    View::Wind,
#endif
#if ENABLED(GRAPH_RAIN)
    View::Rain,
#endif
#if ENABLED(GRAPH_SNOW)
    View::Snow,
#endif
};
static const int kPageCount = sizeof(kPages) / sizeof(kPages[0]);

static int  pageIndex = 0;
static View currentView() { return kPages[pageIndex]; }

Interval fastTick(50);   // ~20 fps redraw while the forecast text is fading

static uint32_t loadedIconVersion = 0;
static int      weatherIconHeight = 22;   // set from the temperature font in setup()

// Minute-of-day of the last rendered frame (0..1439, or -1 when the time is
// unknown). Starts at -2 so the very first loop() always paints one frame.
static int       lastRenderedMinute = -2;

// Second-of-minute of the last rendered frame, tracked only when the colon
// blinks (that is the one thing on the clock page that changes within a minute).
static int       lastRenderedSecond = -2;

static WifiState lastRenderedState  = WifiState::Idle;
static bool      lastOverlayActive  = false;   // was the brightness HUD visible last frame
static View      lastRenderedView   = View::Clock;
static bool      lastNetBusy        = false;   // was the download arrow visible last frame

static void initDisplay()
{
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
}

static void initBacklight()
{
    pinMode(PIN_LCD_BL, OUTPUT);
    ledcSetup(kBacklightChannel, kBacklightFreq, kBacklightBits);
    ledcAttachPin(PIN_LCD_BL, kBacklightChannel);
    applyBacklight();            // start at the default brightness step
}

static void initSerialForDebug()
{
#ifdef DEBUG
    Serial.begin(115200);
    Serial.setTimeout(2000);
    while (!Serial) { }
#endif
}

static void initFrame()
{
    frame.setColorDepth(16);
    frame.createSprite(tft.width(), tft.height());
}

// True while anything is being fetched over WiFi: the weather worker (current
// conditions, forecast, icon), the news worker, or an NTP sync awaiting a reply.
static bool networkBusy()
{
    return weather.Busy()
        || ntp.Busy()
#if NEWS_ENABLED
        || news.Busy()
#endif
        ;
}

// WiFi signal strength bar graph, top-right; an X while the link is down. While
// a fetch is in flight a download arrow sits to its left.
static void drawStatusCluster()
{
    int barsX = frame.width() - kStatusMargin - kSignalWidth;
    int barsY = kStatusMargin + (kStatusHeight - kSignalHeight) / 2;

    if (wifi.IsConnected())
        signalBars.Render(&frame, barsX, barsY, wifi.SignalPercent());
    else
        signalBars.RenderNoLink(&frame, barsX, barsY);

    if (networkBusy() && DownIcon.Ready())
    {
        // Native size, so it fills the status row and stands a little proud of
        // the shorter bars.
        int iconX = barsX - kStatusGap - DownIcon.Width();
        int iconY = kStatusMargin + (kStatusHeight - DownIcon.Height()) / 2;
        DownIcon.Render(&frame, iconX, iconY);
    }
}

// The default page: live clock, date, weather, and status cluster.
static void renderClockView()
{
    // The vertical room the digits may use. Normally the whole sprite; while a
    // headline is on screen the ticker squeezes it to open a band underneath.
#if NEWS_ENABLED
    int clockTop    = newsTicker.ClockTop();
    int clockBottom = newsTicker.ClockBottom();
#else
    int clockTop    = 0;
    int clockBottom = frame.height();
#endif

    struct tm now;
    if (ntp.GetTime(now))
    {
        timePanel.Render(&frame, now, clockTop, clockBottom);   // large live clock
        datePanel.Render(&frame, now);                          // date, top-left
    }
    else
    {
        timePanel.RenderUnknown(&frame, clockTop, clockBottom); // placeholder until sync
    }

    // Bottom line: [icon][temperature] ... [rotating forecast] ... [wind].
    // The forecast is centred in the space between the temperature and the wind.
    // The baseline is anchored to the actual display height, not a fixed pixel row.
    int baseline      = frame.height() - kBottomMargin;
    int forecastLeft  = kStatusMargin;
    int forecastRight = frame.width() - kStatusMargin;
    if (weather.HasWeather())
    {
        forecastLeft  = weatherPanel.Render(&frame, weatherIcon, weather.Temperature(), baseline) + 12;
        forecastRight = windPanel.Render(&frame, frame.width() - kStatusMargin,
                                         weather.WindSpeed(), weather.WindDegree(), baseline) - 12;
    }
    forecastPanel.Render(&frame, weather, forecastLeft, forecastRight, baseline);

    drawStatusCluster();

#if NEWS_ENABLED
    if (newsTicker.BandVisible())
        newsTicker.RenderBand(&frame);   // between the shrunken digits and the weather
#endif
}

// Compose the current page offscreen, then blit it in one pass.
static void renderFrame()
{
    frame.fillSprite(TFT_BLACK);

    if (currentView() == View::Clock)
    {
        renderClockView();
    }
    else
    {
        struct tm now;
        bool haveTime = ntp.GetTime(now);
        const struct tm* today = haveTime ? &now : nullptr;

        switch (currentView())
        {
#if ENABLED(GRAPH_TEMPERATURE)
            case View::Temperature: drawTemperatureGraph(&frame, weather, today); break;
#endif
#if ENABLED(GRAPH_PRESSURE)
            case View::Pressure:    drawPressureGraph(&frame, weather, today);    break;
#endif
#if ENABLED(GRAPH_WIND)
            case View::Wind:        drawWindGraph(&frame, weather, today);        break;
#endif
#if ENABLED(GRAPH_RAIN)
            case View::Rain:        drawRainGraph(&frame, weather, today);        break;
#endif
#if ENABLED(GRAPH_SNOW)
            case View::Snow:        drawSnowGraph(&frame, weather, today);        break;
#endif
            default:                break;
        }
    }

    brightnessOverlay.Render(&frame);   // transient HUD, drawn on top of everything

    frame.pushSprite(0, 0);
}

// Bottom-left button: step the backlight up by 10%, wrapping 100% -> 10%.
static void onButton1Click()
{
    cycleBacklight();
    brightnessOverlay.Show(backlightPercent);   // reveal/refresh the HUD for 2 s
    DPRINTF("Backlight -> %d%%\n", backlightPercent);
}

// Bottom-right button: step to the next enabled page, wrapping to the clock.
static void onButton2Click()
{
    pageIndex = (pageIndex + 1) % kPageCount;
    DPRINTF("View -> %d\n", (int)currentView());
}

void setup()
{
    initDisplay();
    initBacklight();
    btStop();                    // no Bluetooth needed
    initSerialForDebug();
    initFrame();

    // Size the weather icon to the temperature digits' ascent. (The smooth font
    // reports a taller ascent than the seven-segment digit height, so the former
    // 6/5 multiplier overshot and left the icon standing proud of the digits.)
    frame.loadFont(cabin21);
    weatherIconHeight = frame.gFont.maxAscent;

    // Decode the embedded icons once. Globals cannot do this in their
    // constructors: it allocates, and the heap is not ready that early.
    DownIcon.Prepare();   // native size

    button1.attachClick(onButton1Click);
    button2.attachClick(onButton2Click);

    wifi.Begin();
    ntp.Begin();

#if NEWS_ENABLED
    netLock = xSemaphoreCreateMutex();
    weather.SetNetworkLock(netLock);
#endif

    weather.Begin();

#if NEWS_ENABLED
    news.Begin(netLock);
#endif
}

void loop()
{
    button1.tick();              // poll the buttons (non-blocking, millis-debounced)
    button2.tick();

    wifi.Update();               // non-blocking; keeps the link up
    ntp.Update(wifi.IsConnected());
    weather.Update(wifi.IsConnected());   // non-blocking; a worker task does the fetching

    // Decode a newly downloaded weather icon (once per change).
    if (weather.HasIcon() && weather.IconVersion() != loadedIconVersion)
    {
        if (weatherIcon.Load(weather.IconData(), weather.IconLength(), weatherIconHeight))
            loadedIconVersion = weather.IconVersion();
    }

    forecastPanel.Update();      // advance the rotating-stat fade state

#if NEWS_ENABLED
    news.Update(wifi.IsConnected());   // non-blocking; a worker task does the fetching
    newsTicker.Update(news);           // shrink/scroll/grow state
#endif

    // Current minute-of-day and second, or -1 while the clock has not synced yet.
    struct tm now;
    bool haveTime      = ntp.GetTime(now);
    int  minuteNow     = haveTime ? (now.tm_hour * 60 + now.tm_min) : -1;
    int  secondNow     = haveTime ? now.tm_sec : -1;
    bool minuteChanged = (minuteNow != lastRenderedMinute);
    bool stateChanged  = (wifi.State() != lastRenderedState);

    // The forecast fade and the news ticker both want ~20 fps. fastTick.Ready()
    // is consuming -- a true return restarts its window -- so it must be read
    // exactly once, or whichever animation tested it second would be starved of
    // half its frames.
    bool clockAnimating = (currentView() == View::Clock)
                       && (forecastPanel.Animating()
#if NEWS_ENABLED
                           || newsTicker.Animating()
#endif
                          );
    bool animTick = clockAnimating && fastTick.Ready();

    // A blinking colon is the only sub-minute change on the clock page, so it
    // needs its own repaint each second. CLOCK_BLINK_COLON is a compile-time
    // constant: with it off, this folds away and the minute cadence stands.
    bool blinkTick = CLOCK_BLINK_COLON
                  && (currentView() == View::Clock)
                  && (secondNow != lastRenderedSecond);

    // Repaint when the brightness HUD appears/updates (TakeDirty) or when it
    // expires (active -> inactive), so it is drawn and later cleared exactly once.
    bool overlayActive  = brightnessOverlay.Active();
    bool overlayChanged = brightnessOverlay.TakeDirty() || (overlayActive != lastOverlayActive);
    bool viewChanged    = (currentView() != lastRenderedView);

    // A fetch can begin and end between two minute rollovers, so the download
    // arrow needs a trigger of its own, or it would appear and clear late -- or,
    // with the colon not blinking, never be seen at all.
    bool netBusy        = networkBusy();
    bool netBusyChanged = (netBusy != lastNetBusy);

    // The clock only changes once a minute, so repaint on the minute rollover
    // and on WiFi state changes; also once a second while the colon blinks, at
    // ~20 fps while the forecast fades or a headline scrolls, and whenever the
    // page, the brightness HUD, or the download arrow changes.
    if (minuteChanged || stateChanged || animTick || overlayChanged || viewChanged
        || blinkTick || netBusyChanged)
    {
        renderFrame();
        lastRenderedMinute = minuteNow;
        lastRenderedSecond = secondNow;
        lastNetBusy        = netBusy;
        lastRenderedState  = wifi.State();
        lastOverlayActive  = overlayActive;
        lastRenderedView   = currentView();
    }
}
