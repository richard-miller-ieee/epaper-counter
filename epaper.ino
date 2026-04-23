#include <GxEPD2_BW.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include "FreeSansBold64pt7b.h"
#include <SPI.h>
#include <esp_sleep.h>
#include <Preferences.h>
#include "button.h"

// epaper
#define BUSY   5   // purple
#define RST    4   // salmon
#define DC     3   // white
#define CS     10  // blue
#define SCL    7   // green
#define SDA    6   // yellow

#define SCREEN_WIDTH  200
#define SCREEN_HEIGHT 200

#define FONT_SMALL FreeSans9pt7b
#define FONT_MED   FreeSansBold9pt7b
#define FONT_BIG   FreeSansBold64pt7b

#define BUTTON 2
#define LED    8

GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(GxEPD2_154_D67(CS, DC, RST, BUSY));

Preferences prefs;
// int count;
RTC_DATA_ATTR int count = 0;

// Constants for button behavior
const unsigned long DEBOUNCE        = 50;     // milliseconds
const unsigned long LONG_PRESS_TIME = 500;

// State variables
bool                   LongPressTriggered = false;
volatile bool          state              = LOW;
volatile int           releaseCount       = 0;
volatile unsigned long lastTransitionTime = 0;

enum ButtonState {IDLE, PRESSED, RELEASED, SHORT_PRESS, LONG_PRESS};

ButtonState checkButton() {

  if (!LongPressTriggered && state == HIGH && (millis() - lastTransitionTime) > LONG_PRESS_TIME) {
      LongPressTriggered = true;
      releaseCount = 0;
      return LONG_PRESS;
  }

  if (releaseCount > 0) {
    releaseCount = releaseCount - 1;
    return SHORT_PRESS;
  }

  return IDLE;
}

void IRAM_ATTR buttonISR() {

  unsigned long now = millis();
  
  if ( (now - lastTransitionTime) < DEBOUNCE) {
    return;
  }
  lastTransitionTime = now;

  // flip
  state = !state;
  digitalWrite(LED, state);

  // press
  /*
  if (state == HIGH) {
  }
  */

  // release 
  if (state == LOW) {
    if (LongPressTriggered) {
      LongPressTriggered = false;
      releaseCount = 0;
    }
    else {
      releaseCount = releaseCount + 1;
    }
  }

}

void setup() {

  Serial.begin(115200);

  int wakeCause = (int)esp_sleep_get_wakeup_cause();
  Serial.printf("Wake cause: %d\n", wakeCause);

  pinMode(LED, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);

  if (wakeCause == 7) {
    Serial.printf("Wake on button\n");

    // simulate the button press ISR when waking from sleep
    delay(100);
    state = true;
    digitalWrite(LED, state);

  }

  prefs.begin("storage", false);  // read/write
  count = prefs.getInt("count", 99);
  prefs.end();

  SPI.begin(SCL, -1, SDA, CS);
  display.init(115200, false);

  display.setPartialWindow(1, 1, 200, 200);

  attachInterrupt(digitalPinToInterrupt(BUTTON), buttonISR, CHANGE);

  paint();
}


void paint() {

  int16_t x1, y1;
  uint16_t w, h;

  display.setRotation(1);  // Landscape
  display.setPartialWindow(0, 0, 200, 200);
  display.setTextColor(GxEPD_BLACK);

  display.firstPage();
  do {  
    display.setFont(&FONT_MED);
    display.setTextSize(1.0);

    display.getTextBounds("ROW COUNTER", 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, 35);
    display.print("ROW COUNTER");

    display.getTextBounds("Click to Increase", 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, 170);
    display.print("Click to increase");

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

unsigned long lastDisplay    = 0;
unsigned long lastHeartBeat  = 0;

#define HEARTBEAT 1000

void loop() {
  ButtonState action = IDLE;  

  action = checkButton();

  if (action == SHORT_PRESS) {
    // Serial.println("short press");
    // Serial.println(count);
    count++;
    paint();
  }

  if (action == LONG_PRESS) {
    // Serial.println("long press");
    count = 0;
    paint();
  }

  if ( millis() - lastHeartBeat > HEARTBEAT) {
    digitalWrite(LED, !state);
    delay(10);
    digitalWrite(LED, state);
    lastHeartBeat = millis();
  }

  if (true && (millis() - lastTransitionTime) > 30000 && count != 10) {
    Serial.println("sleeping");
    prefs.begin("storage", false);
    prefs.putInt("count", count);
    prefs.end();
    esp_deep_sleep_enable_gpio_wakeup(1ULL << BUTTON, ESP_GPIO_WAKEUP_GPIO_LOW);
    esp_deep_sleep_start();
  }
  
}


