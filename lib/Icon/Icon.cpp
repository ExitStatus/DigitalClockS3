#include "Icon.h"

// The one PNG decoder instance (large, ~40 KB). Every user decodes on the loop
// thread, one at a time, so a single shared instance needs no locking -- but
// nothing may decode from a worker task.
PNG decoder;

// Icons. The name after the comma is the embedded file with '.' turned into
// '_', which is how the linker names the symbols for board_build.embed_files.
// Direct-initialised, not "Icon X = Icon(...)": Icon owns heap buffers and so is
// non-copyable, and the copy is only elided automatically from C++17 on.
#define ICON(VARNAME, FILENAME) \
    extern const uint8_t FILENAME##_start[] asm("_binary_images_" #FILENAME "_start"); \
    extern const uint8_t FILENAME##_end[] asm("_binary_images_" #FILENAME "_end"); \
    Icon VARNAME(FILENAME##_start, FILENAME##_end);

ICON(DownIcon, down_png);

namespace
{
    inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
    {
        return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    }

    // Where the decoder writes: the whole source image, colour and alpha apart.
    struct DecodeBuffer
    {
        uint16_t* pixels;
        uint8_t*  alpha;
        int  w;
        int  h;
        bool ok;
    };

    // PNGdec hands back a line in the file's own layout, so each is unpacked
    // here rather than asking the library to flatten it against a background --
    // that would throw the alpha away, which is the one thing worth keeping.
    //
    // Indexed images keep their alpha in the palette: the RGB triples fill the
    // first 768 bytes and entry c's alpha follows at pPalette[768 + c].
    int decodeLine(PNGDRAW* pDraw)
    {
        DecodeBuffer* buf = (DecodeBuffer*)pDraw->pUser;
        if (!buf->ok || pDraw->y >= buf->h)
            return 1;

        if (pDraw->iBpp != 8)
        {
            buf->ok = false;      // sub-byte or 16-bit channels: not handled
            return 0;
        }

        const uint8_t* src  = pDraw->pPixels;
        uint16_t*      outP = buf->pixels + (size_t)pDraw->y * buf->w;
        uint8_t*       outA = buf->alpha  + (size_t)pDraw->y * buf->w;

        const int n = (pDraw->iWidth < buf->w) ? pDraw->iWidth : buf->w;

        for (int i = 0; i < n; i++)
        {
            uint8_t r, g, b, a;

            switch (pDraw->iPixelType)
            {
                case PNG_PIXEL_INDEXED:
                {
                    const uint8_t c = src[i];
                    r = pDraw->pPalette[c * 3 + 0];
                    g = pDraw->pPalette[c * 3 + 1];
                    b = pDraw->pPalette[c * 3 + 2];
                    a = pDraw->iHasAlpha ? pDraw->pPalette[768 + c] : 255;
                    break;
                }
                case PNG_PIXEL_TRUECOLOR_ALPHA:
                    r = src[i * 4 + 0];
                    g = src[i * 4 + 1];
                    b = src[i * 4 + 2];
                    a = src[i * 4 + 3];
                    break;

                case PNG_PIXEL_TRUECOLOR:
                    r = src[i * 3 + 0];
                    g = src[i * 3 + 1];
                    b = src[i * 3 + 2];
                    a = 255;
                    break;

                case PNG_PIXEL_GRAYSCALE:
                    r = g = b = src[i];
                    a = 255;
                    break;

                case PNG_PIXEL_GRAY_ALPHA:
                    r = g = b = src[i * 2 + 0];
                    a = src[i * 2 + 1];
                    break;

                default:
                    buf->ok = false;
                    return 0;
            }

            outP[i] = rgb565(r, g, b);
            outA[i] = a;
        }

        return 1;
    }
}

Icon::Icon(const uint8_t* start, const uint8_t* end)
    : _data(start), _len((size_t)(end - start)), _inFlash(true)
{
}

Icon::~Icon()
{
    freeCache();
}

void Icon::freeCache()
{
    free(_pixels);
    free(_alpha);
    _pixels = nullptr;
    _alpha  = nullptr;
    _w = _h = 0;
    _ready = false;
}

bool Icon::LoadFromRam(const uint8_t* png, size_t len)
{
    if (png == nullptr || len == 0)
        return false;

    _data    = png;
    _len     = len;
    _inFlash = false;
    _ready   = false;
    return true;
}

bool Icon::Prepare(int targetHeight)
{
    if (_data == nullptr || _len == 0)
        return false;

    freeCache();

    int rc = _inFlash ? decoder.openFLASH((uint8_t*)_data, _len, decodeLine)
                      : decoder.openRAM((uint8_t*)_data, _len, decodeLine);
    if (rc != PNG_SUCCESS)
        return false;

    const int sw = decoder.getWidth();
    const int sh = decoder.getHeight();
    if (sw <= 0 || sh <= 0 || sw > kMaxSourceSide || sh > kMaxSourceSide)
        return false;

    DecodeBuffer buf;
    buf.w  = sw;
    buf.h  = sh;
    buf.ok = true;
    buf.pixels = (uint16_t*)malloc((size_t)sw * sh * sizeof(uint16_t));
    buf.alpha  = (uint8_t*)malloc((size_t)sw * sh);

    if (!buf.pixels || !buf.alpha)
    {
        free(buf.pixels);
        free(buf.alpha);
        return false;
    }

    if (decoder.decode(&buf, 0) != PNG_SUCCESS || !buf.ok)
    {
        free(buf.pixels);
        free(buf.alpha);
        return false;
    }

    // Crop to the bounding box of anything not fully transparent.
    int minX = sw, minY = sh, maxX = -1, maxY = -1;
    for (int y = 0; y < sh; y++)
    {
        for (int x = 0; x < sw; x++)
        {
            if (buf.alpha[y * sw + x] != 0)
            {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }

    if (maxX < minX)                       // nothing but transparency
    {
        free(buf.pixels);
        free(buf.alpha);
        return false;
    }

    const int cw = maxX - minX + 1;        // cropped content size
    const int ch = maxY - minY + 1;

    int th = (targetHeight > 0) ? targetHeight : ch;
    int tw = cw * th / ch;                 // keep the aspect ratio
    if (tw < 1) tw = 1;
    if (th < 1) th = 1;

    _pixels = (uint16_t*)malloc((size_t)tw * th * sizeof(uint16_t));
    _alpha  = (uint8_t*)malloc((size_t)tw * th);
    if (!_pixels || !_alpha)
    {
        freeCache();
        free(buf.pixels);
        free(buf.alpha);
        return false;
    }

    // Box-average each destination pixel over the source region it covers.
    // Colour is accumulated premultiplied by alpha and divided back out at the
    // end: a pixel that is one-tenth covered should contribute one-tenth of its
    // colour, not pull the average toward whatever an invisible pixel holds.
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

            uint32_t sr = 0, sg = 0, sb = 0, sa = 0, n = 0;

            for (int yy = sy0; yy < sy1 && yy < sh; yy++)
            {
                for (int xx = sx0; xx < sx1 && xx < sw; xx++)
                {
                    const uint16_t c = buf.pixels[yy * sw + xx];
                    const uint32_t a = buf.alpha[yy * sw + xx];

                    sr += (((c >> 11) & 0x1F) << 3) * a;
                    sg += (((c >> 5)  & 0x3F) << 2) * a;
                    sb += (( c        & 0x1F) << 3) * a;
                    sa += a;
                    n++;
                }
            }

            const int i = ty * tw + tx;
            if (n == 0 || sa == 0)
            {
                _pixels[i] = 0;
                _alpha[i]  = 0;
            }
            else
            {
                _pixels[i] = rgb565((uint8_t)(sr / sa), (uint8_t)(sg / sa), (uint8_t)(sb / sa));
                _alpha[i]  = (uint8_t)(sa / n);
            }
        }
    }

    free(buf.pixels);
    free(buf.alpha);

    _w = (uint16_t)tw;
    _h = (uint16_t)th;
    _ready = true;
    return true;
}

void Icon::Render(TFT_eSprite* dest, int x, int y) const
{
    if (!_ready || dest == nullptr)
        return;

    for (int iy = 0; iy < _h; iy++)
    {
        const int dy = y + iy;
        if (dy < 0 || dy >= dest->height())
            continue;

        for (int ix = 0; ix < _w; ix++)
        {
            const int dx = x + ix;
            if (dx < 0 || dx >= dest->width())
                continue;

            const int i = iy * _w + ix;
            const uint8_t a = _alpha[i];

            if (a == 0)
                continue;                      // leave the destination alone

            if (a == 255)
                dest->drawPixel(dx, dy, _pixels[i]);
            else
                dest->drawPixel(dx, dy, dest->alphaBlend(a, _pixels[i], dest->readPixel(dx, dy)));
        }
    }
}
