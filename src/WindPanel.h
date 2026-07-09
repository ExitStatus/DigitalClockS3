#ifndef _WIND_PANEL_H
#define _WIND_PANEL_H

#include <TFT_eSPI.h>

// Draws the current wind at the bottom-right: a direction arrow (coloured by
// speed: green < 10 mph, orange at 25 mph, red > 40 mph) followed by the speed
// in mph, right-aligned to a given edge.
class WindPanel
{
    public:
        WindPanel();

        // Right-align the block to rightX on baseline baselineY (both passed at
        // render time); returns the left edge x of what was drawn.
        int Render(TFT_eSprite* sprite, int rightX, float windMph, int windDegree, int baselineY);

    private:
        uint16_t colourForSpeed(TFT_eSprite* sprite, float mph) const;
        void drawArrow(TFT_eSprite* sprite, int cx, int cy, float bearingDeg, int radius, uint16_t colour) const;
};

#endif // _WIND_PANEL_H
