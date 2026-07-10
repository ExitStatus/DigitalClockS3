#include "RssParser.h"

#include <string.h>

namespace
{
    bool isNameChar(uint8_t c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == ':' || c == '-' || c == '_' || c == '.';
    }

    char lower(char c)
    {
        return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }

    bool isDigit(char c) { return c >= '0' && c <= '9'; }

    // Days since 1970-01-01 from a proleptic Gregorian date. Howard Hinnant's
    // days_from_civil. Used instead of mktime(), which interprets its argument in
    // local time -- and NtpClient sets TZ, so mktime() would silently shift every
    // publication date by the UK's offset. timegm() is not dependable here either.
    long daysFromCivil(int y, unsigned m, unsigned d)
    {
        y -= m <= 2;
        const int era = (y >= 0 ? y : y - 399) / 400;
        const unsigned yoe = (unsigned)(y - era * 400);              // [0, 399]
        const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
        const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;  // [0, 146096]
        return (long)era * 146097 + (long)doe - 719468;
    }

    int monthFromName(const char* s)
    {
        static const char* kMonths = "janfebmaraprmayjunjulaugsepoctnovdec";
        char m[3] = { lower(s[0]), lower(s[1]), lower(s[2]) };
        for (int i = 0; i < 12; i++)
            if (memcmp(kMonths + i * 3, m, 3) == 0)
                return i + 1;
        return 0;
    }
}

RssParser::RssParser(uint32_t since, int maxItems)
    : _since(since), _maxItems(maxItems)
{
    if (_maxItems < 1)            _maxItems = 1;
    if (_maxItems > kMaxItemsCap) _maxItems = kMaxItemsCap;
}

// ---- text -----------------------------------------------------------------

// Append one decoded character to whichever buffer is being captured. Anything
// the fonts cannot draw becomes '?', and runs of those collapse to one so a
// headline in another script does not arrive as forty question marks. Leading
// and repeated spaces are collapsed for the same reason.
void RssParser::emit(char c)
{
    char*  buf;
    size_t* len;
    size_t  cap;

    if (_capture == TITLE)        { buf = _title;   len = &_titleLen; cap = kMaxTitle; }
    else if (_capture == PUBDATE) { buf = _pubDate; len = &_pubLen;   cap = kMaxPubDate; }
    else                          return;

    if (c == '\n' || c == '\r' || c == '\t')
        c = ' ';

    if ((uint8_t)c < 0x20 || (uint8_t)c > 0x7E)
        c = '?';

    char last = (*len > 0) ? buf[*len - 1] : 0;

    if (c == ' ' && (*len == 0 || last == ' '))
        return;                       // no leading or doubled spaces
    if (c == '?' && last == '?')
        return;                       // one '?' per run of unrenderable text

    if (*len >= cap - 1)
        return;                       // full; the tail is dropped

    buf[(*len)++] = c;
    buf[*len] = 0;
}

// Fold a Unicode code point to something the ASCII-only fonts can draw. The
// mappings are the punctuation Google actually uses in headlines; everything
// else beyond ASCII becomes '?'.
void RssParser::emitCodepoint(long cp)
{
    if      (cp == 0x2018 || cp == 0x2019) emit('\'');
    else if (cp == 0x201C || cp == 0x201D) emit('"');
    else if (cp == 0x2013 || cp == 0x2014) emit('-');
    else if (cp == 0x00A0)                 emit(' ');
    else if (cp == 0x2026)               { emit('.'); emit('.'); emit('.'); }
    else if (cp >= 0x20 && cp < 0x7F)      emit((char)cp);
    else if (cp == '\n' || cp == '\r' || cp == '\t') emit(' ');
    else                                   emit('?');
}

