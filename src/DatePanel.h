#ifndef _DATE_PANEL_H
#define _DATE_PANEL_H

#include <TFT_eSPI.h>
#include <time.h>

// Draws a date in "03 January, 2026" form using the embedded Gill Sans 20 font.
// Anchored middle-left: x is the left edge, y is the vertical centre line.
class DatePanel
{
    public:
        DatePanel(int x, int y);

        void Render(TFT_eSprite* sprite, const struct tm& time);

    private:
        int _x;
        int _y;
};

#endif // _DATE_PANEL_H
