#include "NtpClient.h"
#include "Debug.h"

NtpClient::NtpClient(const char* tz, const char* server)
    : _tz(tz), _server(server),
      _retry(10000, true),      // fire immediately the first time WiFi is up
      _refresh(3600000)         // one hour
{
}

void NtpClient::Begin()
{
    // Intervals are already armed; the first Update() with WiFi up requests a sync.
}

void NtpClient::Update(bool wifiConnected)
{
    if (!wifiConnected)
        return;                 // SNTP can't reach a server without a link

    if (!_hasTime)
    {
        // Poll for the answer every loop (cheap); (re)request every 10 s.
        struct tm now;
        if (readClock(now))
        {
            // tzset() ran in requestSync() while the clock was still at 1970, so
            // newlib fixed the DST boundary to the wrong year. Re-apply the
            // timezone now that the clock holds the real date, then re-read, so
            // localtime_r() evaluates BST/GMT correctly.
            applyTimezone();
            readClock(now);

            _hasTime = true;
            _refresh.Reset();   // start the hourly cycle from the moment we synced
#ifdef DEBUG
            time_t epoch = time(nullptr);
            struct tm utc;
            gmtime_r(&epoch, &utc);
            char localBuf[48], utcBuf[16];
            strftime(localBuf, sizeof(localBuf), "%d %B, %Y  %H:%M:%S", &now);
            strftime(utcBuf, sizeof(utcBuf), "%H:%M:%S", &utc);
            DPRINTF("NtpClient: time acquired -> local %s  (UTC %s)\n", localBuf, utcBuf);
#endif
            return;
        }

        if (_retry.Ready())
            requestSync();
    }
    else
    {
        if (_refresh.Ready())
        {
            DPRINTLN("NtpClient: hourly refresh");
            // Non-destructive: this only (re)starts the background SNTP poller.
            // If the server errors or is unreachable the system clock keeps
            // free-running from the last value; nothing here can clear the time.
            requestSync();
        }
    }
}

void NtpClient::requestSync()
{
    DPRINTF("NtpClient: requesting time from %s (tz %s)\n", _server, _tz);
    configTzTime(_tz, _server);   // starts SNTP (system clock kept in UTC) and sets TZ
    applyTimezone();
}

void NtpClient::applyTimezone()
{
    setenv("TZ", _tz, 1);
    tzset();
}

bool NtpClient::readClock(struct tm& out) const
{
    time_t now = time(nullptr);
    if (now < kValidEpoch)
        return false;           // clock not set yet

    localtime_r(&now, &out);    // apply the timezone/DST configured by configTzTime
    return true;
}

bool NtpClient::GetTime(struct tm& out) const
{
    // Once synced, the system clock free-runs from the crystal-backed RTC, so it
    // keeps advancing regardless of NTP failures. Should a live read ever fail,
    // fall back to the last value we read so the clock never reverts to unknown.
    if (!_hasTime)
        return false;

    if (readClock(out))
    {
        _lastGood = out;
        _haveLastGood = true;
        return true;
    }

    if (_haveLastGood)
    {
        out = _lastGood;
        return true;
    }
    return false;
}