// Decode "&...;" once the terminating ';' has arrived, and decode raw UTF-8
// sequences. _entity holds the '&' and everything after it. Anything
// unrecognised is emitted verbatim rather than swallowed, so a bare '&' in a
// headline survives.
void RssParser::feedText(uint8_t c)
{
    if (_entLen > 0)
    {
        if (c == ';')
        {
            _entity[_entLen] = 0;
            const char* e = _entity + 1;          // skip the '&'
            long cp = -1;

            if      (strcmp(e, "amp")  == 0) cp = '&';
            else if (strcmp(e, "lt")   == 0) cp = '<';
            else if (strcmp(e, "gt")   == 0) cp = '>';
            else if (strcmp(e, "quot") == 0) cp = '"';
            else if (strcmp(e, "apos") == 0) cp = '\'';
            else if (strcmp(e, "nbsp") == 0) cp = ' ';
            else if (e[0] == '#')
            {
                const char* d = e + 1;
                int base = 10;
                if (*d == 'x' || *d == 'X') { base = 16; d++; }
                cp = 0;
                for (; *d; d++)
                {
                    int v;
                    if      (isDigit(*d))                 v = *d - '0';
                    else if (base == 16 && lower(*d) >= 'a' && lower(*d) <= 'f')
                                                          v = lower(*d) - 'a' + 10;
                    else { cp = -1; break; }
                    cp = cp * base + v;
                    if (cp > 0x10FFFF) { cp = -1; break; }
                }
            }

            if (cp < 0)
            {
                // Not an entity we know. Put it back as it came.
                for (size_t i = 0; i < _entLen; i++)
                    emit(_entity[i]);
                emit(';');
            }
            else
            {
                emitCodepoint(cp);
            }

            _entLen = 0;
            return;
        }

        if (_entLen >= kMaxEntity - 1)
        {
            // Overlong: not an entity after all. Replay it and carry on.
            for (size_t i = 0; i < _entLen; i++)
                emit(_entity[i]);
            _entLen = 0;
            emit((char)c);
            return;
        }

        _entity[_entLen++] = (char)c;
        return;
    }

    // A multi-byte UTF-8 sequence in progress.
    if (_u8need > 0)
    {
        if ((c & 0xC0) == 0x80)
        {
            _u8cp = (_u8cp << 6) | (c & 0x3F);
            if (--_u8need == 0)
                emitCodepoint(_u8cp);
        }
        else
        {
            _u8need = 0;      // truncated sequence; drop it and reconsider this byte
            feedText(c);
        }
        return;
    }

    if (c == '&')
    {
        _entity[0] = '&';
        _entLen = 1;
        return;
    }

    if (c < 0x80)
    {
        emitCodepoint(c);
    }
    else if ((c & 0xE0) == 0xC0) { _u8cp = c & 0x1F; _u8need = 1; }
    else if ((c & 0xF0) == 0xE0) { _u8cp = c & 0x0F; _u8need = 2; }
    else if ((c & 0xF8) == 0xF0) { _u8cp = c & 0x07; _u8need = 3; }
    else
    {
        emit('?');   // a stray continuation byte, or something that is not UTF-8
    }
}

// ---- tags -----------------------------------------------------------------

void RssParser::handleTag()
{
    _tag[_tagLen] = 0;

    if (strcmp(_tag, "item") == 0)
    {
        if (!_closing)
        {
            _inItem    = true;
            _capture   = NONE;
            _titleLen  = 0;
            _title[0]  = 0;
            _haveTitle = false;
            _pubLen    = 0;
            _pubDate[0] = 0;
            _epoch     = 0;
        }
        else
        {
            commitItem();
            _inItem  = false;
            _capture = NONE;
        }
        return;
    }

    if (!_inItem)
        return;   // the channel's own <title>/<pubDate> never reach a buffer

    if (!_closing)
    {
        // Only the first of each per item: <source> follows <pubDate>, and a
        // second <title> anywhere in the item must not overwrite the headline.
        if (strcmp(_tag, "title") == 0 && !_haveTitle)
        {
            _capture  = TITLE;
            _titleLen = 0;
            _title[0] = 0;
            _entLen   = 0;
        }
        else if (strcmp(_tag, "pubdate") == 0 && _epoch == 0)
        {
            _capture    = PUBDATE;
            _pubLen     = 0;
            _pubDate[0] = 0;
            _entLen     = 0;
        }
        return;
    }

    if (strcmp(_tag, "title") == 0 && _capture == TITLE)
    {
        while (_titleLen > 0 && _title[_titleLen - 1] == ' ')
            _title[--_titleLen] = 0;
        _capture   = NONE;
        _haveTitle = true;
    }
    else if (strcmp(_tag, "pubdate") == 0 && _capture == PUBDATE)
    {
        _capture = NONE;
        _epoch   = ParsePubDate(_pubDate);
    }
}

