#include "Icon.h"
#include "ctype.h"

// Shared with WeatherIcon (src/WeatherIcon.cpp), which owns the definition. The
// PNG object is around 40 KB, so a second one is not affordable. Both users
// decode on the loop thread, one at a time, so sharing needs no locking -- but
// nothing here may decode from a worker task.
extern PNG decoder;

extern TFT_eSPI tft;

// Icons. The name after the comma is the embedded file with '.' turned into
// '_', which is how the linker names the symbols for board_build.embed_files.
#define ICON(VARNAME, FILENAME) \
    extern const uint8_t FILENAME##_start[] asm("_binary_images_" #FILENAME "_start"); \
    extern const uint8_t FILENAME##_end[] asm("_binary_images_" #FILENAME "_end"); \
    Icon VARNAME = Icon(FILENAME##_start, FILENAME##_end);

ICON(DownIcon, down_png);

// The PNGdec callbacks are plain functions, so what they need is passed through
// these rather than through a class instance.
int pngDrawImageWidth;
int pngDrawX;
int pngDrawY;
TFT_eSprite* pngSprite;

// Push one line, skipping the runs the alpha mask says are transparent.
void pushMaskedImage(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t* img, uint8_t* mask)
{
    uint8_t*  mptr = mask;
    uint8_t*  eptr = mask + ((w + 7) >> 3);
    uint16_t* iptr = img;
    uint32_t  setCount = 0;

    while (h--)
    {
        uint32_t xp = 0;
        uint32_t clearCount = 0;
        uint8_t  mbyte = *mptr++;
        uint32_t bits  = 8;

        do {
            setCount = 0;

            // Run of clear bits: how far to skip before drawing.
            while ((mbyte & 0x80) == 0x00)
            {
                if (mbyte == 0)                 // whole byte clear; skip it at once
                {
                    clearCount += bits;
                    if (mptr >= eptr) break;
                    mbyte = *mptr++;
                    bits  = 8;
                    continue;
                }
                mbyte = mbyte << 1;
                clearCount++;
                if (--bits) continue;
                if (mptr >= eptr) break;
                mbyte = *mptr++;
                bits  = 8;
            }

            // Run of set bits: how many pixels to draw.
            while ((mbyte & 0x80) == 0x80)
            {
                if (mbyte == 0xFF)              // whole byte set
                {
                    setCount += bits;
                    if (mptr >= eptr) break;
                    mbyte = *mptr++;
                    continue;
                }
                mbyte = mbyte << 1;
                setCount++;
                if (--bits) continue;
                if (mptr >= eptr) break;
                mbyte = *mptr++;
                bits  = 8;
            }

            if (setCount)
            {
                xp += clearCount;
                clearCount = 0;
                pngSprite->pushImage(x + xp, y, setCount, 1, iptr + xp);   // clips for us
                xp += setCount;
            }
        } while (setCount || mptr < eptr);

        y++;
        iptr += w;
        eptr += ((w + 7) >> 3);
    }
}

// PNGdec 1.1.6 wants an int back from a draw callback -- 1 to carry on, 0 to
// stop. The version this was ported from returned void.
int pngDraw(PNGDRAW* pDraw)
{
    uint16_t lineBuffer[pngDrawImageWidth];
    uint8_t  maskBuffer[1 + pngDrawImageWidth / 8];

    decoder.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_BIG_ENDIAN, 0xffffffff);

    if (decoder.getAlphaMask(pDraw, maskBuffer, 255))
        tft.pushMaskedImage(pngDrawX, pngDrawY + pDraw->y, pDraw->iWidth, 1, lineBuffer, maskBuffer);

    return 1;
}

int pngDrawSprite(PNGDRAW* pDraw)
{
    uint16_t lineBuffer[pngDrawImageWidth];
    uint8_t  maskBuffer[1 + pngDrawImageWidth / 8];

    decoder.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_BIG_ENDIAN, 0xffffffff);

    if (decoder.getAlphaMask(pDraw, maskBuffer, 255))
        pushMaskedImage(pngDrawX, pngDrawY + pDraw->y, pDraw->iWidth, 1, lineBuffer, maskBuffer);

    return 1;
}

