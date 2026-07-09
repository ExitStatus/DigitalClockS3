#ifndef _TIME_PANEL_H
#define _TIME_PANEL_H

#include <TFT_eSPI.h>
#include <time.h>

#include "SevenSegment.h"

// Draws the time as seven-segment digits: a large HH:MM. In 12-hour mode the
// hour's leading zero is suppressed and AM/PM is drawn as a top-aligned
// superscript to the right; in 24-hour mode the hour is zero-padded and there is
// no superscript, so HH:MM centres on its own.
//
// Every segment is drawn every time: lit ones in activeColour, unlit ones in
// inactiveColour, which is what gives the digits their "ghost" outline. Both are
// 16-bit 5-6-5, as produced by Config.h's RGB565(). The AM/PM superscript takes
// the active colour too.
//
// With blinkColon set, the colon lights on even seconds and goes dark on odd
// ones. It follows the seconds it is handed rather than a timer of its own, so
// the caller must repaint every second for the blink to be seen.
class TimePanel
{
    public:
        TimePanel(bool use24Hour, bool blinkColon,
                  uint16_t activeColour, uint16_t inactiveColour);

        void Render(TFT_eSprite* sprite, const struct tm& time);
        void RenderUnknown(TFT_eSprite* sprite);   // ghost placeholder before the clock syncs

    private:
        void draw(TFT_eSprite* sprite,
                  int h0, int h1, int m0, int m1,
                  const char* ampm, bool colonLit);

        bool colonLit(const struct tm& time) const;

        SevenSegment _big;     // HH:MM
        bool _use24Hour;
        bool _blinkColon;
        uint16_t _active;      // lit segments
        uint16_t _inactive;    // unlit segments
};

#endif // _TIME_PANEL_H
