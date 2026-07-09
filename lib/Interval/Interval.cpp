#include "Interval.h"

Interval::Interval()
{
    _intervalPeriod = _every = 1000;
    _last = millis();
}

Interval::Interval(uint32_t e, bool now)
{
    _intervalPeriod = _every = e;
    _last = millis();
    _now = now;
}

void Interval::Enable()
{
    _enabled = true;
}

void Interval::Disable()
{
    _enabled = false;
}

void Interval::Start(uint32_t e, bool now)
{
    _intervalPeriod = _every = e;
    _last = millis();
    _now = now;
}

void Interval::Now()
{
    _now = true;
}

void Interval::Reset()
{
    _last = millis();
    _now = false;
}

void Interval::Reset(uint32_t e)
{
    _intervalPeriod = _every = e;
    _last = millis();
    _now = false;
}

uint32_t Interval::GetInterval()
{
    return _intervalPeriod;
}

bool Interval::Ready()
{
    if (!_enabled)
        return false;

    uint32_t current = millis();

    if (current < _last)
        _last = 0;

    if (current - _last >= _every || _now)
    {
        _last = current;
        _now = false;
        _every = _intervalPeriod;

        return true;
    }
    else
        return false;
}

void Interval::RetryIn(int retryTime)
{
    _every = retryTime;
}
