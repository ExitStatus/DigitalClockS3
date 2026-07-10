#ifndef _NEWS_API_H
#define _NEWS_API_H

#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "Interval.h"
#include "RssParser.h"

// Google News headlines, fetched on a worker task so loop() never blocks.
//
// The shape is deliberately the same as WeatherApi's: a worker on core 0 does
// the network work and parks the result in a staging buffer; the caller adopts
// it from Update(), on the loop thread, before anything is rendered. The worker
// never touches the queue the display reads, so rendering needs no locks.
//
// Headlines are shown oldest-first. The publication time of the last one shown
// is kept in NVS, so a reboot does not replay the news you have already read.
// Anything at or older than that watermark is discarded at parse time.
//
// Update() must be called from loop(), never from inside a render.
class NewsApi
{
    public:
        // firstFetchMs staggers the first fetch away from the weather fetches;
        // afterwards the cadence is intervalMs.
        NewsApi(const char* country, int maxItems, uint32_t intervalMs, uint32_t firstFetchMs);

        // netLock serialises TLS with the other network users. See the comment
        // on _netLock below.
        void Begin(SemaphoreHandle_t netLock);
        void Update(bool wifiConnected);

        // True while the worker has a fetch in flight, including the time spent
        // waiting for the network lock. See WeatherApi::Busy() for why this needs
        // no locking.
        bool Busy() const { return _fetching; }

        bool HasHeadline() const { return _head < _count; }

        // The oldest headline not yet shown, or nullptr. Valid until MarkShown().
        const RssParser::Item* NextHeadline() const;

        // Records that a headline has been seen: advances the queue and the
        // watermark, and writes the watermark through to NVS.
        void MarkShown(uint32_t epoch);

    private:
        struct Batch
        {
            bool ok = false;
            int  count = 0;
            RssParser::Item items[RssParser::kMaxItemsCap];
        };

        static void taskEntry(void* arg);
        void taskLoop();
        void dispatch();
        void collect();
        void applyBatch(const Batch& b);   // caller thread
        bool fetch(Batch& out);            // worker thread

        static const uint32_t kFetchRetryMs  = 30000;
        static const uint8_t  kMaxRetries    = 3;
        static const uint32_t kHttpTimeoutMs = 8000;

        // The whole feed has to be read -- it is not in publication order -- and
        // it runs to a few hundred KB. This caps how long that may take, and so
        // how long the network lock is held.
        static const uint32_t kReadDeadlineMs = 15000;

        // How long the worker will wait for the network lock before giving up on
        // this cycle. Long enough to sit out a weather fetch, short enough that a
        // wedged peer does not park the task forever.
        static const uint32_t kNetLockWaitMs = 20000;

        // Measured headroom at 10240 was 2620 bytes: enough, but not enough to be
        // comfortable with TLS and the parser on the same stack.
        static const uint32_t    kTaskStack    = 12288;
        static const UBaseType_t kTaskPriority = 1;
        static const BaseType_t  kTaskCore     = 0;

        String   _url;
        int      _maxItems;
        uint32_t _firstFetchMs;

        // ---- caller thread only ----
        Preferences _prefs;
        uint32_t    _lastViewed = 0;   // epoch of the newest headline shown
        Interval    _refresh;
        uint8_t     _retries = 0;

        RssParser::Item _queue[RssParser::kMaxItemsCap];
        int _count = 0;   // headlines in the queue
        int _head  = 0;   // index of the next one to show

        // ---- shared, guarded by _lock ----
        SemaphoreHandle_t _lock = nullptr;
        TaskHandle_t      _task = nullptr;
        bool  _pending = false;   // requested, not yet picked up
        bool  _done    = false;   // finished, not yet adopted
        Batch _staged;

        // Held only around the HTTP/TLS section, and never at the same time as
        // _lock. A live WiFiClientSecure costs tens of KB of heap; this is what
        // guarantees only one exists at a time across the whole firmware.
        SemaphoreHandle_t _netLock = nullptr;

        // Written by the worker, read by the loop thread. See Busy().
        volatile bool _fetching = false;

        // Worker-only: the watermark as it stood when the fetch was dispatched.
        uint32_t _since = 0;
};

#endif // _NEWS_API_H