// ---- items ----------------------------------------------------------------

void RssParser::commitItem()
{
    if (!_haveTitle || _titleLen == 0 || _epoch == 0)
        return;   // an item we cannot date is an item we cannot order

    if (_epoch <= _since)
        return;   // already seen. The feed is unordered, so keep reading.

    // Once full, an item only earns a place by being newer than the oldest one
    // held. The feed is not in publication order, so this cannot be done by
    // position -- the newest item may arrive last.
    int replace = -1;
    if (_count >= _maxItems)
    {
        int oldest = 0;
        for (int i = 1; i < _count; i++)
            if (_items[i].epoch < _items[oldest].epoch)
                oldest = i;

        if (_epoch <= _items[oldest].epoch)
            return;

        replace = oldest;
    }

    // Google appends the publisher: "Cabinet splits over welfare bill - BBC News".
    // Cut at the last " - " when what follows looks like a name rather than part
    // of the headline. A headline genuinely ending in " - X" loses its tail; that
    // is the accepted cost of a heuristic with no better signal to go on.
    if (_titleLen >= 3)
    {
        for (size_t i = _titleLen - 3; i > 0; i--)
        {
            if (memcmp(_title + i, " - ", 3) == 0)
            {
                size_t suffix = _titleLen - (i + 3);
                if (suffix > 0 && suffix < 40)
                {
                    _titleLen = i;
                    _title[i] = 0;
                }
                break;
            }
        }
    }

    // A title cut short at the buffer bound should say so.
    if (_titleLen == kMaxTitle - 1)
        memcpy(_title + _titleLen - 3, "...", 3);

    Item& it = (replace >= 0) ? _items[replace] : _items[_count];
    it.epoch = _epoch;
    memcpy(it.title, _title, _titleLen + 1);

    if (replace < 0)
        _count++;
}

// Oldest first. An insertion sort: there are never more than kMaxItemsCap of them.
void RssParser::Finish()
{
    for (int i = 1; i < _count; i++)
    {
        Item key = _items[i];
        int j = i - 1;
        while (j >= 0 && _items[j].epoch > key.epoch)
        {
            _items[j + 1] = _items[j];
            j--;
        }
        _items[j + 1] = key;
    }
}

// ---- the machine ----------------------------------------------------------

