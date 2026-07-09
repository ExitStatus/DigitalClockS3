#include "WeatherGraphs.h"
#include "HourlyGraph.h"

// ---- Colour ramps: each metric maps its value to a colour via graphColourRamp ----

static uint16_t tempColour(TFT_eSprite* s, int t)
{
    static const ColourStop st[] = {
        { -5,  60, 110, 235 }, {  4,  45, 180, 220 }, { 12,  70, 200, 110 },
        { 20, 240, 205,  55 }, { 27, 243, 146,  40 }, { 34, 240,  45,  45 },
    };
    return graphColourRamp(s, t, st, sizeof(st) / sizeof(st[0]));
}

static uint16_t pressureColour(TFT_eSprite* s, int p)
{
    static const ColourStop st[] = {
        { 985, 90, 140, 235 }, { 1013, 80, 195, 120 }, { 1035, 240, 190, 60 },
    };
    return graphColourRamp(s, p, st, sizeof(st) / sizeof(st[0]));
}

// Wind and gust are two lines on one graph, so each gets a distinct solid
// colour (value-independent) rather than a speed gradient.
static uint16_t windLineColour(TFT_eSprite* s, int) { return s->color565(80, 180, 240); }   // cyan
static uint16_t gustLineColour(TFT_eSprite* s, int) { return s->color565(243, 150, 60); }   // amber

static uint16_t rainColour(TFT_eSprite* s, int pct)
{
    static const ColourStop st[] = {
        { 0, 120, 140, 170 }, { 50, 70, 130, 225 }, { 100, 40, 90, 225 },
    };
    return graphColourRamp(s, pct, st, sizeof(st) / sizeof(st[0]));
}

static uint16_t snowColour(TFT_eSprite* s, int pct)
{
    static const ColourStop st[] = {
        { 0, 130, 140, 155 }, { 50, 200, 215, 235 }, { 100, 240, 248, 255 },
    };
    return graphColourRamp(s, pct, st, sizeof(st) / sizeof(st[0]));
}

// ---- Extract the chosen hourly field into a flat series, then render it ----

static void renderMetric(TFT_eSprite* s, const WeatherApi& w, const struct tm* today,
                         const GraphSpec& spec, int (*pick)(const DayForecast&, int))
{
    if (!w.HasForecast())
    {
        drawGraphMessage(s, "Forecast unavailable");
        return;
    }

    const DayForecast& d0 = w.Today();
    const DayForecast& d1 = w.Tomorrow();
    int n0 = d0.hourCount, n1 = d1.hourCount, total = n0 + n1;
    if (total <= 0)
        return;

    int vals[48];
    int count = (total < 48) ? total : 48;
    for (int i = 0; i < count; i++)
        vals[i] = (i < n0) ? pick(d0, i) : pick(d1, i - n0);

    renderHourlyGraph(s, vals, count, n0, n1, today, spec);
}

static int pickTemp(const DayForecast& d, int i)     { return d.hourTemp[i]; }
static int pickPressure(const DayForecast& d, int i) { return d.hourPressure[i]; }
static int pickRain(const DayForecast& d, int i)     { return d.hourRain[i]; }
static int pickSnow(const DayForecast& d, int i)     { return d.hourSnow[i]; }

void drawTemperatureGraph(TFT_eSprite* s, const WeatherApi& w, const struct tm* today)
{
    GraphSpec spec = { "Predicted Temperature", "", true, 22, 1, 1, false, false, 0, tempColour };
    renderMetric(s, w, today, spec, pickTemp);
}

void drawPressureGraph(TFT_eSprite* s, const WeatherApi& w, const struct tm* today)
{
    GraphSpec spec = { "Predicted Air Pressure", "mb", false, 36, 2, 2, false, false, 0, pressureColour };
    renderMetric(s, w, today, spec, pickPressure);
}

void drawWindGraph(TFT_eSprite* s, const WeatherApi& w, const struct tm* today)
{
    if (!w.HasForecast())
    {
        drawGraphMessage(s, "Forecast unavailable");
        return;
    }

    const DayForecast& d0 = w.Today();
    const DayForecast& d1 = w.Tomorrow();
    int n0 = d0.hourCount, n1 = d1.hourCount, total = n0 + n1;
    if (total <= 0)
        return;

    int wind[48], gust[48];
    int count = (total < 48) ? total : 48;
    for (int i = 0; i < count; i++)
    {
        const DayForecast& d = (i < n0) ? d0 : d1;
        int j = (i < n0) ? i : i - n0;
        wind[i] = d.hourWind[j];
        gust[i] = d.hourGust[j];
    }

    GraphSpec spec = { "Predicted Wind Speed", "mph", false, 22, 1, 2, true, false, 0, windLineColour };
    renderHourlyGraph(s, wind, count, n0, n1, today, spec, gust, gustLineColour, "Wind", "Gust");
}

void drawRainGraph(TFT_eSprite* s, const WeatherApi& w, const struct tm* today)
{
    GraphSpec spec = { "Predicted Chance of Rain", "%", false, 28, 0, 5, true, true, 100, rainColour };
    renderMetric(s, w, today, spec, pickRain);
}

void drawSnowGraph(TFT_eSprite* s, const WeatherApi& w, const struct tm* today)
{
    GraphSpec spec = { "Predicted Chance of Snow", "%", false, 28, 0, 5, true, true, 100, snowColour };
    renderMetric(s, w, today, spec, pickSnow);
}
