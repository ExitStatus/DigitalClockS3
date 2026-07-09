#include "ForecastPanel.h"
#include "Font.h"

ForecastPanel::ForecastPanel(int baselineY, uint32_t fadeMs, uint32_t holdMs)
    : _y(baselineY), _fadeMs(fadeMs), _holdMs(holdMs), _cycleMs(2 * fadeMs + holdMs)
{
}

void ForecastPanel::Update()
{
    uint32_t now = millis();

    if (!_started)
    {
        _start = now;
        _started = true;
        return;
    }

    if (now - _start >= _cycleMs)
    {
        _index = (_index + 1) % kStatCount;   // advance to the next stat
        _start = now;
    }
}

int ForecastPanel::alphaNow() const
{
    uint32_t e = millis() - _start;

    if (e < _fadeMs)
        return (int)(e * 255 / _fadeMs);                          // fading in
    if (e < _fadeMs + _holdMs)
        return 255;                                               // holding
    if (e < _cycleMs)
        return (int)(255 - (e - _fadeMs - _holdMs) * 255 / _fadeMs); // fading out
    return 0;
}

bool ForecastPanel::Animating() const
{
    uint32_t e = millis() - _start;
    return (e < _fadeMs) || (e >= _fadeMs + _holdMs && e < _cycleMs);
}

void ForecastPanel::Render(TFT_eSprite* sprite, const WeatherApi& weather,
                           int leftBound, int rightBound)
{
    if (!weather.HasForecast())
        return;

    int alpha = alphaNow();
    if (alpha <= 0)
        return;                                        // nothing visible right now

    uint16_t colour = sprite->alphaBlend((uint8_t)alpha, TFT_WHITE, TFT_BLACK);
    int centerX = (leftBound + rightBound) / 2;

    switch (_index)
    {
        case 0:
            drawStat(sprite, centerX, "Max",
                     String(weather.MaxTempToday()) + "/" + String(weather.MaxTempTomorrow()),
                     "", true, colour);
            break;
        case 1:
            drawStat(sprite, centerX, "Rain",
                     String(weather.RainChanceToday()) + "/" + String(weather.RainChanceTomorrow()),
                     "%", false, colour);
            break;
        case 2:
            drawStat(sprite, centerX, "Wind",
                     String(weather.MaxWindToday()) + "/" + String(weather.MaxWindTomorrow()),
                     " mph", false, colour);
            break;
    }
}

void ForecastPanel::drawStat(TFT_eSprite* sprite, int centerX, const char* label,
                             const String& value, const char* unit, bool degree, uint16_t colour)
{
    sprite->loadFont(gillsans18);
    sprite->setTextColor(colour, TFT_BLACK);

    if (!degree)
    {
        String str = String(label) + " " + value + unit;
        sprite->setTextDatum(C_BASELINE);          // centred horizontally, baseline at _y
        sprite->drawString(str, centerX, _y);
        return;
    }

    // Temperature: "<label> <value>" then a degree ring and 'C', centred as a unit.
    String pre = String(label) + " " + value;
    sprite->setTextDatum(L_BASELINE);

    int preW = sprite->textWidth(pre);
    int cW   = sprite->textWidth("C");
    int r = 3, ringGap = 2, cGap = 3, ringW = 2 * r;
    int totalW = preW + ringGap + ringW + cGap + cW;
    int startX = centerX - totalW / 2;

    sprite->drawString(pre, startX, _y);

    int ascent = sprite->gFont.maxAscent;
    int ringCx = startX + preW + ringGap + r;
    sprite->drawCircle(ringCx, _y - ascent + r + 1, r, colour);

    sprite->drawString("C", startX + preW + ringGap + ringW + cGap, _y);
}
