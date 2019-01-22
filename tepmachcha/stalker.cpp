// Stalker V3.1 functions

#include "tepmachcha.h"

// Non-blocking delay function
void wait (uint32_t period)
{
  uint32_t waitend = millis() + period;
  while (millis() <= waitend)
  {
    Serial.flush();
  }
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

extern int __heap_start;
extern void *__brkval;

uint16_t freeRam (void) {
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

void debugFreeRam(void) {
  Serial.print (F("Ram: "));
  Serial.print (freeRam());
  Serial.print (F(" SP: "));
  Serial.print (SP);
  Serial.print (F(" heap: "));
  Serial.println ((uint16_t)&__heap_start);
}
