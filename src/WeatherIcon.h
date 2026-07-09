#ifndef _WEATHER_ICON_H
#define _WEATHER_ICON_H

#include <Arduino.h>
#include <TFT_eSPI.h>

// Decodes a downloaded weather PNG, trims its transparent border, and scales the
// remaining content to a target height (preserving aspect ratio) into a sprite,
// composited on black. The sprite can then be blitted into a frame.
class WeatherIcon
{
    public:
        explicit WeatherIcon(TFT_eSPI* tft);

        // Decode + trim + resize so the icon is targetHeight pixels tall.
        bool Load(const uint8_t* png, size_t len, int targetHeight);
        void Render(TFT_eSprite* dest, int x, int y);

        bool Ready() const  { return _ready; }
        int  Width() const  { return _w; }
        int  Height() const { return _h; }

    private:
        TFT_eSprite _sprite;
        bool _ready = false;
        int  _w = 0;
        int  _h = 0;
};

#endif // _WEATHER_ICON_H
