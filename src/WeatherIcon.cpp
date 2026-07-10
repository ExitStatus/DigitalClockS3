#include "WeatherIcon.h"

// The TFT_eSPI* is no longer needed: Icon caches plain arrays rather than a
// sprite, and draws straight into whatever sprite Render() is handed. The
// parameter stays so the call sites do not have to change.
WeatherIcon::WeatherIcon(TFT_eSPI*)
{
}

bool WeatherIcon::Load(const uint8_t* png, size_t len, int targetHeight)
{
    if (png == nullptr || len == 0 || targetHeight <= 0)
        return false;

    return _icon.LoadFromRam(png, len) && _icon.Prepare(targetHeight);
}

void WeatherIcon::Render(TFT_eSprite* dest, int x, int y)
{
    _icon.Render(dest, x, y);
}
