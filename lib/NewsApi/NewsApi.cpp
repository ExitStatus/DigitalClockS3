#include "NewsApi.h"

#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#include "Config.h"
#include "Debug.h"

// "GB" is two letters plus a terminator. Catches "G" and "GBR", which would
// otherwise reach Google as a country nobody has heard of and return an empty
// feed with a 200.
static_assert(sizeof(NEWS_COUNTRY) == 3, "settings.ini: news_country must be two letters");

namespace
{
    // Somewhere to pour the response body. HTTPClient::writeToStream() undoes
    // chunked transfer-encoding on the way through, which is why the body is
    // pushed here rather than pulled from getStreamPtr() -- the raw socket still
    // carries the chunk markers (see the comment in WeatherApi::fetchForecast).
    //
    // Bytes go straight into the parser, so however big the feed is, only the
    // headlines are ever held in memory. The whole body has to be read: Google's
    // feed is not in publication order, so the newest item may be the last one.
    // The deadline is the only thing that cuts a download short, and closing the
    // socket is what unblocks writeToStream().
    class RssSink : public Stream
    {
        public:
            RssSink(RssParser& parser, WiFiClient& client, uint32_t deadlineMs)
                : _parser(parser), _client(client), _deadline(deadlineMs) {}

            size_t write(uint8_t b) override
            {
                if (_stopped) return 1;
                _parser.Feed(b);
                return 1;
            }

            size_t write(const uint8_t* buf, size_t len) override
            {
                if (_stopped) return len;
                for (size_t i = 0; i < len; i++)
                    _parser.Feed(buf[i]);
                if ((int32_t)(millis() - _deadline) > 0) stop();
                return len;
            }

            // Nothing ever reads from this end.
            int available() override { return 0; }
            int read() override      { return -1; }
            int peek() override      { return -1; }

        private:
            void stop()
            {
                if (_stopped) return;
                _stopped = true;
                _client.stop();
            }

            RssParser&  _parser;
            WiFiClient& _client;
            uint32_t    _deadline;
            bool        _stopped = false;
    };

    // The feed is always requested in English; only the edition changes.
    String buildUrl(const char* country)
    {
        String c(country);
        return String("https://news.google.com/rss?hl=en-") + c + "&gl=" + c + "&ceid=" + c + ":en";
    }
}

NewsApi::NewsApi(const char* country, int maxItems, uint32_t intervalMs, uint32_t firstFetchMs)
    : _url(buildUrl(country)),
      _maxItems(maxItems),
      _firstFetchMs(firstFetchMs),
      _refresh(intervalMs, false)
{
}

void NewsApi::Begin(SemaphoreHandle_t netLock)
{
    _netLock = netLock;

    _prefs.begin("news", false);
    _lastViewed = _prefs.getUInt("lastViewed", 0);
    DPRINTF("NewsApi: watermark %u\n", (unsigned)_lastViewed);

    _lock = xSemaphoreCreateMutex();
    if (!_lock)
    {
        DPRINTLN("NewsApi: could not create mutex");
        return;
    }

    BaseType_t ok = xTaskCreatePinnedToCore(taskEntry, "news", kTaskStack,
                                            this, kTaskPriority, &_task, kTaskCore);
    if (ok != pdPASS)
    {
        DPRINTLN("NewsApi: could not create worker task");
        _task = nullptr;
        return;
    }

    // Hold the first fetch back so the clock and the weather have the network,
    // and the screen, to themselves on a cold boot.
    _refresh.RetryIn(_firstFetchMs);
}

void NewsApi::taskEntry(void* arg)
{
    static_cast<NewsApi*>(arg)->taskLoop();
}

// Caller thread. Adopt anything the worker has finished, then decide whether to
// ask for more.
void NewsApi::Update(bool wifiConnected)
{
    collect();

    if (!wifiConnected || !_task)
        return;

    // Ready() is consuming, so its result has to be acted on now or lost.
    if (_refresh.Ready())
        dispatch();
}

void NewsApi::dispatch()
{
    xSemaphoreTake(_lock, portMAX_DELAY);
    bool alreadyBusy = _pending;
    if (!alreadyBusy)
    {
        _pending = true;
        _since   = _lastViewed;   // worker-only from here until it finishes
    }
    xSemaphoreGive(_lock);

    if (!alreadyBusy)
        xTaskNotifyGive(_task);
}

