#include "DatePanel.h"
#include "Font.h"

DatePanel::DatePanel(int x, int y)
    : _x(x), _y(y)
{
}

void DatePanel::Render(TFT_eSprite* sprite, const struct tm& time)
{
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%d %B, %Y", &time);   // e.g. "03 January, 2026"

    sprite->loadFont(gillsans20);
    sprite->setTextColor(TFT_WHITE, TFT_BLACK);

    // Vertically centre the glyph ink on _y. ML_DATUM centres the full
    // line-advance box (which includes line gap) and leaves the text sitting
    // high, so instead place the baseline such that the ascent..descent ink box
    // is centred on _y.
    int ascent    = sprite->gFont.maxAscent;
    int descent   = sprite->gFont.maxDescent;
    int baselineY = _y + (ascent - descent) / 2 + 2;   // +2px optical bias downward

    sprite->setTextDatum(L_BASELINE);
    sprite->drawString(buffer, _x, baselineY);
}
