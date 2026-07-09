#ifndef _WEATHER_GRAPHS_H
#define _WEATHER_GRAPHS_H

#include <TFT_eSPI.h>
#include <time.h>

#include "Config.h"
#include "WeatherApi.h"

// Each renders one full-screen hourly forecast page (today + tomorrow) into the
// sprite. 'today' is the current local date for the day labels, or nullptr if
// the clock has not synced. The caller clears the sprite first.
//
// A graph switched off in settings.ini is not declared or compiled at all, so
// referring to it there is a build error rather than a blank page.

#if ENABLED(GRAPH_TEMPERATURE)
void drawTemperatureGraph(TFT_eSprite* sprite, const WeatherApi& weather, const struct tm* today);
#endif

#if ENABLED(GRAPH_PRESSURE)
void drawPressureGraph(TFT_eSprite* sprite, const WeatherApi& weather, const struct tm* today);
#endif

#if ENABLED(GRAPH_WIND)
void drawWindGraph(TFT_eSprite* sprite, const WeatherApi& weather, const struct tm* today);
#endif

#if ENABLED(GRAPH_RAIN)
void drawRainGraph(TFT_eSprite* sprite, const WeatherApi& weather, const struct tm* today);
#endif

#if ENABLED(GRAPH_SNOW)
void drawSnowGraph(TFT_eSprite* sprite, const WeatherApi& weather, const struct tm* today);
#endif

#endif // _WEATHER_GRAPHS_H
