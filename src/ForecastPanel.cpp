#include "ForecastPanel.h"
#include "Config.h"
#include "Font.h"

ForecastPanel::ForecastPanel(uint32_t fadeMs, uint32_t holdMs, uint8_t statMask)
    : _fadeMs(fadeMs), _holdMs(holdMs), _cycleMs(2 * fadeMs + holdMs)
{
    const uint8_t order[] = { StatMaxTemp, StatRain, StatWind };
    for (uint8_t stat : order)
        if (statMask & stat)
            _stats[_count++] = stat;
}

void ForecastPanel::Update()
{
    if (_count == 0)
        return;

    uint32_t now = millis();

    if (!_started)
    {
        _start = now;
        _started = true;
        return;
    }

    if (now - _start >= _cycleMs)
    {
        _index = (_index + 1) % _count;   // advance to the next enabled stat
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
    if (_count == 0)
        return false;

    uint32_t e = millis() - _start;
    return (e < _fadeMs) || (e >= _fadeMs + _holdMs && e < _cycleMs);
}

void ForecastPanel::Render(TFT_eSprite* sprite, const WeatherApi& weather,
                           int leftBound, int rightBound, int baselineY)
{
    _y = baselineY;   // baseline supplied at render time (tracks display height)

    if (_count == 0 || !weather.HasForecast())
        return;

    int alpha = alphaNow();
    if (alpha <= 0)
        return;                                        // nothing visible right now

    uint16_t colour = sprite->alphaBlend((uint8_t)alpha, TFT_WHITE, TFT_BLACK);
    int centerX = (leftBound + rightBound) / 2;

    switch (_stats[_index])
    {
        case StatMaxTemp:
            drawStat(sprite, centerX, "Max",
                     String(weather.MaxTempToday()) + "/" + String(weather.MaxTempTomorrow()),
                     "", true, colour);
            break;
        case StatRain:
            drawStat(sprite, centerX, "Rain",
                     String(weather.RainChanceToday()) + "/" + String(weather.RainChanceTomorrow()),
                     "%", false, colour);
            break;
        case StatWind:
            drawStat(sprite, centerX, "Wind",
                     String(weather.MaxWindToday()) + "/" + String(weather.MaxWindTomorrow()),
                     " " WIND_UNIT_LABEL, false, colour);
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

    // Temperature: "<label> <value>" then a degree ring and the unit letter,
    // centred as a unit.
    String pre = String(label) + " " + value;
    sprite->setTextDatum(L_BASELINE);

    int preW = sprite->textWidth(pre);
    int cW   = sprite->textWidth(TEMP_UNIT_LABEL);
    int r = 3, ringGap = 2, cGap = 3, ringW = 2 * r;
    int totalW = preW + ringGap + ringW + cGap + cW;
    int startX = centerX - totalW / 2;

    sprite->drawString(pre, startX, _y);

    int ascent = sprite->gFont.maxAscent;
    int ringCx = startX + preW + ringGap + r;
    sprite->drawCircle(ringCx, _y - ascent + r + 1, r, colour);

    sprite->drawString(TEMP_UNIT_LABEL, startX + preW + ringGap + ringW + cGap, _y);
}
