#ifndef _WIFI_MANAGER_H
#define _WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>

// Coarse, presentation-friendly connection state, independent of the raw
// wl_status_t values. Enough to drive a status icon and app logic.
enum class WifiState
{
    Idle,          // Begin() not yet called
    Connecting,    // an attempt is in progress (initial connect or reconnect)
    Connected,     // associated and holding an IP address
    Disconnected   // link is down; waiting before the next attempt
};

// Non-blocking WiFi station manager.
//
// Usage: construct with credentials, call Begin() once in setup(), then call
// Update() every loop() iteration. It drives a small state machine that keeps
// the link up: it retries a failed connection and automatically reconnects
// after a dropout, and it never blocks (no delay(), no busy-wait loops).
//
// Association loss is noticed immediately, on the loop() that follows it. A
// link that has gone dead while still *reporting* itself associated is caught
// by a health probe every HealthCheckMs; see linkUp().
class WifiManager
{
    public:
        WifiManager(const char* ssid, const char* password);

        void Begin();     // kick off the first connection attempt
        void Update();     // advance the state machine; call frequently

        WifiState State() const { return _state; }
        bool IsConnected() const { return _state == WifiState::Connected; }

        // Signal strength as 0..100%. Returns 0 when not connected (RSSI is only
        // meaningful on an active link).
        int SignalPercent() const;

    private:
        void startAttempt();
        void setState(WifiState state);
        void resetAdapter();          // power-cycle the radio and reconnect
        void checkDisconnectEscalation();
        bool linkUp() const;          // associated *and* holding an IP address

        const char* _ssid;
        const char* _password;

        WifiState _state = WifiState::Idle;
        uint32_t  _stateSince = 0;    // millis() when the current state was entered
        uint32_t  _lastAttempt = 0;    // millis() of the last WiFi.begin()

        uint32_t  _lastConnectedMs = 0;   // millis() we were last connected (0 = never)
        bool      _adapterResetDone = false;   // 1-hour reset already done this outage
        uint32_t  _lastRssiLog = 0;       // throttle for DEBUG RSSI logging
        uint32_t  _lastHealthCheck = 0;   // millis() of the last link health probe

        // Tuning
        static const uint32_t ConnectTimeoutMs = 15000;      // abandon an attempt after this
        static const uint32_t RetryIntervalMs  = 5000;       // minimum gap between attempts
        static const uint32_t HealthCheckMs    = 10000;      // how often to probe a live link
        static const uint32_t ResetAfterMs     = 3600000;    // 1 hour down -> reset adapter
        static const uint32_t RebootAfterMs    = 14400000;   // 4 hours down -> reboot

        // Signal % calibration. Measured ~-62..-63 dBm right next to the router
        // (this board's small PCB antenna), so treat -65 dBm as full strength.
        static const int RssiFull = -65;   // dBm mapped to 100%
        static const int RssiNone = -90;   // dBm mapped to 0%
};

#endif // _WIFI_MANAGER_H
