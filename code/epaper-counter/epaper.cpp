#include <GxEPD2_BW.h>
#include <Adafruit_GFX.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include "LiberationSans_Bold64pt7b.h"
#include <U8g2_for_Adafruit_GFX.h>
#include <SPI.h>
#include "epaper.h"

// epaper interface
#define BUSY          5   // purple
#define RST           4   // salmon
#define DC            3   // white
#define CS            10  // blue
#define SCL           7   // green
#define SDA           6   // yellow

#define SCREEN_WIDTH  200
#define SCREEN_HEIGHT 200

#define FONT_SMALL    FreeSans9pt7b
#define FONT_MED      FreeSansBold9pt7b
//#define FONT_BIG      FreeSansBold64pt7b
#define FONT_BIG      LiberationSans_Bold64pt7b

GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(GxEPD2_154_D67(CS, DC, RST, BUSY));
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

void epaperSetup() {
  SPI.begin(SCL, -1, SDA, CS);
  display.init(115200, false);
  display.setPartialWindow(1, 1, 200, 200);

  u8g2Fonts.begin(display);

  SPI.begin(SCL, -1, SDA, CS);
  display.init(115200, false);
  display.setPartialWindow(1, 1, 200, 200);

  u8g2Fonts.begin(display);
}

void epaperPaint(int count, const char *symbol) {
  int16_t x1, y1;
  uint16_t w, h;
  
  display.setRotation(1);    // Landscape
  display.setPartialWindow(0, 0, 200, 200);
  display.setTextColor(GxEPD_BLACK);

  display.firstPage();
  do {  
    display.setFont(&FONT_MED);
    display.setTextSize(1.0);
    display.getTextBounds("Hold to Zero", 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2 +8, 190);
    display.print("Hold to reset");

    u8g2Fonts.setFont(u8g2_font_open_iconic_weather_2x_t);
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.setBackgroundColor(GxEPD_WHITE);

    u8g2Fonts.drawUTF8(28, 192, symbol);

    display.setFont(&FONT_BIG);
    display.getTextBounds(String(count), 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2 - x1, 140);
    display.print(count);

  } while (display.nextPage());

  display.hibernate();
}