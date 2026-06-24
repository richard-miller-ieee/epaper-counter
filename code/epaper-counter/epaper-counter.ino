/*
** epaper-counter
**
** target microcontroller: SEED XIAO_ESP32C3
**
** RJM - 24 July 2026
*/
#include <Arduino.h>
#include <Preferences.h>
#include "epaper.h"

Preferences       prefs;
      int         buttonPressCount = 0;

const int         BUTTON           = 2; 
const int         LONG_PRESS       = 1000 * 1000;
const int         IDLE_TIME        = 10 * 1000;
const bool        PRESSED          = LOW;

SemaphoreHandle_t xButtonSemaphore = NULL;
TimerHandle_t     xDebounceTimer   = NULL;
QueueHandle_t     xButtonQueue     = NULL;

void ARDUINO_ISR_ATTR onButtonPress() {
  detachInterrupt(digitalPinToInterrupt(BUTTON));

  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  xTimerStartFromISR(xDebounceTimer, &xHigherPriorityTaskWoken);

  if (xHigherPriorityTaskWoken == pdTRUE) {
    portYIELD_FROM_ISR();
  }
}

void vDebounceTimerCallback(TimerHandle_t xTimer) {
  (void) xTimer;

  if (digitalRead(BUTTON) == PRESSED) {
    xSemaphoreGive(xButtonSemaphore);
  }
  attachInterrupt(digitalPinToInterrupt(BUTTON), onButtonPress, FALLING);
}

void vButtonTask(void *pvParameters) {
  (void) pvParameters;

  for (;;) {
    if (xSemaphoreTake(xButtonSemaphore, portMAX_DELAY) == pdTRUE) {
      
      uint32_t pressStartTime = esp_timer_get_time();
      bool longPressTriggered = false;

      while (digitalRead(BUTTON) == PRESSED) {
        
        if ((esp_timer_get_time() - pressStartTime) >= LONG_PRESS) {
          buttonPressCount = 0;
          xQueueReset(xButtonQueue);
          xQueueSend(xButtonQueue, &buttonPressCount, 0);          
          longPressTriggered = true;
          break;
        }

        vTaskDelay(20 / portTICK_PERIOD_MS);
      }

      if (!longPressTriggered) {
        buttonPressCount++;
        xQueueSend(xButtonQueue, &buttonPressCount, 0);
      } 
      else {
        // If a long press WAS triggered, we must wait here until they actually 
        // lift their finger so we don't accidentally trigger a new press edge.
        while (digitalRead(BUTTON) == LOW) {
          vTaskDelay(50 / portTICK_PERIOD_MS);
        }
      }
    }
  }
}

void vEPaperTask(void *pvParameters) {
  (void) pvParameters;
  int epaperCount = 0;

  const TickType_t xMaxBlockTime = pdMS_TO_TICKS(IDLE_TIME);

  for (;;) {
    if (xQueueReceive(xButtonQueue, &epaperCount, IDLE_TIME) == pdTRUE) {
      Serial.printf("[E-Paper Task] Count changed to: %d\n", epaperCount);
      epaperPaint(epaperCount, SUN);
    } 
    else {

      // the buttonQueue has been empty for IDLE_TIME -> Sleep
      if (epaperCount != 5) {  // magic no sleep value

        //Serial.println("[E-Paper Task] inactivity");
        epaperPaint(epaperCount, MOON);
      
        // check for activity during the epaperPaint() above

        // save state
        prefs.begin("counter", false);    // Read-Only: false (ie writable)
        prefs.putInt("PressCount", buttonPressCount);
        prefs.end();
        // Serial.printf("[E-Paper Task] Saved new count %d to Flash.\n", buttonPressCount);

        vTaskDelay(100 / portTICK_PERIOD_MS);

        // check for activity during the epaperPaint() and state saving above
        if (uxQueueMessagesWaiting(xButtonQueue) > 0) {
          Serial.println("[E-paper Task] Abort sleep: New buttonQueue message available");
          continue; 
        }

        // Check if the user is still CURRENTLY holding the button down,
        // or the debounce timer/interrupt just gave the semaphore.
        if (digitalRead(BUTTON) == PRESSED || xSemaphoreTake(xButtonSemaphore, 0) == pdTRUE) {
          Serial.println("[E-Paper Task] Abort sleep: Button is physically pressed or active!");
          
          // If we took the semaphore, give it back so vButtonTask can process it
          if (digitalRead(BUTTON) == PRESSED) {
            xSemaphoreGive(xButtonSemaphore); 
          }
          
          continue; // Abort sleep and let the tasks handle the button
        }
        
        esp_deep_sleep_enable_gpio_wakeup(1ULL << BUTTON, ESP_GPIO_WAKEUP_GPIO_LOW);
        Serial.flush(); // Ensure all serial data clears the TX buffer before powering down
        esp_deep_sleep_start();
      }
    }
  } 
} 


void setup() {
  Serial.begin(115200);
  // delay(1000);

  epaperSetup();

  // read the historical state
  prefs.begin("counter", true); // read-only: true
  buttonPressCount = prefs.getInt("PressCount", 0); 
  prefs.end();

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  xButtonSemaphore = xSemaphoreCreateBinary();
  xDebounceTimer = xTimerCreate(
                    "DebounceTimer",
                    pdMS_TO_TICKS(50), 
                    pdFALSE, (void *)0, 
                    vDebounceTimerCallback
  );
  xButtonQueue = xQueueCreate(20, sizeof(int));

  if (xButtonSemaphore != NULL && xDebounceTimer != NULL && xButtonQueue != NULL) {
    pinMode(BUTTON, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON), onButtonPress, FALLING);

    xTaskCreate(vButtonTask, "ButtonTask", 2048, NULL, 2, NULL); 
    xTaskCreate(vEPaperTask, "EPaperTask", 4096, NULL, 1, NULL); 
    
    Serial.println("System Initialized (FreeRTOS)");
  } 
  else {
    Serial.println("Initialization Error!");
  }

  if (wakeup_reason == ESP_SLEEP_WAKEUP_GPIO) {
    Serial.printf("[Setup] Woken from Sleep! Restored baseline count: %d\n", buttonPressCount);
    xSemaphoreGive(xButtonSemaphore);
  } 
  else {
    Serial.printf("[Setup] Cold Boot! Displaying stored baseline: %d\n", buttonPressCount);
    epaperPaint(buttonPressCount, SUN);
  }

}


void loop() {
  vTaskDelay(portMAX_DELAY);
}