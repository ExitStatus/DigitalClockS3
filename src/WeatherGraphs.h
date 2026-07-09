#ifndef _WEATHER_GRAPHS_H
#define _WEATHER_GRAPHS_H

#include <TFT_eSPI.h>
#include <time.h>

#include "WeatherApi.h"

// Each renders one full-screen hourly forecast page (today + tomorrow) into the
// sprite. 'today' is the current local date for the day labels, or nullptr if
// the clock has not synced. The caller clears the sprite first.
void drawTemperatureGraph(TFT_eSprite* sprite, const WeatherApi& weather, const struct tm* today);
void drawPressureGraph(TFT_eSprite* sprite, const WeatherApi& weather, const struct tm* today);
void drawWindGraph(TFT_eSprite* sprite, const WeatherApi& weather, const struct tm* today);
void drawRainGraph(TFT_eSprite* sprite, const WeatherApi& weather, const struct tm* today);
void drawSnowGraph(TFT_eSprite* sprite, const WeatherApi& weather, const struct tm* today);

#endif // _WEATHER_GRAPHS_H