void RssParser::Feed(uint8_t c)
{
    switch (_state)
    {
        case TEXT:
            if (c == '<')
            {
                _entLen  = 0;      // an unterminated entity dies with its text run
                _u8need  = 0;      // and so does a truncated UTF-8 sequence
                _state   = LT;
                _tagLen  = 0;
                _closing = false;
            }
            else if (_capture != NONE)
            {
                feedText(c);
            }
            break;

        case LT:
            if (c == '!')      { _state = BANG; _match = 0; }
            else if (c == '?') { _state = PI_BODY; _match = 0; }
            else if (c == '/') { _closing = true; _state = TAG_NAME; }
            else if (isNameChar(c))
            {
                _state = TAG_NAME;
                _tag[_tagLen++] = lower((char)c);
            }
            else _state = TEXT;   // a stray '<' in text
            break;

        case BANG:
            if (_match == 0)
            {
                if (c == '-')      { _state = COMMENT_BODY; _match = 0; }
                else if (c == '[') { _match = 1; }
                else               { _state = SKIP_TO_GT; }
            }
            else
            {
                static const char kCdata[] = "[CDATA[";
                if (c == (uint8_t)kCdata[_match])
                {
                    if (++_match == 7) { _state = CDATA_BODY; _match = 0; }
                }
                else _state = SKIP_TO_GT;
            }
            break;

        case CDATA_BODY:
            // Everything here is literal: the <a> and <li> tags inside a
            // <description> must never be seen as markup.
            if (_match == 0 && c == ']')      _match = 1;
            else if (_match == 1 && c == ']') _match = 2;
            else if (_match == 2 && c == '>') { _state = TEXT; _match = 0; }
            else if (_match == 2 && c == ']') { if (_capture != NONE) emit(']'); }
            else
            {
                for (uint8_t i = 0; i < _match; i++)
                    if (_capture != NONE) emit(']');
                _match = 0;
                if (_capture != NONE) emit((char)c);   // no entity decoding in CDATA
            }
            break;

        case COMMENT_BODY:
            if (_match < 2) _match = (c == '-') ? _match + 1 : 0;
            else if (c == '>') { _state = TEXT; _match = 0; }
            else if (c != '-') _match = 0;
            break;

        case PI_BODY:
            if (c == '?')      _match = 1;
            else if (_match == 1 && c == '>') { _state = TEXT; _match = 0; }
            else _match = 0;
            break;

        case SKIP_TO_GT:
            if (c == '>') _state = TEXT;
            break;

        case TAG_NAME:
            if (isNameChar(c))
            {
                if (_tagLen < kMaxTagName - 1)
                    _tag[_tagLen++] = lower((char)c);
            }
            else
            {
                handleTag();
                if (c == '>') _state = TEXT;
                else          { _state = TAG_ATTRS; _quote = 0; }
            }
            break;

        case TAG_ATTRS:
            // Quote-aware, so a '>' inside url="..." cannot end the tag early.
            if (_quote)                       { if (c == _quote) _quote = 0; }
            else if (c == '"' || c == '\'')   _quote = (char)c;
            else if (c == '>')                _state = TEXT;
            break;
    }
}

// ---- dates ----------------------------------------------------------------

uint32_t RssParser::ParsePubDate(const char* s)
{
    if (!s) return 0;

    // Skip the optional "Wed, " day name.
    while (*s && !isDigit(*s)) s++;
    if (!*s) return 0;

    int day = 0;
    while (isDigit(*s)) day = day * 10 + (*s++ - '0');

    while (*s == ' ') s++;
    if (!s[0] || !s[1] || !s[2]) return 0;
    int month = monthFromName(s);
    if (month == 0) return 0;
    while (*s && *s != ' ') s++;
    while (*s == ' ') s++;

    int year = 0;
    while (isDigit(*s)) year = year * 10 + (*s++ - '0');
    while (*s == ' ') s++;

    int hh = 0, mm = 0, ss = 0;
    while (isDigit(*s)) hh = hh * 10 + (*s++ - '0');
    if (*s == ':') { s++; while (isDigit(*s)) mm = mm * 10 + (*s++ - '0'); }
    if (*s == ':') { s++; while (isDigit(*s)) ss = ss * 10 + (*s++ - '0'); }
    while (*s == ' ') s++;

    if (day < 1 || day > 31 || year < 1970 || hh > 23 || mm > 59 || ss > 60)
        return 0;

    // Zone. Google always says GMT; a numeric offset is handled anyway. An
    // alphabetic zone we do not know is treated as UTC, which is the RFC's own
    // advice and at worst misorders items by a few hours.
    long offset = 0;
    if (*s == '+' || *s == '-')
    {
        int sign = (*s == '-') ? -1 : 1;
        s++;
        int v = 0;
        for (int i = 0; i < 4 && isDigit(*s); i++) v = v * 10 + (*s++ - '0');
        offset = sign * ((v / 100) * 3600 + (v % 100) * 60);
    }

    long days = daysFromCivil(year, (unsigned)month, (unsigned)day);
    long long epoch = (long long)days * 86400 + hh * 3600 + mm * 60 + ss - offset;

    if (epoch <= 0 || epoch > 0xFFFFFFFFLL)
        return 0;

    return (uint32_t)epoch;
}
