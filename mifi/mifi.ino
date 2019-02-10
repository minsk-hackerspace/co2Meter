

extern "C" {
#include "user_interface.h"
}
#include <WiFiManager.h>
#include <jled.h>

/*
 * 
 * SATUS LED
 * 
 */
 
os_timer_t myTimer;
JLed led = JLed(LED_BUILTIN).LowActive();

void timerCallback(void *pArg) {
  led.Update();
} // End of timerCallback

void timerInit(void) {
      os_timer_setfn(&myTimer, timerCallback, NULL);
      os_timer_arm(&myTimer, 20, true);
} // End of timerInit



/*
 * 
 * SETUP 
 * 
 */

void setup() {
  Serial.begin(115200);
  Serial.println("setup");
  pinMode(LED_BUILTIN, OUTPUT);
  analogWriteRange(255);
  timerInit();
  
  led.Breathe(2000).Forever();
    
}


/*
 * 
 * MAIN LOOP 
 * 
 */

void loop() {
  // put your main code here, to run repeatedly:

}
