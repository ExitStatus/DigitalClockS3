#include "WeatherIcon.h"
#include <PNGdec.h>

// The single PNG decoder instance (large, ~40 KB). All decoding happens on the
// one loop thread, so a single shared instance is fine.
PNG decoder;

// Full decoded image, composited on black (transparent -> 0x0000).
struct DecodeBuffer
{
    uint16_t* pixels;
    int w;
    int h;
};

// PNGdec draw callback: copy each source scanline into the full buffer.
static int pngLineDraw(PNGDRAW* pDraw)
{
    DecodeBuffer* buf = (DecodeBuffer*)pDraw->pUser;
    if (pDraw->y >= buf->h)
        return 1;

    // Composite transparency against black so trimming can treat 0x0000 as empty.
    decoder.getLineAsRGB565(pDraw, buf->pixels + pDraw->y * buf->w,
                            PNG_RGB565_LITTLE_ENDIAN, 0x00000000);
    return 1;
}

WeatherIcon::WeatherIcon(TFT_eSPI* tft)
    : _sprite(tft)
{
}

bool WeatherIcon::Load(const uint8_t* png, size_t len, int targetHeight)
{
    _ready = false;
    if (png == nullptr || len == 0 || targetHeight <= 0)
        return false;

    if (decoder.openRAM((uint8_t*)png, len, pngLineDraw) != PNG_SUCCESS)
        return false;

    int sw = decoder.getWidth();
    int sh = decoder.getHeight();
    if (sw <= 0 || sh <= 0 || sw > 256 || sh > 256)
        return false;

    DecodeBuffer buf;
    buf.w = sw;
    buf.h = sh;
    buf.pixels = (uint16_t*)malloc((size_t)sw * sh * sizeof(uint16_t));
    if (buf.pixels == nullptr)
        return false;

    if (decoder.decode(&buf, 0) != PNG_SUCCESS)
    {
        free(buf.pixels);
        return false;
    }

    // Trim: find the bounding box of non-transparent (non-black) pixels.
    int minX = sw, minY = sh, maxX = -1, maxY = -1;
    for (int y = 0; y < sh; y++)
    {
        for (int x = 0; x < sw; x++)
        {
            if (buf.pixels[y * sw + x] != 0x0000)
            {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }
    if (maxX < minX)                     // fully transparent image
    {
        free(buf.pixels);
        return false;
    }

    int cw = maxX - minX + 1;            // trimmed content size
    int ch = maxY - minY + 1;

    int th = targetHeight;               // scale to target height, keep aspect
    int tw = cw * th / ch;
    if (tw < 1)  tw = 1;
    if (tw > 64) tw = 64;

    _sprite.deleteSprite();
    _sprite.setColorDepth(16);
    _sprite.createSprite(tw, th);
    _sprite.fillSprite(TFT_BLACK);

    // Box-average each target pixel from its source region within the crop.
    for (int ty = 0; ty < th; ty++)
    {
        int sy0 = minY + ty * ch / th;
        int sy1 = minY + (ty + 1) * ch / th;
        if (sy1 <= sy0) sy1 = sy0 + 1;

        for (int tx = 0; tx < tw; tx++)
        {
            int sx0 = minX + tx * cw / tw;
            int sx1 = minX + (tx + 1) * cw / tw;
            if (sx1 <= sx0) sx1 = sx0 + 1;

            uint32_t sr = 0, sg = 0, sb = 0, n = 0;
            for (int yy = sy0; yy < sy1 && yy < sh; yy++)
            {
                for (int xx = sx0; xx < sx1 && xx < sw; xx++)
                {
                    uint16_t c = buf.pixels[yy * sw + xx];
                    sr += ((c >> 11) & 0x1F) << 3;
                    sg += ((c >> 5)  & 0x3F) << 2;
                    sb += ( c        & 0x1F) << 3;
                    n++;
                }
            }
            if (n)
                _sprite.drawPixel(tx, ty, _sprite.color565(sr / n, sg / n, sb / n));
        }
    }

    free(buf.pixels);
    _w = tw;
    _h = th;
    _ready = true;
    return true;
}

void WeatherIcon::Render(TFT_eSprite* dest, int x, int y)
{
    if (_ready)
        _sprite.pushToSprite(dest, x, y);
}
