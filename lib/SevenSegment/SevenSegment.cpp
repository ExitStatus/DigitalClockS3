#include "SevenSegment.h"

// Segment bits.
//      aaaa
//     f    b
//     f    b
//      gggg
//     e    c
//     e    c
//      dddd
static const uint8_t A = 1 << 0;
static const uint8_t B = 1 << 1;
static const uint8_t C = 1 << 2;
static const uint8_t D = 1 << 3;
static const uint8_t E = 1 << 4;
static const uint8_t F = 1 << 5;
static const uint8_t G = 1 << 6;

// Which segments are lit for each digit 0-9.
static const uint8_t kDigit[10] = {
    A | B | C | D | E | F,       // 0
    B | C,                       // 1
    A | B | G | E | D,           // 2
    A | B | G | C | D,           // 3
    F | G | B | C,               // 4
    A | F | G | C | D,           // 5
    A | F | G | E | C | D,       // 6
    A | B | C,                   // 7
    A | B | C | D | E | F | G,   // 8
    A | B | C | D | F | G        // 9
};

SevenSegment::SevenSegment(int digitWidth, int digitHeight, int thickness)
    : _w(digitWidth), _h(digitHeight), _t(thickness)
{
    _g = _t / 6;
    if (_g < 1) _g = 1;
}

void SevenSegment::fillHexagon(TFT_eSprite* s, const int16_t* xs, const int16_t* ys, uint16_t colour) const
{
    // Triangle fan from vertex 0 across the 6-point convex hexagon.
    for (int i = 1; i < 5; i++)
        s->fillTriangle(xs[0], ys[0], xs[i], ys[i], xs[i + 1], ys[i + 1], colour);
}

void SevenSegment::fillHSegment(TFT_eSprite* s, int x1, int x2, int cy, uint16_t colour) const
{
    int h = _t / 2;
    int16_t xs[6] = { (int16_t)x1, (int16_t)(x1 + h), (int16_t)(x2 - h), (int16_t)x2, (int16_t)(x2 - h), (int16_t)(x1 + h) };
    int16_t ys[6] = { (int16_t)cy, (int16_t)(cy - h), (int16_t)(cy - h), (int16_t)cy, (int16_t)(cy + h), (int16_t)(cy + h) };
    fillHexagon(s, xs, ys, colour);
}

void SevenSegment::fillVSegment(TFT_eSprite* s, int cx, int y1, int y2, uint16_t colour) const
{
    int h = _t / 2;
    int16_t xs[6] = { (int16_t)cx, (int16_t)(cx + h), (int16_t)(cx + h), (int16_t)cx, (int16_t)(cx - h), (int16_t)(cx - h) };
    int16_t ys[6] = { (int16_t)y1, (int16_t)(y1 + h), (int16_t)(y2 - h), (int16_t)y2, (int16_t)(y2 - h), (int16_t)(y1 + h) };
    fillHexagon(s, xs, ys, colour);
}

void SevenSegment::DrawDigit(TFT_eSprite* s, int x, int y, int value, uint16_t on, uint16_t off) const
{
    uint8_t mask = (value >= 0 && value <= 9) ? kDigit[value] : 0;   // BLANK -> all off

    int left = x, right = x + _w;
    int top = y, bottom = y + _h;
    int midY = y + _h / 2;

    int hx1 = left + _t / 2 + _g;    // horizontal segment span
    int hx2 = right - _t / 2 - _g;
    int cxL = left + _t / 2;         // vertical segment centre lines
    int cxR = right - _t / 2;
    int vyT1 = top + _t / 2 + _g;    // upper vertical span
    int vyT2 = midY - _t / 2 - _g;
    int vyB1 = midY + _t / 2 + _g;   // lower vertical span
    int vyB2 = bottom - _t / 2 - _g;

    fillHSegment(s, hx1, hx2, top + _t / 2,    (mask & A) ? on : off);   // a
    fillVSegment(s, cxR, vyT1, vyT2,           (mask & B) ? on : off);   // b
    fillVSegment(s, cxR, vyB1, vyB2,           (mask & C) ? on : off);   // c
    fillHSegment(s, hx1, hx2, bottom - _t / 2, (mask & D) ? on : off);   // d
    fillVSegment(s, cxL, vyB1, vyB2,           (mask & E) ? on : off);   // e
    fillVSegment(s, cxL, vyT1, vyT2,           (mask & F) ? on : off);   // f
    fillHSegment(s, hx1, hx2, midY,            (mask & G) ? on : off);   // g
}

void SevenSegment::DrawColon(TFT_eSprite* s, int x, int y, uint16_t colour) const
{
    int cx = x + ColonWidth() / 2;
    int r = _t / 2;
    s->fillCircle(cx, y + _h / 3,     r, colour);
    s->fillCircle(cx, y + 2 * _h / 3, r, colour);
}
