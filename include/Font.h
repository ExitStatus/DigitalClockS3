#ifndef _FONT_H
#define _FONT_H

#include <stdlib.h>

// Embedded VLW (TFT_eSPI smooth-font) blobs. All are Cabin (SIL OFL), covering
// printable ASCII only -- every string the firmware draws is sanitised to that
// range. Each is named cabin<N> for the pixel size it was generated at, which is
// how the linker names the symbols too. See ATTRIBUTION.md for the licence and
// fonts/README.md for how they are regenerated.

extern const uint8_t cabin11[] asm("_binary_fonts_cabin11_vlw_start");  // graph labels
extern const uint8_t cabin12[] asm("_binary_fonts_cabin12_vlw_start");  // graph labels
extern const uint8_t cabin16[] asm("_binary_fonts_cabin16_vlw_start");  // forecast stat
extern const uint8_t cabin17[] asm("_binary_fonts_cabin17_vlw_start");  // date
extern const uint8_t cabin21[] asm("_binary_fonts_cabin21_vlw_start");  // temperature, wind, ticker, HUD
extern const uint8_t cabin26[] asm("_binary_fonts_cabin26_vlw_start");  // AM/PM superscript (compact clock)
extern const uint8_t cabin28[] asm("_binary_fonts_cabin28_vlw_start");  // AM/PM superscript (full clock)

#endif // _FONT_H
