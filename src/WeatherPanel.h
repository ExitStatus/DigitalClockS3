#ifndef _WEATHER_PANEL_H
#define _WEATHER_PANEL_H

#include <TFT_eSPI.h>
#include <Arduino.h>

#include "WeatherIcon.h"

// Draws the weather icon (left) and the current temperature (with a drawn degree
// ring) to its right, on a bottom-left baseline anchor. The degree letter comes
// from the configured temperature unit.
class WeatherPanel
{
    public:
        explicit WeatherPanel(int x);

        // Draws on the bottom baseline 'baselineY' (passed at render time so it
        // tracks the actual display height). Returns the right edge x of the block.
        int Render(TFT_eSprite* sprite, WeatherIcon& icon, float temp, int baselineY);

    private:
        int _x;

        static const int kIconGap = 6;   // gap between the icon and the temperature
};

#endif // _WEATHER_PANEL_H
