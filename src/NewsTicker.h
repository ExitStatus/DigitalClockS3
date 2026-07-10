#ifndef _NEWS_TICKER_H
#define _NEWS_TICKER_H

#include <TFT_eSPI.h>

#include "NewsApi.h"

// Shows headlines along a band beneath the clock.
//
// There is no room for a ticker on the clock page as it stands, so it is made:
// while headlines are waiting the digits shrink, opening a band between them and
// the weather line, and the headline scrolls through it right-to-left. When the
// queue drains the digits grow back. The weather line is never touched.
//
// The ticker does not own the clock's geometry; it only reports the vertical
// region the digits should occupy this frame, and TimePanel fits itself to it.
// Idle, that region is the whole sprite, which is the original layout exactly.
//
// Scrolling is at a constant speed rather than for a constant duration, so a
// long headline takes longer to cross rather than moving faster. The position is
// computed from elapsed milliseconds, not accumulated per frame, so a dropped
// frame skips the text along instead of slowing it down.
class NewsTicker
{
    public:
        NewsTicker(int scrollPixelsPerSecond, uint32_t openMs);

        // Drives the state machine; pulls a headline when it is ready for one.
        void Update(NewsApi& news);

        // True while the digits are resizing or a headline is scrolling: the
        // frame needs repainting at animation rate.
        bool Animating() const;

        // The vertical region the clock digits may use this frame.
        int ClockTop() const;
        int ClockBottom() const;

        bool BandVisible() const;
        void RenderBand(TFT_eSprite* sprite);

    private:
        enum class Phase { Idle, Opening, Scrolling, Closing };

        int  phasePermille() const;   // 0 = full clock, 1000 = fully compact
        void loadNext(NewsApi& news);

        // Geometry, in sprite rows. The band sits between the compact digits and
        // the weather line's ink, which starts around y=138.
        static const int kSpriteHeight = 170;
        static const int kCompactTop    = 30;   // clear of the date and status row
        static const int kCompactBottom = 86;
        static const int kBandTop       = 92;
        static const int kBandBaseline  = 121;  // cabin21 ink then spans 97..127

        int      _speed;      // pixels per second
        uint32_t _openMs;

        Phase    _phase = Phase::Idle;
        uint32_t _phaseStart = 0;   // millis() when the current phase began

        char     _headline[RssParser::kMaxTitle] = {};
        uint32_t _headlineEpoch = 0;
        int      _textWidth = 0;    // measured once per headline
        bool     _pendingMeasure = false;
};

#endif // _NEWS_TICKER_H
