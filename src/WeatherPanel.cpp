#include "WeatherPanel.h"
#include "Config.h"
#include "Font.h"

WeatherPanel::WeatherPanel(int x)
    : _x(x)
{
}

int WeatherPanel::Render(TFT_eSprite* sprite, WeatherIcon& icon, float temp, int baselineY)
{
    int tempX = _x;

    // Weather icon on the left, sitting just below the text baseline so it
    // aligns with the digits rather than floating above them.
    if (icon.Ready())
    {
        icon.Render(sprite, _x, baselineY - icon.Height() + 4);
        tempX = _x + icon.Width() + kIconGap;
    }

    uint16_t colour = TFT_WHITE;

    sprite->loadFont(cabin21);
    sprite->setTextColor(colour, TFT_BLACK);
    sprite->setTextDatum(L_BASELINE);   // draw on the baseline

    char number[8];
    snprintf(number, sizeof(number), "%.0f", temp);
    sprite->drawString(number, tempX, baselineY);
    int numberWidth = sprite->textWidth(number);

    // Degree ring near the top of the digits, then the unit letter on the
    // baseline. Drawn manually so it does not depend on the font having a
    // degree glyph.
    int ascent = sprite->gFont.maxAscent;
    int r  = 3;
    int dx = tempX + numberWidth + 2;
    sprite->drawCircle(dx + r, baselineY - ascent + r + 1, r, colour);

    int cx = dx + 2 * r + 2;
    sprite->drawString(TEMP_UNIT_LABEL, cx, baselineY);

    return cx + sprite->textWidth(TEMP_UNIT_LABEL);   // right edge of the temperature block
}
