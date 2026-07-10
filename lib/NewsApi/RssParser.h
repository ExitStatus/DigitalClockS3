#ifndef _RSS_PARSER_H
#define _RSS_PARSER_H

#include <stddef.h>
#include <stdint.h>

// An incremental RSS reader: bytes go in one at a time, headlines come out.
// Nothing is buffered but the headlines themselves, so a feed of any size costs
// the same few hundred bytes of RAM.
//
// Only <title> and <pubDate> inside an <item> are of interest, which is harder
// than it sounds. The channel carries a <title> and a <pubDate> of its own
// before the first item; <description> holds a CDATA block of raw HTML whose
// tags must not be mistaken for markup; and <source> has element text too. The
// state machine below is arranged so that none of those can be captured: text
// is only ever kept while we are inside an item AND inside the first <title> or
// <pubDate> of that item.
//
// Google's feed is NOT in publication order -- an item eighteen hours old can
// sit between two from this morning. So the whole feed is read, the newest
// `maxItems` are kept as it goes, and Finish() sorts them oldest-first. Nothing
// may assume the order bytes arrive in. (An earlier version stopped reading at
// the first item older than the watermark, which silently dropped newer items
// further down.)
//
// Text is sanitised to printable ASCII, because the display's fonts have no
// glyphs beyond it. XML entities are decoded, raw UTF-8 is decoded, the
// punctuation Google actually emits (curly quotes, en dashes) is folded to its
// ASCII equivalent, and anything else becomes '?'.
class RssParser
{
    public:
        static const size_t kMaxTitle    = 128;   // including the terminator
        static const int    kMaxItemsCap = 10;    // hard ceiling on maxItems

        struct Item
        {
            uint32_t epoch = 0;             // publication time, seconds since 1970 UTC
            char     title[kMaxTitle] = {}; // printable ASCII, publisher suffix stripped
        };

        // Keeps items published strictly after `since`, at most `maxItems` of them.
        RssParser(uint32_t since, int maxItems);

        void Feed(uint8_t byte);

        // Sorts the kept items oldest-first, which is the order they are shown in.
        void Finish();

        int         Count() const           { return _count; }
        const Item& At(int i) const         { return _items[i]; }

        // Parses an RFC-822 date ("Wed, 09 Jul 2026 12:34:56 GMT") to a UTC epoch.
        // Returns 0 if it cannot be understood. Exposed for testing.
        static uint32_t ParsePubDate(const char* s);

    private:
        enum State : uint8_t
        {
            TEXT,          // between tags
            LT,            // just saw '<'
            BANG,          // saw "<!" -- comment, CDATA or doctype
            CDATA_BODY,    // inside <![CDATA[ ... ]]>
            COMMENT_BODY,  // inside <!-- ... -->
            PI_BODY,       // inside <? ... ?>
            SKIP_TO_GT,    // a doctype or anything else we don't care about
            TAG_NAME,      // reading an element name
            TAG_ATTRS,     // skipping attributes to '>'
        };

        enum Capture : uint8_t { NONE, TITLE, PUBDATE };

        void handleTag();
        void feedText(uint8_t c);      // entity and UTF-8 decoding live here
        void emitCodepoint(long cp);   // fold a Unicode code point to ASCII
        void emit(char c);             // sanitise, then append to the active buffer
        void commitItem();

        static const size_t kMaxPubDate = 40;
        static const size_t kMaxTagName = 16;
        static const size_t kMaxEntity  = 10;   // "&#x2019;" and friends, plus slack

        State   _state   = TEXT;
        Capture _capture = NONE;

        bool _inItem  = false;
        bool _closing = false;   // the tag being read is a closing tag

        char   _tag[kMaxTagName] = {};
        size_t _tagLen = 0;

        // Matching progress through a literal ("[CDATA[", "]]>", "-->", "?>").
        uint8_t _match = 0;

        char   _quote = 0;       // the quote character we are inside, or 0

        char   _entity[kMaxEntity] = {};
        size_t _entLen = 0;      // 0 = not in an entity; otherwise chars buffered, '&' first

        // Part-built UTF-8 code point: Google emits curly quotes and dashes as
        // raw multi-byte UTF-8, not as entities.
        long    _u8cp   = 0;
        uint8_t _u8need = 0;     // continuation bytes still expected

        char   _title[kMaxTitle] = {};
        size_t _titleLen = 0;
        bool   _haveTitle = false;

        char   _pubDate[kMaxPubDate] = {};
        size_t _pubLen = 0;
        uint32_t _epoch = 0;

        uint32_t _since;
        int      _maxItems;

        Item _items[kMaxItemsCap];
        int  _count = 0;
};

#endif // _RSS_PARSER_H
