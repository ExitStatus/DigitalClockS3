#ifndef _WEATHER_ICON_H
#define _WEATHER_ICON_H

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "Icon.h"

// The downloaded weather condition icon.
//
// A thin wrapper over Icon, which does the work: decode, trim the transparent
// border, scale to the target height, and blend over the destination. This
// exists to name the thing and to hold the one Icon the weather panel draws.
class WeatherIcon
{
    public:
        explicit WeatherIcon(TFT_eSPI* tft);

        // Decode + trim + resize so the icon is targetHeight pixels tall. The
        // png buffer need only stay alive for the duration of this call.
        bool Load(const uint8_t* png, size_t len, int targetHeight);
        void Render(TFT_eSprite* dest, int x, int y);

        bool Ready() const  { return _icon.Ready(); }
        int  Width() const  { return _icon.Width(); }
        int  Height() const { return _icon.Height(); }

    private:
        Icon _icon;
};

#endif // _WEATHER_ICON_H
