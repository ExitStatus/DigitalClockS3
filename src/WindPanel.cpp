#include "WindPanel.h"
#include "Config.h"
#include "Font.h"
#include <math.h>

WindPanel::WindPanel()
{
}

// Linear blend between two RGB colours, t in [0,1].
static uint16_t blend(TFT_eSprite* s,
                      uint8_t r0, uint8_t g0, uint8_t b0,
                      uint8_t r1, uint8_t g1, uint8_t b1, float t)
{
    uint8_t r = r0 + (int)((r1 - r0) * t);
    uint8_t g = g0 + (int)((g1 - g0) * t);
    uint8_t b = b0 + (int)((b1 - b0) * t);
    return s->color565(r, g, b);
}

// green (<=10 mph) -> orange (25 mph) -> red (>=40 mph), with the thresholds
// expressed in whatever unit the speed arrives in.
uint16_t WindPanel::colourForSpeed(TFT_eSprite* s, float speed) const
{
    const float calm   = WIND_STOP(10);
    const float breezy = WIND_STOP(25);
    const float gale   = WIND_STOP(40);

    if (speed <= calm) return s->color565(46, 190, 120);
    if (speed >= gale) return s->color565(240, 40, 40);

    if (speed < breezy)
        return blend(s, 46, 190, 120,  243, 146, 40, (speed - calm) / (breezy - calm));   // green -> orange
    return blend(s, 243, 146, 40,  240, 40, 40, (speed - breezy) / (gale - breezy));      // orange -> red
}

// Draw a filled arrow (dart) pointing along a compass bearing (0 = up/N,
// clockwise). Screen y is down; bearing direction vector is (sin, -cos).
void WindPanel::drawArrow(TFT_eSprite* s, int cx, int cy, float bearingDeg, int radius, uint16_t colour) const
{
    float th = bearingDeg * PI / 180.0f;
    float fx = sinf(th),  fy = -cosf(th);   // forward (toward the tip)
    float rx = cosf(th),  ry = sinf(th);    // right (90 deg clockwise of forward)

    const float tip = radius;               // tip distance from centre
    const float back = radius * 0.7f;       // barbs behind centre
    const float notch = radius * 0.25f;
    const float wide = radius * 0.6f;       // half-width at the barbs

    int tipX = cx + lroundf(fx * tip),                 tipY = cy + lroundf(fy * tip);
    int rbX  = cx + lroundf(-fx * back + rx * wide),   rbY  = cy + lroundf(-fy * back + ry * wide);
    int nX   = cx + lroundf(-fx * notch),              nY   = cy + lroundf(-fy * notch);
    int lbX  = cx + lroundf(-fx * back - rx * wide),   lbY  = cy + lroundf(-fy * back - ry * wide);

    s->fillTriangle(tipX, tipY, rbX, rbY, nX, nY, colour);
    s->fillTriangle(tipX, tipY, nX, nY, lbX, lbY, colour);
}

int WindPanel::Render(TFT_eSprite* sprite, int rightX, float windSpeed, int windDegree, int baselineY)
{
    sprite->loadFont(gillsans24);   // match the temperature font/size

    char buffer[12];
    snprintf(buffer, sizeof(buffer), "%d " WIND_UNIT_LABEL, (int)lroundf(windSpeed));
    int textW = sprite->textWidth(buffer);

    int ascent = sprite->gFont.maxAscent;
    int arrowRadius = ascent * 5 / 8;    // ~25% taller than the text height
    int arrowBox = 2 * arrowRadius;
    int gap = 4;
    int totalW = arrowBox + gap + textW;
    int startX = rightX - totalW;

    // Arrow points in the direction the wind is blowing towards (wind_degree is
    // the direction it comes from, so add 180), coloured by speed.
    drawArrow(sprite, startX + arrowRadius, baselineY - ascent / 2,
              windDegree + 180, arrowRadius, colourForSpeed(sprite, windSpeed));

    sprite->setTextColor(TFT_WHITE, TFT_BLACK);
    sprite->setTextDatum(L_BASELINE);
    sprite->drawString(buffer, startX + arrowBox + gap, baselineY);

    return startX;
}
