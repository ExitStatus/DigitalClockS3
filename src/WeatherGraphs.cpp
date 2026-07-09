#include "WeatherGraphs.h"
#include "Config.h"
#include "HourlyGraph.h"

// Each graph is compiled in only when its settings.ini switch is on, along with
// the colour ramp and series picker it needs, so a disabled page costs nothing.

#define ANY_METRIC_GRAPH (ENABLED(GRAPH_TEMPERATURE) || ENABLED(GRAPH_PRESSURE) || \
                          ENABLED(GRAPH_RAIN) || ENABLED(GRAPH_SNOW))

// ---- Colour ramps: each metric maps its value to a colour via graphColourRamp ----
//
// Stops are authored in Celsius / mph / millibars; TEMP_STOP, WIND_STOP and
// PRESSURE_STOP convert them to whatever unit the series arrives in.

#if ENABLED(GRAPH_TEMPERATURE)
static uint16_t tempColour(TFT_eSprite* s, int t)
{
    static const ColourStop st[] = {
        { TEMP_STOP(-5),  60, 110, 235 }, { TEMP_STOP( 4),  45, 180, 220 },
        { TEMP_STOP(12),  70, 200, 110 }, { TEMP_STOP(20), 240, 205,  55 },
        { TEMP_STOP(27), 243, 146,  40 }, { TEMP_STOP(34), 240,  45,  45 },
    };
    return graphColourRamp(s, t, st, sizeof(st) / sizeof(st[0]));
}
#endif

#if ENABLED(GRAPH_PRESSURE)
static uint16_t pressureColour(TFT_eSprite* s, int p)
{
    static const ColourStop st[] = {
        { PRESSURE_STOP( 985), 90, 140, 235 }, { PRESSURE_STOP(1013), 80, 195, 120 },
        { PRESSURE_STOP(1035), 240, 190, 60 },
    };
    return graphColourRamp(s, p, st, sizeof(st) / sizeof(st[0]));
}
#endif

#if ENABLED(GRAPH_WIND)
// Wind and gust are two lines on one graph, so each gets a distinct solid
// colour (value-independent) rather than a speed gradient.
static uint16_t windLineColour(TFT_eSprite* s, int) { return s->color565(80, 180, 240); }   // cyan
static uint16_t gustLineColour(TFT_eSprite* s, int) { return s->color565(243, 150, 60); }   // amber
#endif

#if ENABLED(GRAPH_RAIN)
static uint16_t rainColour(TFT_eSprite* s, int pct)
{
    static const ColourStop st[] = {
        { 0, 120, 140, 170 }, { 50, 70, 130, 225 }, { 100, 40, 90, 225 },
    };
    return graphColourRamp(s, pct, st, sizeof(st) / sizeof(st[0]));
}
#endif

#if ENABLED(GRAPH_SNOW)
static uint16_t snowColour(TFT_eSprite* s, int pct)
{
    static const ColourStop st[] = {
        { 0, 130, 140, 155 }, { 50, 200, 215, 235 }, { 100, 240, 248, 255 },
    };
    return graphColourRamp(s, pct, st, sizeof(st) / sizeof(st[0]));
}
#endif

// ---- Extract the chosen hourly field into a flat series, then render it ----

#if ANY_METRIC_GRAPH
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
#endif

#if ENABLED(GRAPH_TEMPERATURE)
static int pickTemp(const DayForecast& d, int i) { return d.hourTemp[i]; }

void drawTemperatureGraph(TFT_eSprite* s, const WeatherApi& w, const struct tm* today)
{
    GraphSpec spec = { "Predicted Temperature", TEMP_UNIT_LABEL, true, 22, 1, 1,
                       false, false, 0, 1, 0, tempColour };
    renderMetric(s, w, today, spec, pickTemp);
}
#endif

#if ENABLED(GRAPH_PRESSURE)
static int pickPressure(const DayForecast& d, int i) { return d.hourPressure[i]; }

void drawPressureGraph(TFT_eSprite* s, const WeatherApi& w, const struct tm* today)
{
    GraphSpec spec = { "Predicted Air Pressure", PRESSURE_UNIT_LABEL, false,
                       PRESSURE_PLOT_LEFT, PRESSURE_DELTA(2), PRESSURE_DELTA(2),
                       false, false, 0, PRESSURE_SCALE, PRESSURE_DECIMALS, pressureColour };
    renderMetric(s, w, today, spec, pickPressure);
}
#endif

#if ENABLED(GRAPH_WIND)
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

    GraphSpec spec = { "Predicted Wind Speed", WIND_UNIT_LABEL, false, 22, 1, 2,
                       true, false, 0, 1, 0, windLineColour };
    renderHourlyGraph(s, wind, count, n0, n1, today, spec, gust, gustLineColour, "Wind", "Gust");
}
#endif

#if ENABLED(GRAPH_RAIN)
static int pickRain(const DayForecast& d, int i) { return d.hourRain[i]; }

void drawRainGraph(TFT_eSprite* s, const WeatherApi& w, const struct tm* today)
{
    GraphSpec spec = { "Predicted Chance of Rain", "%", false, 28, 0, 5,
                       true, true, 100, 1, 0, rainColour };
    renderMetric(s, w, today, spec, pickRain);
}
#endif

#if ENABLED(GRAPH_SNOW)
static int pickSnow(const DayForecast& d, int i) { return d.hourSnow[i]; }

void drawSnowGraph(TFT_eSprite* s, const WeatherApi& w, const struct tm* today)
{
    GraphSpec spec = { "Predicted Chance of Snow", "%", false, 28, 0, 5,
                       true, true, 100, 1, 0, snowColour };
    renderMetric(s, w, today, spec, pickSnow);
}
#endif
