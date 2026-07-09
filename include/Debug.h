#ifndef _DEBUG_H
#define _DEBUG_H

#ifdef DEBUG

#define DPRINT(...)    Serial.print(__VA_ARGS__); Serial.flush()
#define DPRINTLN(...)  Serial.println(__VA_ARGS__); Serial.flush()
#define DPRINTF(MSG, ...)    Serial.printf(MSG, __VA_ARGS__); Serial.flush()

#else

#define DPRINT(...)     //blank line
#define DPRINTLN(...)   //blank line
#define DPRINTF(MSG, ...)    //blank line

#endif

#endif