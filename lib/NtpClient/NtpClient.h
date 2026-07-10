#ifndef _NTP_CLIENT_H
#define _NTP_CLIENT_H

#include <Arduino.h>
#include <time.h>

#include "Interval.h"

// Keeps the system clock synced via SNTP.
//
// Call Begin() once, then Update(wifiConnected) every loop() iteration. While it
// has no time yet it retries every 10 s (only while WiFi is up); once synced it
// refreshes every hour. GetTime() reports the current local time (timezone/DST
// applied) once HasTime() is true.
class NtpClient
{
    public:
        // Default timezone is UK (GMT/BST with automatic daylight saving).
        NtpClient(const char* tz = "GMT0BST,M3.5.0/1,M10.5.0",
                  const char* server = "pool.ntp.org");

        void Begin();
        void Update(bool wifiConnected);

        bool HasTime() const { return _hasTime; }
        bool GetTime(struct tm& out) const;   // true and fills 'out' if time is known

        // True between asking SNTP for the time and hearing back. Unlike the
        // weather and news fetches there is no call to sit inside: configTzTime()
        // hands off to a background poller and returns at once, so this tracks a
        // completion callback instead. A sync that never answers is given up on
        // after kSyncTimeoutMs, otherwise an unreachable server would leave the
        // indicator stuck on forever.
        bool Busy() const { return _syncPending; }

    private:
        void requestSync();
        void applyTimezone();   // setenv(TZ) + tzset()
        bool readClock(struct tm& out) const;

        const char* _tz;
        const char* _server;
        bool _hasTime = false;   // latched true after the first sync; never cleared

        // Last time successfully read, kept as a fallback so GetTime() can always
        // report a time once synced, even if a live read ever fails.
        mutable struct tm _lastGood {};
        mutable bool      _haveLastGood = false;

        Interval _retry;     // 10 s between attempts while waiting for first sync
        Interval _refresh;   // 1 hour between refreshes once synced

        bool     _syncPending = false;   // a sync was asked for; no answer yet
        uint32_t _syncStart = 0;         // millis() when it was asked for

        // Epoch (seconds) below which the clock is considered "not yet set".
        // 1700000000 = 2023-11-14, safely after any real sync, before now.
        static const time_t kValidEpoch = 1700000000;

        // How long to keep showing a sync as in flight before writing it off.
        static const uint32_t kSyncTimeoutMs = 15000;
};

#endif // _NTP_CLIENT_H
