#ifndef _BRIGHTNESS_OVERLAY_H
#define _BRIGHTNESS_OVERLAY_H

#include <TFT_eSPI.h>

// A transient centred HUD showing the current backlight brightness. Call Show()
// on each button click (it (re)starts a countdown and refreshes the value); the
// panel draws itself over the frame while Active() and vanishes after the
// configured window. TakeDirty() lets the render loop know it must repaint.
class BrightnessOverlay
{
    public:
        explicit BrightnessOverlay(uint32_t showMs);

        void Show(int percent);        // (re)show with this %, restart the timer
        bool Active() const;           // true while the panel should be visible
        bool TakeDirty();              // true once after each Show(), then clears

        void Render(TFT_eSprite* sprite);   // draws only while Active()

    private:
        uint32_t _showMs;
        uint32_t _shownAt = 0;
        int      _percent = 0;
        bool     _visible = false;
        bool     _dirty   = false;
};

#endif // _BRIGHTNESS_OVERLAY_H
