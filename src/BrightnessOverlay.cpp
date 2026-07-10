#include "BrightnessOverlay.h"
#include "Font.h"

BrightnessOverlay::BrightnessOverlay(uint32_t showMs)
    : _showMs(showMs)
{
}

void BrightnessOverlay::Show(int percent)
{
    _percent = percent;
    _shownAt = millis();
    _visible = true;
    _dirty   = true;      // the render loop must repaint to reveal/refresh us
}

bool BrightnessOverlay::Active() const
{
    return _visible && (millis() - _shownAt) < _showMs;
}

bool BrightnessOverlay::TakeDirty()
{
    bool d = _dirty;
    _dirty = false;
    return d;
}

void BrightnessOverlay::Render(TFT_eSprite* s)
{
    if (!Active())
        return;

    const int r = 10;
    int W = 168, H = 72;                       // preferred size, clamped to the display
    if (W > s->width()  - 8) W = s->width()  - 8;
    if (H > s->height() - 8) H = s->height() - 8;
    int x = (s->width()  - W) / 2;             // always centred, whatever the resolution
    int y = (s->height() - H) / 2;

    uint16_t bg     = s->color565(28, 28, 30);
    uint16_t border = s->color565(120, 120, 120);
    uint16_t track  = s->color565(70, 70, 74);
    uint16_t fill   = s->color565(240, 200, 40);   // warm amber

    // Panel
    s->fillRoundRect(x, y, W, H, r, bg);
    s->drawRoundRect(x, y, W, H, r, border);

    // Percentage, centred near the top of the panel.
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", _percent);
    s->loadFont(cabin21);
    s->setTextColor(TFT_WHITE, bg);       // bg matches the panel for clean anti-aliasing
    s->setTextDatum(TC_DATUM);
    s->drawString(buf, x + W / 2, y + 12);

    // Brightness bar along the bottom of the panel.
    int barPad = 18;
    int barX   = x + barPad;
    int barW   = W - 2 * barPad;
    int barH   = 10;
    int barY   = y + H - barH - 12;

    s->fillRoundRect(barX, barY, barW, barH, barH / 2, track);
    int fillW = (barW * _percent) / 100;
    if (fillW < barH) fillW = barH;       // keep the rounded cap visible at low %
    s->fillRoundRect(barX, barY, fillW, barH, barH / 2, fill);
}
