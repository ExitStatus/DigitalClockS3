#include "HourlyGraph.h"
#include "Font.h"
#include <math.h>

// ---- Colour ramp ----------------------------------------------------------

uint16_t graphColourRamp(TFT_eSprite* s, int x, const ColourStop* st, int n)
{
    if (x <= st[0].v)     return s->color565(st[0].r, st[0].g, st[0].b);
    if (x >= st[n - 1].v) return s->color565(st[n - 1].r, st[n - 1].g, st[n - 1].b);
    for (int i = 1; i < n; i++)
    {
        if (x <= st[i].v)
        {
            float f = (float)(x - st[i - 1].v) / (st[i].v - st[i - 1].v);
            uint8_t r = st[i - 1].r + (int)((st[i].r - st[i - 1].r) * f);
            uint8_t g = st[i - 1].g + (int)((st[i].g - st[i - 1].g) * f);
            uint8_t b = st[i - 1].b + (int)((st[i].b - st[i - 1].b) * f);
            return s->color565(r, g, b);
        }
    }
    return s->color565(st[n - 1].r, st[n - 1].g, st[n - 1].b);
}

// ---- Centred message ------------------------------------------------------

void drawGraphMessage(TFT_eSprite* s, const char* text)
{
    s->loadFont(gillsans20);
    s->setTextColor(TFT_WHITE, TFT_BLACK);
    s->setTextDatum(MC_DATUM);
    s->drawString(text, s->width() / 2, s->height() / 2);
}

// ---- Smooth line (Catmull-Rom spline) -------------------------------------

