#include <GxEPD2_BW.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include "FreeSansBold64pt7b.h"
#include <SPI.h>
#include <esp_sleep.h>
#include <Preferences.h>

#define DEBUG         true

// epaper
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

const unsigned long         DEBOUNCE           = 10000;     // micro seconds
const unsigned long         LONG_PRESS_TIME    = 1000000;
const bool                  RELEASED           = HIGH;
const bool                  PRESSED            = LOW;

// State variables
bool                        LongPressTriggered     = false;
volatile bool               buttonState            = RELEASED;
volatile int                releaseCount           = 0;
volatile unsigned long long lastTransitionTime     = 0;
unsigned long long          lastButtonStateDisplay = 0;

unsigned int                missedTransitions      = 0;
unsigned int                ups                    = 0;
unsigned int                downs                  = 0;
bool                        previousButtonState    = RELEASED;

enum Action {IDLE, SHORT_PRESS, LONG_PRESS};

Action checkButton() {

  previousButtonState = buttonState;
  buttonState = digitalRead(BUTTON);
  if (previousButtonState != buttonState) {
    missedTransitions ++;
    lastTransitionTime = esp_timer_get_time();

    if (DEBUG) Serial.printf("        MISSED transition: %d\n", buttonState);
    if (buttonState == RELEASED) {
      releaseCount = releaseCount + 1;
    }
  }

  if (!LongPressTriggered && buttonState == PRESSED  && (esp_timer_get_time() - lastTransitionTime) > LONG_PRESS_TIME) {
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
  
  unsigned long long now = esp_timer_get_time();
  
  if ( (now - lastTransitionTime) < DEBOUNCE) {
    return;
  }

  lastTransitionTime = now;
  buttonState = digitalRead(BUTTON);

  if (buttonState == RELEASED) {
    ups ++;
  }
  else {
    downs ++;
  }

  // release 
  if (buttonState == RELEASED) {
    if (LongPressTriggered) {
      LongPressTriggered = false;
    }
    else {
      releaseCount = releaseCount + 1;
    }
  }

}

void setup() {

  if (DEBUG) Serial.begin(115200);

  pinMode(LED, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);

  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO) {
    if (DEBUG) Serial.printf("Wake on button press\n");

    // simulate the button press ISR when waking from sleep (as ISR doesn't trigger)
    // delay(50);                       // let bouce dissapate
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

  if (DEBUG) {
    Serial.print("paint: ");
    // Serial.printf("count: %d ", count);
    Serial.printf("buttonState: %d ", buttonState);
    Serial.printf("prevousButtonState: %d ", previousButtonState);
    Serial.printf("missedTransitions: %d ", missedTransitions);
    // Serial.printf("lastTransitionTime: %d ", lastTransitionTime / 1000);
    Serial.printf("ups: %d downs: %d ", ups, downs);
    Serial.print("\n");
    // return;
  }

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

unsigned long lastHeartBeat  = 0;
const int     HEARTBEAT      = 1000;

void loop() {

  Action action = checkButton();

  if (action == SHORT_PRESS) {
    if (DEBUG) Serial.println("short press");
    count++;
    paint();
  }

  if (action == LONG_PRESS) {
    Serial.println("long press");
    count = 0;
    paint();
  }

  /*
  if ( esp_timer_get_time() - lastHeartBeat > HEARTBEAT) {
    digitalWrite(LED, !buttonState);
    delay(10);
    digitalWrite(LED, buttonState);
    lastHeartBeat = esp_timer_get_time();
  }
  */

  if (true && (esp_timer_get_time() - lastTransitionTime) > 15000000 && releaseCount == 0 && count != 10) {

    if (DEBUG) Serial.println("sleeping");
    prefs.begin("storage", false);    // Read-Only: false (ie writable)
    prefs.putInt("count", count);
    prefs.end();

    esp_deep_sleep_enable_gpio_wakeup(1ULL << BUTTON, ESP_GPIO_WAKEUP_GPIO_LOW);
    esp_deep_sleep_start();
    
  }
  
}