#ifndef _SEVEN_SEGMENT_H
#define _SEVEN_SEGMENT_H

#include <TFT_eSPI.h>

// Draws seven-segment digits at a configurable size into a sprite. Every
// segment is drawn every time: lit segments in the active colour, unlit ones in
// the inactive colour, giving the classic "ghosted" LCD/LED look. Segments are
// bevelled hexagons.
class SevenSegment
{
    public:
        static const int BLANK = -1;   // draw a digit cell with all segments inactive

        SevenSegment(int digitWidth, int digitHeight, int thickness);

        // Draw one digit (0-9, or BLANK) with its top-left at (x, y).
        void DrawDigit(TFT_eSprite* sprite, int x, int y, int value,
                       uint16_t onColour, uint16_t offColour) const;

        // Draw a two-dot colon separator in a cell ColonWidth() wide at (x, y).
        void DrawColon(TFT_eSprite* sprite, int x, int y, uint16_t colour) const;

        int Width() const      { return _w; }
        int Height() const     { return _h; }
        int Thickness() const  { return _t; }
        int ColonWidth() const { return _t * 2; }

    private:
        void fillHSegment(TFT_eSprite* s, int x1, int x2, int cy, uint16_t colour) const;
        void fillVSegment(TFT_eSprite* s, int cx, int y1, int y2, uint16_t colour) const;
        void fillHexagon(TFT_eSprite* s, const int16_t* xs, const int16_t* ys, uint16_t colour) const;

        int _w;
        int _h;
        int _t;
        int _g;   // small gap between segment ends
};

#endif // _SEVEN_SEGMENT_H
