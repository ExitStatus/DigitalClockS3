#ifndef _FORECAST_PANEL_H
#define _FORECAST_PANEL_H

#include <TFT_eSPI.h>

#include "WeatherApi.h"

// Cycles through four forecast stats on one line, fading each in, holding it,
// then fading it out. Text is horizontally centred within the region it is given
// and drawn on a shared baseline. Fade/hold durations come from the caller.
class ForecastPanel
{
    public:
        ForecastPanel(int baselineY, uint32_t fadeMs, uint32_t holdMs);

        void Update();                 // advance the cycle/fade state
        bool Animating() const;        // true while fading (needs frequent redraws)

        void Render(TFT_eSprite* sprite, const WeatherApi& weather,
                    int leftBound, int rightBound);

    private:
        int  alphaNow() const;         // current fade level, 0..255
        // Draw "<label> <value><unit>" centred. If degree is true, unit is
        // ignored and a degree ring + 'C' is drawn instead.
        void drawStat(TFT_eSprite* sprite, int centerX, const char* label,
                      const String& value, const char* unit, bool degree, uint16_t colour);

        int      _y;
        uint32_t _fadeMs;
        uint32_t _holdMs;
        uint32_t _cycleMs;             // 2*fade + hold

        int      _index = 0;           // which of the 4 stats
        uint32_t _start = 0;           // millis() when the current stat began
        bool     _started = false;

        static const int kStatCount = 3;
};

#endif // _FORECAST_PANEL_H