// Soft-edged draw: one pixel at a time, each blended over what is already in the
// sprite. Two source layouts are handled, which between them cover the icons
// this project embeds:
//
//   * indexed (colour type 3), where the alpha for palette entry c lives at
//     pPalette[768 + c] -- the RGB triples occupy the first 768 bytes;
//   * truecolor+alpha (colour type 6), a straight RGBA byte per channel.
//
// Anything else falls back to the caller's masked path rather than guessing.
int pngDrawSpriteBlended(PNGDRAW* pDraw)
{
    const int y = pngDrawY + pDraw->y;
    if (y < 0 || y >= pngSprite->height())
        return 1;

    const uint8_t* src = pDraw->pPixels;
    const bool indexed = (pDraw->iPixelType == PNG_PIXEL_INDEXED);

    for (int i = 0; i < pDraw->iWidth; i++)
    {
        const int x = pngDrawX + i;
        if (x < 0 || x >= pngSprite->width())
            continue;

        uint8_t r, g, b, a;

        if (indexed)
        {
            const uint8_t c = src[i];
            r = pDraw->pPalette[c * 3 + 0];
            g = pDraw->pPalette[c * 3 + 1];
            b = pDraw->pPalette[c * 3 + 2];
            a = pDraw->iHasAlpha ? pDraw->pPalette[768 + c] : 255;
        }
        else
        {
            r = src[i * 4 + 0];
            g = src[i * 4 + 1];
            b = src[i * 4 + 2];
            a = src[i * 4 + 3];
        }

        if (a == 0)
            continue;                       // fully transparent: leave the pixel alone

        const uint16_t fg = pngSprite->color565(r, g, b);

        if (a == 255)
            pngSprite->drawPixel(x, y, fg);
        else
            pngSprite->drawPixel(x, y, pngSprite->alphaBlend(a, fg, pngSprite->readPixel(x, y)));
    }

    return 1;
}

Icon::Icon(const uint8_t* start, const uint8_t* end)
{
    _imageStart = (uint8_t*)start;
    _imageEnd   = (uint8_t*)end;

    // Only reads the header. Icons are globals, so this runs before setup(); it
    // touches nothing but flash and the (zero-initialised) decoder.
    if (decoder.openFLASH(_imageStart, _imageEnd - _imageStart, pngDraw) == PNG_SUCCESS)
    {
        _imageWidth  = decoder.getWidth();
        _imageHeight = decoder.getHeight();
    }
}

void Icon::Render(int x, int y)
{
    pngDrawX = x;
    pngDrawY = y;

    if (decoder.openFLASH(_imageStart, _imageEnd - _imageStart, pngDraw) == PNG_SUCCESS)
    {
        pngDrawImageWidth = _imageWidth;

        tft.startWrite();
        decoder.decode(NULL, 0);
        tft.endWrite();
    }
}

void Icon::Render(TFT_eSprite* sprite, int x, int y)
{
    pngDrawX  = x;
    pngDrawY  = y;
    pngSprite = sprite;

    // No startWrite()/endWrite() here: this draws into a sprite in RAM, so it
    // never touches the panel's SPI bus.
    if (decoder.openFLASH(_imageStart, _imageEnd - _imageStart, pngDrawSprite) == PNG_SUCCESS)
    {
        pngDrawImageWidth = _imageWidth;
        decoder.decode(NULL, 0);
    }
}

void Icon::RenderHighQuality(TFT_eSprite* sprite, int x, int y)
{
    pngDrawX  = x;
    pngDrawY  = y;
    pngSprite = sprite;

    if (decoder.openFLASH(_imageStart, _imageEnd - _imageStart, pngDrawSpriteBlended) != PNG_SUCCESS)
        return;

    // The blended callback only understands these two layouts. For anything else
    // the masked path is drawn instead: a hard edge is better than a wrong one.
    const int type = decoder.getPixelType();
    if ((type != PNG_PIXEL_INDEXED && type != PNG_PIXEL_TRUECOLOR_ALPHA) || decoder.getBpp() != 8)
    {
        Render(sprite, x, y);
        return;
    }

    pngDrawImageWidth = _imageWidth;
    decoder.decode(NULL, 0);
}

uint16_t Icon::Width()
{
    return _imageWidth;
}

uint16_t Icon::Height()
{
    return _imageHeight;
}
