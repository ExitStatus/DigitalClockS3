#include "TimePanel.h"
#include "Font.h"

// Layout tuning
static const int kDigitGap  = 6;    // gap between the two digits of a pair
static const int kColonGap  = 4;    // gap either side of the colon
static const int kColumnGap = 8;    // gap between HH:MM and the AM/PM column

TimePanel::TimePanel(bool use24Hour, bool blinkColon,
                     uint16_t activeColour, uint16_t inactiveColour)
    : _big(42, 80, 9),      // large HH:MM digits
      _use24Hour(use24Hour),
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
                     const char* ampm, bool colonLit)
{
    uint16_t on       = _active;
    uint16_t off      = _inactive;
    uint16_t colonCol = colonLit ? on : off;

    int Wm     = _big.Width();
    int Hm     = _big.Height();
    int colonW = _big.ColonWidth();

    int mainW = 4 * Wm + 2 * kDigitGap + 2 * kColonGap + colonW;

    // The AM/PM superscript sits in a column to the right of HH:MM.
    int colW = 0;
    if (ampm)
    {
        s->loadFont(arial18);   // ~25% larger than arial14
        colW = kColumnGap + s->textWidth(ampm);
    }
    int totalW = mainW + colW;

    int startX  = (s->width() - totalW) / 2;
    int mainTop = (s->height() - Hm) / 2;

    // HH:MM
    int x = startX;
    _big.DrawDigit(s, x, mainTop, h0, on, off);  x += Wm + kDigitGap;
    _big.DrawDigit(s, x, mainTop, h1, on, off);  x += Wm + kColonGap;
    _big.DrawColon(s, x, mainTop, colonCol);     x += colonW + kColonGap;
    _big.DrawDigit(s, x, mainTop, m0, on, off);  x += Wm + kDigitGap;
    _big.DrawDigit(s, x, mainTop, m1, on, off);  x += Wm;

    // AM/PM: superscript, top-aligned with HH:MM
    if (ampm)
    {
        s->loadFont(arial18);
        s->setTextColor(on, TFT_BLACK);
        s->setTextDatum(TL_DATUM);
        s->drawString(ampm, x + kColumnGap, mainTop);
    }
}

void TimePanel::Render(TFT_eSprite* s, const struct tm& time)
{
    if (_use24Hour)
    {
        // 00..23, zero-padded, and no AM/PM column.
        draw(s,
             time.tm_hour / 10, time.tm_hour % 10,
             time.tm_min / 10, time.tm_min % 10,
             nullptr, colonLit(time));
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
         pm ? "PM" : "AM", colonLit(time));
}

void TimePanel::RenderUnknown(TFT_eSprite* s)
{
    int B = SevenSegment::BLANK;
    draw(s, B, B, B, B, nullptr, false);
}
