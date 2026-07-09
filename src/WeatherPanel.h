#ifndef _WEATHER_PANEL_H
#define _WEATHER_PANEL_H

#include <TFT_eSPI.h>
#include <Arduino.h>

#include "WeatherIcon.h"

// Draws the weather icon (left) and the current temperature (with a drawn degree
// ring) to its right, on a bottom-left baseline anchor.
class WeatherPanel
{
    public:
        WeatherPanel(int x, int baselineY);

        // Returns the x coordinate of the right edge of what was drawn.
        int Render(TFT_eSprite* sprite, WeatherIcon& icon, float tempC);

    private:
        int _x;
        int _y;   // text baseline

        static const int kIconGap = 6;   // gap between the icon and the temperature
};

#endif // _WEATHER_PANEL_H
