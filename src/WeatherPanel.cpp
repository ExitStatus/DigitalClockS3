#include "WeatherPanel.h"
#include "Font.h"

WeatherPanel::WeatherPanel(int x)
    : _x(x)
{
}

int WeatherPanel::Render(TFT_eSprite* sprite, WeatherIcon& icon, float tempC, int baselineY)
{
    int tempX = _x;

    // Weather icon on the left, bottom-aligned to the text baseline.
    if (icon.Ready())
    {
        icon.Render(sprite, _x, baselineY - icon.Height());
        tempX = _x + icon.Width() + kIconGap;
    }

    uint16_t colour = TFT_WHITE;

    sprite->loadFont(gillsans24);
    sprite->setTextColor(colour, TFT_BLACK);
    sprite->setTextDatum(L_BASELINE);   // draw on the baseline

    char number[8];
    snprintf(number, sizeof(number), "%.0f", tempC);
    sprite->drawString(number, tempX, baselineY);
    int numberWidth = sprite->textWidth(number);

    // Degree ring near the top of the digits, then 'C' on the baseline. Drawn
    // manually so it does not depend on the font having a degree glyph.
    int ascent = sprite->gFont.maxAscent;
    int r  = 3;
    int dx = tempX + numberWidth + 4;
    sprite->drawCircle(dx + r, baselineY - ascent + r + 1, r, colour);

    int cx = dx + 2 * r + 4;
    sprite->drawString("C", cx, baselineY);

    return cx + sprite->textWidth("C");   // right edge of the temperature block
}
