#ifndef _ICON_H
#define _ICON_H

#include <Arduino.h>
#include <stdlib.h>
#include <PNGdec.h>
#include <TFT_eSPI.h>

// A PNG, decoded once and drawn with its transparency intact.
//
// Prepare() decodes the image, trims the fully transparent border away, scales
// what is left to a requested height, and keeps the result as two arrays: the
// colour of each pixel, and its alpha. Render() then blends those over whatever
// is already in the destination sprite.
//
// Two details worth knowing:
//
//   * The crop keys off the alpha channel, not off blackness. Trimming "black"
//     pixels would eat an icon's own dark outline.
//   * Scaling averages premultiplied colour, so a pixel that is half covered
//     contributes half as much colour as it does alpha. Averaging plain RGB
//     would drag the edge toward whatever the transparent pixels happen to hold.
//
// Alpha is kept rather than composited against black at decode time, so an icon
// is correct over any background, not just a black one.
//
// The source may be in flash (embedded via board_build.embed_files) or in RAM (a
// download). A RAM source must stay alive only for the duration of Prepare().
class Icon
{
    public:
        Icon() = default;
        Icon(const uint8_t* start, const uint8_t* end);   // embedded image
        ~Icon();

        Icon(const Icon&) = delete;
        Icon& operator=(const Icon&) = delete;

        // Point at a PNG held in RAM. Does not decode; call Prepare() next.
        bool LoadFromRam(const uint8_t* png, size_t len);

        // Decode, crop and scale. targetHeight <= 0 keeps the cropped size.
        // Safe to call again to re-scale, or after LoadFromRam() for a new image.
        bool Prepare(int targetHeight = 0);

        // Alpha-blend the cached pixels over the destination.
        void Render(TFT_eSprite* dest, int x, int y) const;

        bool     Ready() const  { return _ready; }
        uint16_t Width() const  { return _w; }
        uint16_t Height() const { return _h; }

    private:
        void freeCache();

        const uint8_t* _data = nullptr;
        size_t         _len = 0;
        bool           _inFlash = false;

        uint16_t* _pixels = nullptr;   // _w * _h, RGB565
        uint8_t*  _alpha = nullptr;    // _w * _h, 0..255
        uint16_t  _w = 0;
        uint16_t  _h = 0;
        bool      _ready = false;

        // A decoded source image is held whole and briefly, at 3 bytes a pixel.
        // 128x128 is 48 KB, which is already generous for an icon.
        static const int kMaxSourceSide = 128;
};

// Shown while a fetch is in flight. images/down.png, embedded by
// board_build.embed_files in platformio.ini. Prepare() it before first use.
extern Icon DownIcon;

#endif // _ICON_H
