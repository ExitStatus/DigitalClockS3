#ifndef _TIME_PANEL_H
#define _TIME_PANEL_H

#include <TFT_eSPI.h>
#include <time.h>

#include "SevenSegment.h"

// Draws the time as seven-segment digits: a large HH:MM with AM/PM as a
// top-aligned superscript to the right. Active segments are red, inactive
// segments dark grey (so the whole display keeps its "ghost" outline).
class TimePanel
{
    public:
        TimePanel();

        void Render(TFT_eSprite* sprite, const struct tm& time);
        void RenderUnknown(TFT_eSprite* sprite);   // ghost placeholder before the clock syncs

    private:
        void draw(TFT_eSprite* sprite,
                  int h0, int h1, int m0, int m1,
                  const char* ampm, bool known);

        SevenSegment _big;     // HH:MM
};

#endif // _TIME_PANEL_H
