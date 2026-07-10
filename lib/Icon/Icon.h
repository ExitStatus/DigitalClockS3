#ifndef _ICON_H
#define _ICON_H

#include <Arduino.h>
#include <stdlib.h>
#include <PNGdec.h>
#include <TFT_eSPI.h>

// A PNG held in flash and drawn with its transparency respected: the alpha
// channel becomes a mask, and only the set runs of each line are pushed, so the
// background shows through instead of being painted black.
//
// This differs from WeatherIcon, which decodes a *downloaded* PNG into a sprite
// of its own, trims it and rescales it. An Icon is fixed, embedded at build
// time, and drawn at its native size straight into the destination.
//
// Ported from the WeatherS3 project. Two changes were needed here: it shares
// WeatherIcon's PNG decoder rather than creating a second one (the object is
// ~40 KB, and two would not fit), and only the images this project actually
// embeds are declared.
class Icon
{
    private:
        uint8_t* _imageStart;
        uint8_t* _imageEnd;
        uint16_t _imageWidth = 0;
        uint16_t _imageHeight = 0;

    public:
        Icon(const uint8_t* start, const uint8_t* end);

        void Render(int x, int y);                        // straight to the panel
        void Render(TFT_eSprite* sprite, int x, int y);   // into an offscreen sprite

        // As Render(), but soft-edged. Render() masks each pixel in or out at a
        // threshold, so a partially transparent pixel is simply dropped and the
        // icon gets a hard, jagged rim. This reads each destination pixel and
        // alpha-blends the source over it, so the antialiased edge survives.
        //
        // Slower -- it works a pixel at a time and reads back from the sprite --
        // so it is worth it for a small icon, not for a large image.
        void RenderHighQuality(TFT_eSprite* sprite, int x, int y);

        uint16_t Width();
        uint16_t Height();
};

// Shown while a fetch is in flight. images/down.png, embedded by
// board_build.embed_files in platformio.ini.
extern Icon DownIcon;

#endif // _ICON_H
