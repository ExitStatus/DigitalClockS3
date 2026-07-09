#ifndef _SIGNAL_BARS_H
#define _SIGNAL_BARS_H

#include <TFT_eSPI.h>

// A small signal-strength bar graph. Bars rise in height left-to-right; the
// number of filled bars reflects strength and the remainder are drawn as hollow
// outlines. The whole graph is tinted along a red (weak) -> orange (mid) ->
// green (strong) ramp. Purely presentational: pass it a 0..100 percentage.
class SignalBars
{
    public:
        SignalBars(int width, int height, int barCount = 4);

        void Render(TFT_eSprite* sprite, int x, int y, int percent);

    private:
        int filledBars(int percent) const;
        uint16_t colourFor(TFT_eSprite* sprite, int percent) const;

        int _width;
        int _height;
        int _barCount;

        static const int kGap = 2;   // pixels between bars
};

#endif // _SIGNAL_BARS_H
