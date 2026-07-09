#ifndef Interval_h
#define Interval_h

#include "Arduino.h"

class Interval
{
  private:
    uint32_t _every = 1000;
    uint32_t _intervalPeriod;
    uint32_t _last = 0;
    bool _now = false;
    bool _enabled = true;

  public:
    Interval();
    Interval(uint32_t e, bool now = false);
    void Start(uint32_t e, bool now);
    void Now();
    bool Ready();
    void Reset();
    void Reset(uint32_t e);
    uint32_t GetInterval();

    void Enable();
    void Disable();
    void RetryIn(int retryTime);
};

#endif
