#ifndef _SIGNAL_BARS_H
#define _SIGNAL_BARS_H

#include <TFT_eSPI.h>

// A small signal-strength bar graph. Bars rise in height left-to-right; the
// number of filled bars reflects strength and the remainder are drawn as hollow
// outlines. The whole graph is tinted along a red (weak) -> orange (mid) ->
// green (strong) ramp. Purely presentational: pass it a 0..100 percentage.
//
// With no link at all there is no strength to show, and all-hollow bars read as
// "0%" rather than "offline". RenderNoLink() draws an X in the same footprint
// instead, so the two cases cannot be confused.
class SignalBars
{
    public:
        SignalBars(int width, int height, int barCount = 4);

        void Render(TFT_eSprite* sprite, int x, int y, int percent);
        void RenderNoLink(TFT_eSprite* sprite, int x, int y);

    private:
        int filledBars(int percent) const;
        uint16_t colourFor(TFT_eSprite* sprite, int percent) const;

        int _width;
        int _height;
        int _barCount;

        static const int kGap    = 2;   // pixels between bars
        static const int kStroke = 2;   // line thickness of the no-link X
};

#endif // _SIGNAL_BARS_H
