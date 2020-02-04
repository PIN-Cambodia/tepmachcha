#include "tepmachcha.h"
#include "fona.h"
#include "sonar.h"

// RTC requirements
#include <avr/sleep.h>
#include <avr/power.h>
#include <Wire.h>
#include "DS1337.h"
DS1337 RTC;
static DateTime interruptTime;
static uint16_t interruptInterval = 900; //Seconds. Change this to suitable value.

void setup() {
  // Setup the pins

  /**
   * We use the LED to communicate without Serial.
   */
  pinMode(LED_PIN, OUTPUT);

  /**
   * Setup the SONAR
   */

  pinMode(SONAR_POWER, OUTPUT);
  digitalWrite(SONAR_POWER, LOW);
  pinMode(SONAR_READ, INPUT);

  /**
   * Setup standard serial for debugging
   */
  Serial.begin(115200);
  Serial.println(F("Initializing...."));

  /**
   * Initialize INT0 for accepting interrupts, as well as Wire and RTC needed for low power sleep
   */
  PORTD |= 0x04; 
  DDRD &=~ 0x04;
  Wire.begin();
  RTC.begin();
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);

  /**
   * Turn on fona and print the IMEI and Sim Card Number. 
   */
  if(!fonaOn()) {
      Serial.print(F("CRITICAL: Could not turn FONA on, rebooting..."));
      // Before we reboot, flash the LED 20 times to indicate we're not happy
      reboot();
  }

  // Let's sync the time, just to make sure that everything is working as expected
  fonaEnableNTPTimeSync();
  // TODO: Sync shield and RTC time

  /**
    * Customised behaviour below, only runs on boot
    */

  // Build the variables that we're submitting
  char imei[20] = {0};
  fonaGetIMEI(imei);
  char iccid[25] = {0};
  fonaGetICCID(iccid);

  char post_data[200];

  sprintf_P (post_data,
    (prog_char *)F("{\"source\": \"" DEVICE_ID "\", \"apiKey\": \"" API_KEY "\", \"payload\":{\"rssi\":%d,\"imei\":%s, \"iccid\":\"%s\",\"bat\":%d, \"firmware_version\":\"" FIRMWARE_VERSION "\"}}"),
    fonaGetRSSI(),
    imei,
    iccid,
    (int)(stalker.readBattery()*1000)
  );
  
  // Try posting 3 times before giving up
  fonaPOSTWithRetry(SETUP_ENDPOINT, post_data, 3);
  fonaOff();

  /**
   * Set the first interrupt to be in X seconds. Note that since we dont 
   * actually sleep after this, the loop will run once before we go into 
   * the interrupt sequence, which means we get a measurement straight 
   * after boot (which we want).
   */
  DateTime start = RTC.now();
  interruptTime = DateTime(start.get() + interruptInterval);
}

void loop() {
  DateTime now = RTC.now(); //get the current date-time    
  
  Serial.println(F("Turning on sonar"));
  digitalWrite(SONAR_POWER, HIGH);

  Serial.println(F("Reading..."));
  int distance = sonarRead();
  int spread = sonarSpread();

  Serial.print(F("Distance to closest surface is "));
  Serial.print(distance);
  Serial.println(F("mm"));
  Serial.print(F("Recorded distances vary by "));
  Serial.print(spread);
  Serial.println(F("mm"));

  Serial.println(F("Turning off sonar"));
  digitalWrite(SONAR_POWER, LOW);

  if(fonaOn()) {

    // Build the variables that we're submitting
    char imei[20] = {0};
    fonaGetIMEI(imei);
    char iccid[25] = {0};
    fonaGetICCID(iccid);
  
    char post_data[200];
  
    sprintf_P (post_data,
      (prog_char *)F("{\"source\": \"" DEVICE_ID "\", \"apiKey\": \"" API_KEY "\", \"payload\":{\"rssi\":%d,\"distance\":%d,\"spread\":%d,\"bat\":%d,\"charging\":%d}}"),
      fonaGetRSSI(),
      distance,
      spread,
      (int)(stalker.readBattery()*1000),
      stalker.readChrgStatus()
    );
    
    // Try posting 3 times before giving up
    fonaPOSTWithRetry(DATAPOINT_ENDPOINT, post_data, 3);

    Serial.println(F("Turning off FONA"));
    fonaOff(); 
  } else {
    Serial.print(F("CRITICAL: Could not turn FONA on, will not be able to submit reading"));
  }
  Serial.println();/*

  /**
   * Go into low power sleep until next time
   */
  RTC.clearINTStatus(); //This function call is a must to bring /INT pin HIGH after an interrupt.
  RTC.enableInterrupts(interruptTime.hour(), interruptTime.minute(), interruptTime.second());    // set the interrupt at (h,m,s)
  attachInterrupt(0, INT0_ISR, LOW);  //Enable INT0 interrupt (as ISR disables interrupt). This strategy is required to handle LEVEL triggered interrupt

  //Power Down routines
  cli();
  sleep_enable();      // Set sleep enable bit
  sleep_bod_disable(); // Disable brown out detection during sleep. Saves more power
  sei();
  Serial.println(F("Sleeping..."));
  delay(10); //This delay is required to allow print to complete
  //Shut down all peripherals like ADC before sleep. Refer Atmega328 manual
  power_all_disable(); //This shuts down ADC, TWI, SPI, Timers and USART
  sleep_cpu();         // Sleep the CPU as per the mode set earlier(power down)
  sleep_disable();     // Wakes up sleep and clears enable bit. Before this ISR would have executed
  power_all_enable();  //This shuts enables ADC, TWI, SPI, Timers and USART
  delay(10); //This delay is required to allow CPU to stabilize
  Serial.println("Awake from sleep");
}

//Interrupt service routine for external interrupt on INT0 pin conntected to /INT
void INT0_ISR()
{
  // Keep this as short as possible. Possibly avoid using function calls  
  Serial.println(" External Interrupt detected ");
  detachInterrupt(0);
  interruptTime = DateTime(interruptTime.get() + interruptInterval);
}

void(* resetFunc) (void) = 0;
void reboot() {
  int i = 0; 
  while(i < 20) {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(50);
    i++;
  }
  resetFunc();
}

extern int __heap_start;
extern void *__brkval;
uint16_t freeRam (void) {
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

// read temperature of the atmega328 itself
int16_t internalTemp(void)
{
  uint16_t wADC = 0;
  int16_t t;

  // The internal temperature has to be used
  // with the internal reference of 1.1V.
  // Channel 8 can not be selected with
  // analogRead() yet.

  // Set the internal reference and mux.
  ADMUX = (_BV(REFS1) | _BV(REFS0) | _BV(MUX3));
  ADCSRA |= _BV(ADEN);  // enable the ADC

  delay(20);            // wait for voltages to become stable.

  for (uint8_t i = 0 ; i < 64 ; i++)
  {
    ADCSRA |= _BV(ADSC);  // Start the ADC

    // wait for conversion to complete
    while (bit_is_set(ADCSRA,ADSC));

    // Reading register "ADCW" takes care of how to read ADCL and ADCH.
    wADC += ADCW;
  }

  // offset ~324.31, scale by 1/1.22 to give C
  return (wADC - (uint16_t)(324.31*64) ) / 78;    // 64/78 ~= 1/1.22
}
