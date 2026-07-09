#ifndef _FORECAST_PANEL_H
#define _FORECAST_PANEL_H

#include <TFT_eSPI.h>

#include "WeatherApi.h"

// The rotating bottom-line stats, as a bitmask: the caller enables the ones it
// wants and the panel cycles through those only.
enum ForecastStat : uint8_t
{
    StatMaxTemp = 1 << 0,
    StatRain    = 1 << 1,
    StatWind    = 1 << 2,
};

// Cycles through the enabled forecast stats on one line, fading each in, holding
// it, then fading it out. Text is horizontally centred within the region it is
// given and drawn on a shared baseline. Fade/hold durations and the set of stats
// come from the caller; with no stats enabled the panel draws nothing and never
// reports itself as animating.
class ForecastPanel
{
    public:
        ForecastPanel(uint32_t fadeMs, uint32_t holdMs, uint8_t statMask);

        void Update();                 // advance the cycle/fade state
        bool Animating() const;        // true while fading (needs frequent redraws)

        void Render(TFT_eSprite* sprite, const WeatherApi& weather,
                    int leftBound, int rightBound, int baselineY);

    private:
        int  alphaNow() const;         // current fade level, 0..255
        // Draw "<label> <value><unit>" centred. If degree is true, unit is
        // ignored and a degree ring + the temperature unit letter is drawn instead.
        void drawStat(TFT_eSprite* sprite, int centerX, const char* label,
                      const String& value, const char* unit, bool degree, uint16_t colour);

        int      _y;
        uint32_t _fadeMs;
        uint32_t _holdMs;
        uint32_t _cycleMs;             // 2*fade + hold

        uint8_t  _stats[3];            // the enabled stats, in display order
        uint8_t  _count = 0;           // how many of _stats are in use
        int      _index = 0;           // which of _stats is showing
        uint32_t _start = 0;           // millis() when the current stat began
        bool     _started = false;
};

#endif // _FORECAST_PANEL_H
