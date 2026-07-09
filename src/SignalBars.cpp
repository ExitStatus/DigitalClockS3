#include "SignalBars.h"

SignalBars::SignalBars(int width, int height, int barCount)
    : _width(width), _height(height), _barCount(barCount)
{
}

// How many bars to fill for a given strength. Uses ceiling so any non-zero
// signal lights at least one bar; 0% lights none.
int SignalBars::filledBars(int percent) const
{
    int filled = (percent * _barCount + 99) / 100;   // ceil(percent/100 * barCount)
    if (filled > _barCount) filled = _barCount;
    return filled;
}

// Linear blend between two RGB colours, t in [0,1].
static uint16_t blend(TFT_eSprite* sprite,
                      uint8_t r0, uint8_t g0, uint8_t b0,
                      uint8_t r1, uint8_t g1, uint8_t b1, float t)
{
    uint8_t r = r0 + (int)((r1 - r0) * t);
    uint8_t g = g0 + (int)((g1 - g0) * t);
    uint8_t b = b0 + (int)((b1 - b0) * t);
    return sprite->color565(r, g, b);
}

// Two-segment ramp: red at 0%, orange at 50%, green at 100%.
uint16_t SignalBars::colourFor(TFT_eSprite* sprite, int percent) const
{
    float t = percent / 100.0f;

    if (t < 0.5f)
        return blend(sprite, 230, 70, 55,  243, 146, 40, t / 0.5f);          // red -> orange
    else
        return blend(sprite, 243, 146, 40,  46, 190, 120, (t - 0.5f) / 0.5f); // orange -> green
}

void SignalBars::Render(TFT_eSprite* sprite, int x, int y, int percent)
{
    if (percent < 0)   percent = 0;
    if (percent > 100) percent = 100;

    uint16_t colour = colourFor(sprite, percent);
    int filled = filledBars(percent);

    int barWidth = (_width - kGap * (_barCount - 1)) / _barCount;
    if (barWidth < 1) barWidth = 1;

    for (int i = 0; i < _barCount; i++)
    {
        int barHeight = _height * (i + 1) / _barCount;   // shortest on the left
        int bx = x + i * (barWidth + kGap);
        int by = y + (_height - barHeight);              // bottom-aligned

        if (i < filled)
            sprite->fillRect(bx, by, barWidth, barHeight, colour);
        else
            sprite->drawRect(bx, by, barWidth, barHeight, colour);
    }
}