void NewsApi::collect()
{
    Batch b;
    bool have = false;

    xSemaphoreTake(_lock, portMAX_DELAY);
    if (_done)
    {
        b       = _staged;
        _staged = Batch();
        _done   = false;
        have    = true;
    }
    xSemaphoreGive(_lock);

    if (have)
        applyBatch(b);   // outside the lock
}

// Caller thread. Retry policy lives here, alongside the Interval it drives, for
// the same reason WeatherApi keeps it on this side: the worker should own no
// scheduling.
void NewsApi::applyBatch(const Batch& b)
{
    if (!b.ok)
    {
        if (_retries < kMaxRetries)
        {
            _retries++;
            _refresh.RetryIn(kFetchRetryMs);
            DPRINTF("NewsApi: fetch failed, retry %d/%d shortly\n", _retries, kMaxRetries);
        }
        else
        {
            _retries = 0;
            DPRINTLN("NewsApi: retries exhausted; waiting for normal interval");
        }
        return;
    }

    _retries = 0;

    // Drop anything already shown, and anything already queued: a refresh can
    // land while headlines are still on screen.
    for (int i = 0; i < b.count; i++)
    {
        const RssParser::Item& in = b.items[i];

        if (in.epoch <= _lastViewed)
            continue;

        bool dup = false;
        for (int j = _head; j < _count && !dup; j++)
            dup = (_queue[j].epoch == in.epoch);
        if (dup)
            continue;

        if (_count >= RssParser::kMaxItemsCap)
        {
            // Compact the shown headlines out of the way and try again.
            if (_head == 0)
                break;                       // genuinely full of unshown items
            for (int j = _head; j < _count; j++)
                _queue[j - _head] = _queue[j];
            _count -= _head;
            _head = 0;
        }

        _queue[_count++] = in;
    }

    DPRINTF("NewsApi: %d headline(s) queued\n", _count - _head);
    for (int i = _head; i < _count; i++)
        DPRINTF("  [%u] %s\n", (unsigned)_queue[i].epoch, _queue[i].title);
}

const RssParser::Item* NewsApi::NextHeadline() const
{
    return HasHeadline() ? &_queue[_head] : nullptr;
}

void NewsApi::MarkShown(uint32_t epoch)
{
    if (epoch > _lastViewed)
    {
        _lastViewed = epoch;
        _prefs.putUInt("lastViewed", epoch);
    }

    if (_head < _count)
        _head++;

    if (_head >= _count)   // queue drained
        _head = _count = 0;
}

// ---- worker ---------------------------------------------------------------

void NewsApi::taskLoop()
{
    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        Batch result;
        _fetching = true;
        result.ok = fetch(result);
        _fetching = false;

        xSemaphoreTake(_lock, portMAX_DELAY);
        _staged  = result;
        _pending = false;
        _done    = true;
        xSemaphoreGive(_lock);

        DPRINTF("NewsApi: worker stack headroom %u bytes\n",
                (unsigned)(uxTaskGetStackHighWaterMark(nullptr) * sizeof(StackType_t)));
    }
}

bool NewsApi::fetch(Batch& out)
{
    // Wait our turn for the network. Skipping a cycle is better than queueing
    // behind a peer that is itself retrying; the next interval comes round soon.
    if (_netLock && xSemaphoreTake(_netLock, pdMS_TO_TICKS(kNetLockWaitMs)) != pdTRUE)
    {
        DPRINTLN("NewsApi: network busy, skipping this cycle");
        return false;
    }

    DPRINTLN("NewsApi: fetching headlines");

    bool ok = false;
    {
        WiFiClientSecure client;
        client.setInsecure();

        HTTPClient http;
        if (http.begin(client, _url))
        {
            http.setTimeout(kHttpTimeoutMs);
            http.setUserAgent("DigitalClockS3");

            int code = http.GET();
            if (code == HTTP_CODE_OK)
            {
                RssParser parser(_since, _maxItems);
                RssSink   sink(parser, client, millis() + kReadDeadlineMs);

                // A negative return means the body was cut short -- by the
                // deadline, or by the peer. Whatever was parsed before that is
                // still usable, so the parser's state decides, not this value.
                http.writeToStream(&sink);
                parser.Finish();

                out.count = parser.Count();
                for (int i = 0; i < out.count; i++)
                    out.items[i] = parser.At(i);

                ok = true;
            }
            else
            {
                DPRINTF("NewsApi: HTTP %d\n", code);
            }

            http.end();
        }
    }   // client destroyed, TLS memory returned, before the lock is released

    if (_netLock)
        xSemaphoreGive(_netLock);

    return ok;
}
