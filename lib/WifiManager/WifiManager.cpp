#include "WifiManager.h"
#include "Debug.h"

WifiManager::WifiManager(const char* ssid, const char* password)
    : _ssid(ssid), _password(password)
{
}

void WifiManager::Begin()
{
    WiFi.persistent(false);       // don't wear flash re-storing credentials each begin
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    startAttempt();
}

void WifiManager::Update()
{
    switch (_state)
    {
        case WifiState::Idle:
            // Nothing to do until Begin() starts the first attempt.
            break;

        case WifiState::Connecting:
            if (WiFi.status() == WL_CONNECTED)
                setState(WifiState::Connected);
            else if (millis() - _stateSince >= ConnectTimeoutMs)
                setState(WifiState::Disconnected);   // attempt timed out; retry later
            break;

        case WifiState::Connected:
            if (WiFi.status() != WL_CONNECTED)
                setState(WifiState::Disconnected);   // dropout detected
            break;

        case WifiState::Disconnected:
            if (millis() - _lastAttempt >= RetryIntervalMs)
                startAttempt();
            break;
    }

    checkDisconnectEscalation();
}

// Escalate on a prolonged outage: reset the adapter after 1 hour down, reboot
// after 4 hours. Disconnected duration is measured from the last time we were
// connected (0 at boot, so a never-connecting device escalates from power-on).
void WifiManager::checkDisconnectEscalation()
{
    if (_state == WifiState::Connected)
    {
        _lastConnectedMs = millis();
        _adapterResetDone = false;    // arm escalation for the next outage
#ifdef DEBUG
        if (millis() - _lastRssiLog >= 5000)
        {
            _lastRssiLog = millis();
            DPRINTF("WifiManager: RSSI %d dBm -> %d%%\n", WiFi.RSSI(), SignalPercent());
        }
#endif
        return;
    }

    uint32_t downMs = millis() - _lastConnectedMs;

    if (downMs >= RebootAfterMs)
    {
        DPRINTLN("WifiManager: disconnected > 4h, rebooting device");
        delay(100);                   // let the log flush
        ESP.restart();
    }
    else if (downMs >= ResetAfterMs && !_adapterResetDone)
    {
        DPRINTLN("WifiManager: disconnected > 1h, resetting WiFi adapter");
        resetAdapter();
        _adapterResetDone = true;
    }
}

void WifiManager::resetAdapter()
{
    WiFi.disconnect(true);            // disconnect and power down the radio
    WiFi.mode(WIFI_OFF);
    delay(200);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    startAttempt();                   // fresh connection attempt
}

void WifiManager::startAttempt()
{
    DPRINTF("WifiManager: connecting to %s\n", _ssid);

    _lastAttempt = millis();
    WiFi.disconnect(false);       // drop any half-open association, keep radio on
    WiFi.begin(_ssid, _password);
    setState(WifiState::Connecting);
}

int WifiManager::SignalPercent() const
{
    if (_state != WifiState::Connected)
        return 0;

    // Linear map from RSSI (dBm) to a percentage, calibrated so full strength at
    // this location (RssiFull) reads 100% and RssiNone reads 0%.
    int pct = (WiFi.RSSI() - RssiNone) * 100 / (RssiFull - RssiNone);
    if (pct < 0)   pct = 0;
    if (pct > 100) pct = 100;
    return pct;
}

void WifiManager::setState(WifiState state)
{
    if (state == _state)
        return;

    _state = state;
    _stateSince = millis();
    DPRINTF("WifiManager: state -> %d\n", (int)state);
}
