#ifndef _HOURLY_GRAPH_H
#define _HOURLY_GRAPH_H

#include <TFT_eSPI.h>
#include <time.h>

// Self-contained module for drawing a 2-day (today + tomorrow) hourly line
// graph: title, dated day headers, Y-axis scale, hour ticks, smooth line(s) and
// an optional legend. Weather-agnostic - callers supply the data, colours and
// labels via a GraphSpec.

// A colour-ramp stop: at value 'v' the colour is (r,g,b). Stops must ascend by v.
struct ColourStop { int v; uint8_t r, g, b; };

// Interpolate a 565 colour for value x across an ascending array of n stops.
uint16_t graphColourRamp(TFT_eSprite* sprite, int x, const ColourStop* stops, int n);

// Draw a centred single-line message (e.g. "Forecast unavailable").
void drawGraphMessage(TFT_eSprite* sprite, const char* text);

// Everything that distinguishes one graph from another.
struct GraphSpec
{
    const char* title;        // top-of-screen heading
    const char* unit;         // Y-axis unit text (e.g. "mb"); drawn beside the
                              // ring, rather than above the axis, if degreeUnit
    bool        degreeUnit;   // draw a degree ring before the unit text
    int         plotLeft;     // left gutter width (room for the Y labels)
    int         padLo;        // loScale = min - padLo   (unless zeroBase)
    int         padHi;        // hiScale = max + padHi
    bool        floorZero;    // clamp loScale to >= 0
    bool        zeroBase;     // force loScale = 0 (for 0..100% series)
    int         capHi;        // clamp hiScale to this if > 0 (e.g. 100 for %)

    // Series values are integers, so a quantity needing decimals is carried
    // pre-multiplied (e.g. inHg in hundredths). The axis labels divide by
    // valueScale and print valueDecimals places; the series itself is untouched.
    int         valueScale;     // 1 for none
    int         valueDecimals;  // ignored when valueScale is 1

    uint16_t  (*colourFn)(TFT_eSprite*, int);
};

// Renders a titled, axed, smooth line graph of the hourly series 'vals'
// (count = n0 today + n1 tomorrow) into the sprite. 'today' labels the two day
// regions (nullptr -> "Today"/"Tomorrow"). The caller clears the sprite first.
//
// Optionally overlays a second series 'vals2' (same count) drawn with colourFn2;
// when label1/label2 are given a small legend is shown. The Y-scale then spans
// both series.
void renderHourlyGraph(TFT_eSprite* sprite, const int* vals, int count,
                       int n0, int n1, const struct tm* today, const GraphSpec& spec,
                       const int* vals2 = nullptr,
                       uint16_t (*colourFn2)(TFT_eSprite*, int) = nullptr,
                       const char* label1 = nullptr, const char* label2 = nullptr);

#endif // _HOURLY_GRAPH_H
