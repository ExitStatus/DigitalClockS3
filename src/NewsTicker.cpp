#include "NewsTicker.h"
#include "Font.h"

#include <string.h>

NewsTicker::NewsTicker(int scrollPixelsPerSecond, uint32_t openMs)
    : _speed(scrollPixelsPerSecond), _openMs(openMs)
{
    if (_speed < 1)  _speed = 1;
    if (_openMs < 1) _openMs = 1;
}

// 0 while the clock is full size, 1000 once it has shrunk out of the band's way.
int NewsTicker::phasePermille() const
{
    uint32_t elapsed = millis() - _phaseStart;

    switch (_phase)
    {
        case Phase::Idle:      return 0;
        case Phase::Scrolling: return 1000;

        case Phase::Opening:
        {
            uint32_t p = elapsed * 1000 / _openMs;
            return (p > 1000) ? 1000 : (int)p;
        }
        case Phase::Closing:
        {
            uint32_t p = elapsed * 1000 / _openMs;
            return (p > 1000) ? 0 : (int)(1000 - p);
        }
    }
    return 0;
}

int NewsTicker::ClockTop() const
{
    return kCompactTop * phasePermille() / 1000;
}

int NewsTicker::ClockBottom() const
{
    int p = phasePermille();
    return kSpriteHeight - (kSpriteHeight - kCompactBottom) * p / 1000;
}

bool NewsTicker::BandVisible() const
{
    return phasePermille() >= 900;
}

bool NewsTicker::Animating() const
{
    return _phase != Phase::Idle;
}

void NewsTicker::loadNext(NewsApi& news)
{
    const RssParser::Item* item = news.NextHeadline();
    if (!item)
        return;

    strncpy(_headline, item->title, sizeof(_headline) - 1);
    _headline[sizeof(_headline) - 1] = 0;
    _headlineEpoch = item->epoch;

    // Width needs a sprite and a loaded font, so it is taken at the first render.
    _textWidth      = 0;
    _pendingMeasure = true;
}

void NewsTicker::Update(NewsApi& news)
{
    uint32_t now = millis();

    switch (_phase)
    {
        case Phase::Idle:
            if (news.HasHeadline())
            {
                _phase = Phase::Opening;
                _phaseStart = now;
            }
            break;

        case Phase::Opening:
            if (!news.HasHeadline())
            {
                // The queue emptied under us; give the clock its space back,
                // resuming from wherever the shrink had got to.
                _phase = Phase::Closing;
                _phaseStart = now - (uint32_t)(1000 - phasePermille()) * _openMs / 1000;
            }
            else if (phasePermille() >= 950)
            {
                loadNext(news);
                _phase = Phase::Scrolling;
                _phaseStart = now;
            }
            break;

        case Phase::Scrolling:
        {
            if (_pendingMeasure)
                break;   // the next render measures it; nothing to decide yet

            int offset = 320 - (int)((uint32_t)_speed * (now - _phaseStart) / 1000);
            if (offset <= -_textWidth)
            {
                news.MarkShown(_headlineEpoch);

                if (news.HasHeadline())
                {
                    loadNext(news);       // straight into the next one
                    _phaseStart = now;
                }
                else
                {
                    _phase = Phase::Closing;
                    _phaseStart = now;
                }
            }
            break;
        }

        case Phase::Closing:
            if (news.HasHeadline())
            {
                // A refresh landed mid-close. Reverse without jumping: restart
                // the open at the size we have already shrunk to.
                int p = phasePermille();
                _phase = Phase::Opening;
                _phaseStart = now - (uint32_t)p * _openMs / 1000;
            }
            else if (phasePermille() <= 50)
            {
                _phase = Phase::Idle;
            }
            break;
    }
}

void NewsTicker::RenderBand(TFT_eSprite* sprite)
{
    if (_phase != Phase::Scrolling)
        return;   // the band is open but empty while the digits are still moving

    sprite->loadFont(gillsans24);
    sprite->setTextColor(TFT_WHITE, TFT_BLACK);
    sprite->setTextDatum(L_BASELINE);

    // Without this the headline wraps: a smooth-font glyph whose right edge falls
    // outside the sprite starts a new line, and the headline begins off the right
    // edge by design. Wrapping is what a marquee must never do.
    sprite->setTextWrap(false, false);

    if (_pendingMeasure)
    {
        _textWidth      = sprite->textWidth(_headline);
        _pendingMeasure = false;
    }

    int offset = 320 - (int)((uint32_t)_speed * (millis() - _phaseStart) / 1000);
    sprite->drawString(_headline, offset, kBandBaseline);
}
