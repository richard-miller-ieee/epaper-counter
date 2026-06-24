#include <GxEPD2_BW.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include "FreeSansBold64pt7b.h"
#include <U8g2_for_Adafruit_GFX.h>
#include <SPI.h>
#include <esp_sleep.h>
#include <Preferences.h>

#define DEBUG         false

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
#define FONT_BIG      FreeSansBold64pt7b

#define BUTTON        2
#define LED           8
#define STAYAWAKE     5

GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(GxEPD2_154_D67(CS, DC, RST, BUSY));
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;


Preferences prefs;
RTC_DATA_ATTR int count = 0;

const uint32_t              LONG_PRESS_TIME        = 1000000;
const bool                  RELEASED               = HIGH;
const bool                  PRESSED                = LOW;

// State variables
volatile bool               LongPressTriggered     = false;
volatile bool               buttonState            = RELEASED;
         int                buttonPressCount       = 0;

bool                        sleeping               = false;
         uint32_t           lastPaintTime          = 0;
volatile int                debounceCount          = 0;

volatile uint32_t           buttonPressTime        = 0;
volatile uint32_t           buttonPressCount_ISR   = 0;
volatile uint32_t           lastTriggerTime        = 0;
const    uint32_t           DEBOUNCE               = 60000;     // micro seconds

void IRAM_ATTR buttonISR() {
  uint32_t now = (uint32_t)esp_timer_get_time();
  uint32_t delta = now - lastTriggerTime;

  lastTriggerTime = now;

  if ( delta < DEBOUNCE) {return;}

  buttonPressTime = now;
  buttonPressCount_ISR = buttonPressCount_ISR + 1;
}

void setup() {

  if (DEBUG) Serial.begin(115200);

  pinMode(LED, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);

  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO) {
    if (DEBUG) Serial.printf("Wake on button press\n");

    uint32_t now = micros();

    // simulate the button press ISR when waking from sleep (as ISR doesn't trigger)
    buttonState = PRESSED;
    buttonPressTime = now;
    LongPressTriggered = false;
    sleeping = false;

    // wait for debounce to settle down
    delay(50);
  }
  else {
    buttonState = RELEASED;
    buttonPressTime = 0;
  }

  attachInterrupt(digitalPinToInterrupt(BUTTON), buttonISR, FALLING);

  prefs.begin("storage", true);       // read-only: true
  count = prefs.getInt("count", 0);   // 0 if does not exist
  prefs.end();

  SPI.begin(SCL, -1, SDA, CS);
  display.init(115200, false);
  display.setPartialWindow(1, 1, 200, 200);

  u8g2Fonts.begin(display);

  paint("SETUP");
}


void paint(const char *mesg) {

  int16_t x1, y1;
  uint16_t w, h;

  Serial.print("paint: ");
  Serial.printf("count: %d, bounce: %d ", count, debounceCount);
  Serial.printf("mesg: %s ", mesg);
  Serial.printf("\n");
  // return;
  
  display.setRotation(1);  // Landscape
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
    if (sleeping) {
      u8g2Fonts.drawUTF8(28, 192, "\x42"); // sun
    } else {
      u8g2Fonts.drawUTF8(28, 192, "\x45"); // moon
    }

    display.setFont(&FONT_BIG);
    display.getTextBounds(String(count), 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2 - x1, 140);
    display.print(count);

  } while (display.nextPage());

  display.hibernate();

  lastPaintTime = (uint32_t)esp_timer_get_time();
}

void loop() {

  noInterrupts();
    uint32_t buttonPressTimeSample = buttonPressTime;
  interrupts();

  uint32_t now = esp_timer_get_time();
  
  // long press
  if (true && digitalRead(BUTTON) == PRESSED && (now - buttonPressTimeSample) > LONG_PRESS_TIME) {

    buttonPressCount = 0;
    if (count != 0) {
      count = 0;
      paint("LONG");
    }
    
    // wait for the release and ignore it
    while(digitalRead(BUTTON) == PRESSED) {
      // wait
    }

    delay(59);

    noInterrupts();
      buttonPressCount = 0;
      buttonPressCount_ISR = 0;
    interrupts();
  }
  

  // short press
  noInterrupts();
    buttonPressCount += buttonPressCount_ISR;
    buttonPressCount_ISR = 0;
  interrupts();

  if (digitalRead(BUTTON) == RELEASED && buttonPressCount > 0) {

    delay(100);
    if (digitalRead(BUTTON) == RELEASED) {
      buttonPressCount --;
      count ++;
      paint("SHORT");
    }
  }

  /*
  if (false && (now - lastPaintTime) > 5000000 && count != STAYAWAKE) {

    prefs.begin("storage", false);    // Read-Only: false (ie writable)
    prefs.putInt("count", count);
    prefs.end();

    sleeping = true;
    paint("SLEEP");

    // if activity during the previous paint() don't sleep and re-paint();

    // esp_deep_sleep_enable_gpio_wakeup(1ULL << BUTTON, ESP_GPIO_WAKEUP_GPIO_LOW);
    // esp_deep_sleep_start();
  }
  */
  
}