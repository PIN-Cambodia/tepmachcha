#include "tepmachcha.h"
#include "fona.h"
char replybuffer[255];

SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);
SoftwareSerial *fonaSerial = &fonaSS;
Adafruit_FONA fona = Adafruit_FONA(FONA_RST);

/**
 * Turn on the FONA. This does several things;
 *
 * 1) Set's up fonaSerial
 * 2) Waits for FONA to be registered on the home network
 * 3) Set's the APN based on the FONA_APN and wait for up to 10 seconds 
 *    (this limit is set up by trail and error, and might need to be changed)
 * 4) Enables GPRS
 * 
 * returns false if there are errors with enabling GPRS, otherwise true
 */
bool fonaOn() {
  if (!fonaSerial->available()) {
    fonaSerial->begin(4800);
    if (! fona.begin(*fonaSerial)) {
      Serial.println(F("[FONA ERROR] Couldn't find FONA"));
      while (1);
    }
  }
  Serial.println(F("[FONA] FONA found and serial open"));
  
  if(fona.getNetworkStatus() != 1)
    fonaWaitForNetwork();

  fonaSetupGPRS();
  int i = 0;
  bool gprs = false;
  do {
    fona.enableGPRS(false);
    delay(2000);
    gprs = fona.enableGPRS(true);
    i++;
  } while(!gprs && i < 10);

  if(gprs)
    Serial.println(F("[FONA] Connected to GPRS"));
  else
    Serial.println(F("[FONA ERROR] Could not connect to GRPS"));

  // It takes a bit of time for everything to fall into place, so we wait 5 seconds
  delay(5000);
  return gprs;
}

/**
 * 
 */
bool fonaOff() {
  if (!fona.enableGPRS(false)) {
    Serial.println(F("[FONA ERROR] Could not disconnect from GRPS"));
    return false;
  }

  Serial.println(F("[FONA] Disconnected from GRPS"));
  return true;
}

void fonaWaitForNetwork() {
  uint8_t n;
  Serial.println(F("[FONA] Connecting to network"));
  do {
    n = fona.getNetworkStatus();
    // give it 1 more second
    delay(1000);
  } while(n != 1);
  Serial.println(F("[FONA] Successfully connected to home network"));
}

void fonaEnableNTPTimeSync() {
  if (!fona.enableNTPTimeSync(true, F("pool.ntp.org")))
    Serial.println(F("[FONA ERROR] Failed to enable NTP time sync"));
  else {
    char buffer[23];
    fonaGetTime(buffer);
    Serial.print(F("[FONA] NTP time sync complete: "));
    Serial.println(buffer);
  }
}

void fonaGetTime(char* buffer) {
  fona.getTime(buffer, 23);  // make sure replybuffer is at least 23 bytes!
}

void fonaGetIMEI(char* imei) {
  fona.getIMEI(imei);
}

void fonaPrintIMEI() {
  // Print module IMEI number.
  char imei[16] = {0};
  fonaGetIMEI(imei);
  Serial.print("[FONA] Module IMEI: "); Serial.println(imei);
}

void fonaGetICCID(char* iccid) {
  fona.getSIMCCID(iccid);
}

void fonaPrintICCID() {
  char iccid[20] = {0};
  fonaGetICCID(iccid);
  Serial.print(F("[FONA] SIM CCID: ")); Serial.println(iccid);
}

void fonaSetupGPRS() {
  fona.setGPRSNetworkSettings(F(FONA_APN));
  Serial.print("[FONA] APN set: "); Serial.println(FONA_APN);
}

uint8_t fonaGetRSSI() {
  return fona.getRSSI();
}

bool fonaPOST(char* endpoint, char* post_data) {
  uint16_t statuscode;
  int16_t length;

  Serial.print(F("POST to "));
  Serial.println(endpoint);
  Serial.println(post_data);

  // Post data to website

  if (!fona.HTTP_POST_start(endpoint, F("application/json"), (uint8_t *) post_data, strlen(post_data), &statuscode, (uint16_t *)&length)) {
    Serial.print("Failed to POST! Maybe FONA it not connected?");
    return false;
  }

  // If we got a statuscode that is not between 200 and 300, we've probably hit an error.
  if(statuscode < 200 || statuscode > 299) {
    Serial.print("Server returned unexpected response!");
  }
  
  while (length > 0) {
    while (fona.available()) {
      char c = fona.read();

      #if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
      loop_until_bit_is_set(UCSR0A, UDRE0); /* Wait until data register empty. */
      UDR0 = c;
      #else
      Serial.write(c);
      #endif

      length--;
      if (! length) break;
    }
  }
  Serial.println(F("\n****"));
  fona.HTTP_POST_end();
  if(statuscode < 200 || statuscode > 299) {
    return false;
  }
  return true;
}

bool fonaPOSTWithRetry(char* endpoint, char* post_data, int retries) {
  while(retries > 0) {
    if(!fonaPOST(endpoint, post_data))
      delay(5000);
    else
      return true;
    retries--;
  }
  return false;
}

