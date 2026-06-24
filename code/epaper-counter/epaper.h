#ifndef EPAPER_H
#define EPAPER_H

#include <Arduino.h>

void epaperSetup();
void epaperPaint(int count, const char* symbol);

inline const char* MOON = "\x42";
inline const char* SUN  = "\x45";

#endif