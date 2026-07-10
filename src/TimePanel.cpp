#include "TimePanel.h"
#include "Font.h"

// Layout tuning
static const int kDigitGap  = 6;    // gap between the two digits of a pair
static const int kColonGap  = 4;    // gap either side of the colon
static const int kColumnGap = 8;    // gap between HH:MM and the AM/PM column

// Below this much vertical room the AM/PM superscript drops a size, otherwise it
// crowds the shrunken digits beside it.
static const int kCompactRegion = 70;

// The full-size digit proportions, which every smaller size is derived from.
static const int kFullWidth     = 42;
static const int kFullThickness = 9;

TimePanel::TimePanel(bool use24Hour, bool blinkColon,
                     uint16_t activeColour, uint16_t inactiveColour)
    : _use24Hour(use24Hour),
      _blinkColon(blinkColon),
      _active(activeColour),
      _inactive(inactiveColour)
{
}

// Lit on even seconds, dark on odd, so a full blink cycle takes two seconds.
// Not blinking means always lit.
bool TimePanel::colonLit(const struct tm& time) const
{
    return !_blinkColon || (time.tm_sec % 2 == 0);
}

void TimePanel::draw(TFT_eSprite* s,
                     int h0, int h1, int m0, int m1,
                     const char* ampm, bool colonLit,
                     int topLimit, int bottomLimit)
{
    int region = bottomLimit - topLimit;
    if (region < 1)
        return;

    // As tall as the region allows, never taller than full size, keeping the
    // 42:80:9 proportions. SevenSegment holds no per-draw state, so building one
    // here each frame costs nothing and lets the size vary smoothly.
    int Hm = (region < kFullHeight) ? region : kFullHeight;
    int Wm = (Hm * kFullWidth + kFullHeight / 2) / kFullHeight;
    int Tm = (Hm * kFullThickness + kFullHeight / 2) / kFullHeight;
    if (Tm < 3) Tm = 3;

    SevenSegment big(Wm, Hm, Tm);

    uint16_t on       = _active;
    uint16_t off      = _inactive;
    uint16_t colonCol = colonLit ? on : off;

    int colonW = big.ColonWidth();
    int mainW  = 4 * Wm + 2 * kDigitGap + 2 * kColonGap + colonW;

    // The AM/PM superscript sits in a column to the right of HH:MM.
    const uint8_t* ampmFont = (region < kCompactRegion) ? arial14 : arial18;
    int colW = 0;
    if (ampm)
    {
        s->loadFont(ampmFont);
        colW = kColumnGap + s->textWidth(ampm);
    }
    int totalW = mainW + colW;

    int startX  = (s->width() - totalW) / 2;
    int mainTop = topLimit + (region - Hm) / 2;

    // HH:MM
    int x = startX;
    big.DrawDigit(s, x, mainTop, h0, on, off);  x += Wm + kDigitGap;
    big.DrawDigit(s, x, mainTop, h1, on, off);  x += Wm + kColonGap;
    big.DrawColon(s, x, mainTop, colonCol);     x += colonW + kColonGap;
    big.DrawDigit(s, x, mainTop, m0, on, off);  x += Wm + kDigitGap;
    big.DrawDigit(s, x, mainTop, m1, on, off);  x += Wm;

    // AM/PM: superscript, top-aligned with HH:MM
    if (ampm)
    {
        s->loadFont(ampmFont);
        s->setTextColor(on, TFT_BLACK);
        s->setTextDatum(TL_DATUM);
        s->drawString(ampm, x + kColumnGap, mainTop);
    }
}

void TimePanel::Render(TFT_eSprite* s, const struct tm& time, int topLimit, int bottomLimit)
{
    if (_use24Hour)
    {
        // 00..23, zero-padded, and no AM/PM column.
        draw(s,
             time.tm_hour / 10, time.tm_hour % 10,
             time.tm_min / 10, time.tm_min % 10,
             nullptr, colonLit(time), topLimit, bottomLimit);
        return;
    }

    bool pm = time.tm_hour >= 12;

    int hour12 = time.tm_hour % 12;
    if (hour12 == 0) hour12 = 12;

    int h0 = hour12 / 10;
    int h1 = hour12 % 10;
    if (h0 == 0) h0 = SevenSegment::BLANK;   // suppress the leading zero of the hour

    draw(s,
         h0, h1,
         time.tm_min / 10, time.tm_min % 10,
         pm ? "PM" : "AM", colonLit(time), topLimit, bottomLimit);
}

void TimePanel::RenderUnknown(TFT_eSprite* s, int topLimit, int bottomLimit)
{
    int B = SevenSegment::BLANK;
    draw(s, B, B, B, B, nullptr, false, topLimit, bottomLimit);
}
