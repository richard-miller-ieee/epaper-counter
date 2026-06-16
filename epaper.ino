#include <GxEPD2_BW.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include "FreeSansBold64pt7b.h"
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

GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(GxEPD2_154_D67(CS, DC, RST, BUSY));

Preferences prefs;
RTC_DATA_ATTR int count = 0;

const unsigned long         DEBOUNCE           = 1000;     // micro seconds
const unsigned long         LONG_PRESS_TIME    = 1000000;
const bool                  RELEASED           = HIGH;
const bool                  PRESSED            = LOW;

// State variables
bool                        LongPressTriggered     = false;
volatile bool               buttonState            = RELEASED;
volatile int                releaseCount           = 0;

bool                        previousButtonState    = RELEASED;
unsigned int                paintCount             = 0;

volatile unsigned long long lastTriggerTime        = 0;
volatile unsigned long long buttonPressTime        = 0;

volatile int                transitionCount        = 0;
volatile int                debounceCount          = 0;
volatile unsigned long long lastTransitionTime     = 0;

void IRAM_ATTR buttonISR() {
  unsigned long long now = esp_timer_get_time();
  unsigned long long triggerDelta;

  triggerDelta = now - lastTriggerTime;
  lastTriggerTime = now;

  if ( triggerDelta < DEBOUNCE) {
    // debounceCount = debounceCount + 1;
    return;
  }

  // transitionCount = transitionCount + 1;

  buttonState = digitalRead(BUTTON);

  // press
  if (buttonState == PRESSED) {
    // press
    buttonPressTime = now;
    return;
  }
  
  // release
  if (LongPressTriggered) {
      LongPressTriggered = false;
  }
  else {
    releaseCount = releaseCount + 1;
  }
}

void setup() {

  if (DEBUG) Serial.begin(115200);

  pinMode(LED, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);

  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO) {
    if (DEBUG) Serial.printf("Wake on button press\n");

    // simulate the button press ISR when waking from sleep (as ISR doesn't trigger)
    buttonState = PRESSED; 
    lastTransitionTime = 0;
    LongPressTriggered = false;
  }

  attachInterrupt(digitalPinToInterrupt(BUTTON), buttonISR, CHANGE);

  prefs.begin("storage", true);       // read-only: true
  count = prefs.getInt("count", 0);   // 0 if does not exist
  prefs.end();

  SPI.begin(SCL, -1, SDA, CS);
  display.init(115200, false);
  display.setPartialWindow(1, 1, 200, 200);

  paint();
}


void paint() {

  int16_t x1, y1;
  uint16_t w, h;

  paintCount++;

  /**
    Serial.print("paint: ");
    // Serial.printf("count: %d ", count);
    Serial.printf("buttonState: %d ", buttonState);
    Serial.printf("prevousButtonState: %d ", previousButtonState);
    Serial.printf("missedTransitions: %d ", missedTransitions);
    Serial.print("\n");
    // return;
  */

  display.setRotation(1);  // Landscape
  display.setPartialWindow(0, 0, 200, 200);
  display.setTextColor(GxEPD_BLACK);

  display.firstPage();
  do {  
    display.setFont(&FONT_MED);
    display.setTextSize(1.0);

    // display.getTextBounds("ROW COUNTER", 0, 0, &x1, &y1, &w, &h);
    // display.setCursor((SCREEN_WIDTH - w) / 2, 35);
    // display.print("ROW COUNTER");

    /*
    display.setCursor(1, 170);
    display.print(paintCount); display.print(" "); 
    display.print(transitionCount); display.print(" ");
    display.print(debounceCount); display.print(" ");
    display.print(releaseCount); display.print(" ");
    */

    /*
    display.getTextBounds("Click to Increase", 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, 170);
    display.print("Click to increase");
    */

    display.getTextBounds("Hold to Zero", 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, 190);
    display.print("Hold to reset");

    display.setFont(&FONT_BIG);
    display.getTextBounds(String(count), 0, 0, &x1, &y1, &w, &h);

    display.setCursor((SCREEN_WIDTH - w) / 2 - x1, 140);
    display.print(count);

  } while (display.nextPage());

  display.hibernate();
}

void loop() {

  // long press
  if (buttonState == PRESSED && (esp_timer_get_time() - buttonPressTime) > LONG_PRESS_TIME) {
    LongPressTriggered = true;
    releaseCount = 0;
    if (count != 0) {
      count = 0;
      paint();
    }
  }

  // short press
  if (releaseCount > 0) {
    releaseCount = releaseCount - 1;
    count ++;
    paint();
  }

  if (true && (esp_timer_get_time() - lastTriggerTime) > 15000000 && count != 10) {

    if (DEBUG) Serial.println("sleeping");
    prefs.begin("storage", false);    // Read-Only: false (ie writable)
    prefs.putInt("count", count);
    prefs.end();

    esp_deep_sleep_enable_gpio_wakeup(1ULL << BUTTON, ESP_GPIO_WAKEUP_GPIO_LOW);
    esp_deep_sleep_start();
  }
  
}