// Catmull-Rom interpolation of one value between p1 and p2, with p0/p3 as the
// surrounding neighbours; t in [0,1].
static float catmull(float p0, float p1, float p2, float p3, float t)
{
    float t2 = t * t, t3 = t2 * t;
    return 0.5f * ((2.0f * p1)
                 + (-p0 + p2) * t
                 + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2
                 + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

// Draws a smooth 2 px line through the series, mapping index -> x across plotW
// and value -> y within [loScale, hiScale]. y is clamped to the plot so spline
// overshoot never spills outside it.
static void drawSmoothSeries(TFT_eSprite* s, const int* values, int count,
                             int plotLeft, int plotBottom, int plotW, int plotH,
                             int loScale, int hiScale,
                             uint16_t (*colourFn)(TFT_eSprite*, int))
{
    if (count < 2)
        return;

    int span  = count - 1;
    int denom = hiScale - loScale;
    if (denom == 0) denom = 1;
    int plotTop = plotBottom - plotH;

    const int STEPS = 6;                 // sub-samples per hour segment
    int totalSteps = span * STEPS;
    int prevX = -1, prevY = -1;

    for (int step = 0; step <= totalSteps; step++)
    {
        float fi = (float)step / STEPS;  // fractional index, 0 .. span
        int   i  = (int)fi;
        if (i > span) i = span;
        float t = fi - i;

        float p0 = values[i > 0 ? i - 1 : 0];
        float p1 = values[i];
        float p2 = values[i < span ? i + 1 : span];
        float p3 = values[i + 2 <= span ? i + 2 : span];
        float vf = (i < span) ? catmull(p0, p1, p2, p3, t) : values[span];

        int x = plotLeft + (int)(fi * plotW / span);
        int y = plotBottom - (int)((vf - loScale) * plotH / denom);
        if (y < plotTop)    y = plotTop;
        if (y > plotBottom) y = plotBottom;

        if (prevX >= 0)
        {
            uint16_t c = colourFn(s, (int)lroundf(vf));
            s->drawLine(prevX, prevY,     x, y,     c);
            s->drawLine(prevX, prevY + 1, x, y + 1, c);   // 2 px thick
        }
        prevX = x;
        prevY = y;
    }
}

// ---- Axis helper ----------------------------------------------------------

// One Y-axis label, honouring the spec's fixed-point scale.
static String axisLabel(int v, const GraphSpec& spec)
{
    if (spec.valueScale <= 1)
        return String(v);

    char b[16];
    snprintf(b, sizeof(b), "%.*f", spec.valueDecimals, (double)v / spec.valueScale);
    return String(b);
}

// Round up to a "nice" axis step (1,2,5,10,...) giving roughly targetDivs divisions.
static int niceStep(int range, int targetDivs)
{
    if (range < 1) range = 1;
    int raw = range / targetDivs;
    if (raw < 1) raw = 1;
    const int steps[] = { 1, 2, 5, 10, 15, 20, 25, 50 };
    for (unsigned i = 0; i < sizeof(steps) / sizeof(steps[0]); i++)
        if (steps[i] >= raw) return steps[i];
    return 100;
}

void renderHourlyGraph(TFT_eSprite* s, const int* vals, int count,
                       int n0, int n1, const struct tm* today, const GraphSpec& spec,
                       const int* vals2, uint16_t (*colourFn2)(TFT_eSprite*, int),
                       const char* label1, const char* label2)
{
    if (count <= 0)
        return;

    // Value range across both days (and the second series, if any), per the
    // spec's padding / floor rules.
    int vmin = 100000, vmax = -100000;
    for (int i = 0; i < count; i++) { int v = vals[i]; if (v < vmin) vmin = v; if (v > vmax) vmax = v; }
    if (vals2)
        for (int i = 0; i < count; i++) { int v = vals2[i]; if (v < vmin) vmin = v; if (v > vmax) vmax = v; }

    int loScale = spec.zeroBase ? 0 : (vmin - spec.padLo);
    int hiScale = vmax + spec.padHi;
    if (spec.floorZero && loScale < 0) loScale = 0;
    if (spec.capHi > 0 && hiScale > spec.capHi) hiScale = spec.capHi;
    if (hiScale <= loScale) hiScale = loScale + 1;   // guard against divide-by-zero

    // Plot area. plotTop is a fixed top band (title + date labels); the rest is
    // derived from the sprite size, so the graph tracks the display resolution.
    const int plotLeft   = spec.plotLeft;
    const int plotRight  = s->width() - 6;
    const int plotTop    = 40;
    const int plotBottom = s->height() - 22;   // room below for the hour labels
    const int plotW      = plotRight - plotLeft;
    const int plotH      = plotBottom - plotTop;

    uint16_t axisCol = s->color565(120, 120, 124);
    uint16_t gridCol = s->color565(40, 40, 44);

    // --- Y axis: gridlines + labels (drawn behind the line) ---
    int step = niceStep(hiScale - loScale, 4);
    int firstTick = (int)ceil((double)loScale / step) * step;
    s->loadFont(gillsans12);
    s->setTextColor(s->color565(150, 150, 150), TFT_BLACK);
    s->setTextDatum(MR_DATUM);
    for (int v = firstTick; v <= hiScale; v += step)
    {
        int y = plotBottom - (v - loScale) * plotH / (hiScale - loScale);
        if (y < plotTop) continue;
        s->drawFastHLine(plotLeft, y, plotW, gridCol);
        s->drawString(axisLabel(v, spec), plotLeft - 3, y);
    }

    // --- Smooth line(s) through the series ---
    int span = (count > 1) ? (count - 1) : 1;
    drawSmoothSeries(s, vals, count, plotLeft, plotBottom, plotW, plotH,
                     loScale, hiScale, spec.colourFn);
    if (vals2 && colourFn2)
        drawSmoothSeries(s, vals2, count, plotLeft, plotBottom, plotW, plotH,
                         loScale, hiScale, colourFn2);

    // --- Axis lines on top of the line ---
    s->drawFastVLine(plotLeft, plotTop, plotH, axisCol);
    s->drawFastHLine(plotLeft, plotBottom, plotW, axisCol);

    // Dotted divider between today and tomorrow
    int divX = plotLeft + (int)((n0 - 0.5f) * plotW / span);
    for (int y = plotTop; y < plotBottom; y += 4)
        s->drawFastVLine(divX, y, 2, s->color565(90, 90, 96));

    // --- X axis: hour-of-day ticks + labels every 6 h ---
    s->loadFont(gillsans12);
    s->setTextColor(s->color565(150, 150, 150), TFT_BLACK);
    s->setTextDatum(TC_DATUM);
    for (int d = 0; d < 2; d++)
    {
        int dayStart = (d == 0) ? 0 : n0;
        int nd       = (d == 0) ? n0 : n1;
        for (int hr = 0; hr <= 18; hr += 6)
        {
            if (hr >= nd) break;
            int x = plotLeft + (dayStart + hr) * plotW / span;
            s->drawFastVLine(x, plotBottom + 1, 3, axisCol);
            char hb[4];
            snprintf(hb, sizeof(hb), "%02d", hr);
            s->drawString(hb, x, plotBottom + 5);
        }
    }

    // Build each day's label from the date (e.g. "Wed 09 Jul").
    String dayLabel0 = "Today";
    String dayLabel1 = "Tomorrow";
    if (today)
    {
        char b0[16];
        strftime(b0, sizeof(b0), "%a %d %b", today);
        dayLabel0 = b0;

        struct tm t1 = *today;
        t1.tm_hour = 12;
        t1.tm_mday += 1;
        mktime(&t1);
        char b1[16];
        strftime(b1, sizeof(b1), "%a %d %b", &t1);
        dayLabel1 = b1;
    }

    // Title across the very top.
    s->loadFont(gillsans16);
    s->setTextColor(TFT_WHITE, TFT_BLACK);
    s->setTextDatum(TC_DATUM);
    s->drawString(spec.title, s->width() / 2, 2);

    // Date labels centred over each day's region.
    s->loadFont(gillsans14);
    s->setTextColor(s->color565(200, 200, 200), TFT_BLACK);
    s->setTextDatum(TC_DATUM);
    int dateY = 21;
    s->drawString(dayLabel0, plotLeft + (int)(((n0 - 1) / 2.0f) * plotW / span), dateY);
    s->drawString(dayLabel1, plotLeft + (int)(((n0 + count - 1) / 2.0f) * plotW / span), dateY);

    // Y-axis unit marker, top-left of the axis.
    s->loadFont(gillsans12);
    s->setTextColor(s->color565(150, 150, 150), TFT_BLACK);
    s->setTextDatum(L_BASELINE);
    if (spec.degreeUnit)
    {
        s->drawString(spec.unit, plotLeft - 10, 34);
        s->drawCircle(plotLeft - 14, 34 - s->gFont.maxAscent + 3, 2, s->color565(150, 150, 150));
    }
    else
    {
        s->drawString(spec.unit, 2, 34);
    }

    // Two-series legend, top-right inside the plot.
    if (vals2 && colourFn2 && label1 && label2)
    {
        const int lw = 74, lh = 30;
        int lx = plotRight - lw - 2;
        int ly = plotTop + 3;
        uint16_t boxBg = s->color565(24, 24, 26);
        uint16_t c1 = spec.colourFn(s, 0);
        uint16_t c2 = colourFn2(s, 0);

        s->fillRect(lx, ly, lw, lh, boxBg);
        s->drawRect(lx, ly, lw, lh, s->color565(70, 70, 74));

        s->loadFont(gillsans12);
        s->setTextDatum(TL_DATUM);
        s->fillRect(lx + 6, ly + 8, 14, 3, c1);
        s->setTextColor(c1, boxBg);
        s->drawString(label1, lx + 24, ly + 3);
        s->fillRect(lx + 6, ly + 21, 14, 3, c2);
        s->setTextColor(c2, boxBg);
        s->drawString(label2, lx + 24, ly + 16);
    }
}
