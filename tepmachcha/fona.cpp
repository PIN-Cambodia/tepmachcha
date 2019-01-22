#include "tepmachcha.h"

const char OK_STRING[] PROGMEM = "OK";

SoftwareSerial fonaSerial = SoftwareSerial (FONA_TX, FONA_RX);
Adafruit_FONA fona = Adafruit_FONA (FONA_RST);

boolean fonaSerialOn(void)
{
    Serial.print (F("Turning on FONA Serial... "));

    fonaSerial.begin (4800);                      //  Open a serial interface to FONA

    if (fona.begin (fonaSerial) == false) {         //  Start the FONA on serial interface
      Serial.println (F("not found"));
      return false;
    }

    Serial.println (F("Done"));
    return true;
}

boolean fonaGSMOn(void) {
  uint32_t gsmTimeout = millis() + 30000;

  Serial.print (F("Turning on FONA GSM... "));
  while (millis() < gsmTimeout) {
    uint8_t status = fona.getNetworkStatus();
    wait (500);
    if (status == 1) { // replace with (status == 1 || status == 5) to allow roaming
      Serial.println(F("Done"));
      fona.sendCheckReply (F("AT+COPS?"), OK);  // Network operator
      fonaFlush();
      return true;
    }
  }
  Serial.println (F("timed out. Check SIM, antenna, signal."));
  return false;
}

boolean fonaGPRSOn(void) {
  Serial.println (F("Turning on FONA GPRS..."));
  fona.setGPRSNetworkSettings (F(APN));  //  Set APN to local carrier
  wait (5000);    //  Give the network a moment

  // RSSI is a measure of signal strength -- higher is better; less than 10 is worrying
  uint8_t rssi = fona.getRSSI();
  Serial.print (F("RSSI: ")); Serial.println (rssi);

  if (rssi > 5) {
    for (uint8_t attempt = 1; attempt < 7; attempt++) {
      Serial.print (F("GPRS -> on, attempt ")); Serial.println(attempt);
      fona.enableGPRS (true);

      if (fona.GPRSstate() == 1) {
        Serial.println (F("GPRS is ON"));
        return true;
      }
      Serial.println(F("GPRS failed"));
      wait (2500);
    }
  }
  else {
    Serial.println (F("Inadequate signal strength"));
  }
  Serial.println(F("Giving up, GPRS Failed"));
  return false;
}

void fonaGPRSOff(void) {
  Serial.print (F("GPRS -> off: "));
    if (fona.GPRSstate() == 1) {
      if (!fona.enableGPRS(false)) {
        Serial.println (F("failed"));
        return;
      }
  }
  Serial.println (F("done"));
}

boolean fonaOn() {
    if (fonaSerialOn()) {
      if ( fonaGSMOn() ) {
        return fonaGPRSOn();
      }
  }
  return false;
}

void fonaOff() {
    if (!fonaSerial.available()) {
      return true;
    }
    return fonaGPRSOff();
}

void fonaFlush (void) {
  // Read all available serial input from FONA to flush any pending data.
  while(fona.available()) {
    char c = fona.read();
    loop_until_bit_is_set (UCSR0A, UDRE0); 
    UDR0 = c;
  }
}

char fonaRead(void) {
  // read a char from fona, waiting up to <timeout> ms for something at arrive
  uint32_t timeout = millis() + 1000;

  while(!fona.available()) {
    if (millis() > timeout)
      break;
    else
      delay(1);
  }
  return fona.read();
}

void smsDeleteAll(void) {
  fona.sendCheckReply (F("AT+CMGF=1"), OK);            //  Enter text mode
  fona.sendCheckReply (F("AT+CMGDA=\"DEL ALL\""), OK); //  Delete all SMS messages
}
