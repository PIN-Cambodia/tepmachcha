#include "tepmachcha.h"

const char DEVICE_STR[] PROGMEM = DEVICE;

Sleep sleep;              // Create the sleep object

static void rtcIRQ (void)
{
		RTC.clearINTStatus(); //  Wake from sleep and clear the RTC interrupt
}

void setup (void)
{
    // set RTC interrupt handler
		attachInterrupt (RTCINTA, rtcIRQ, FALLING);
		//interrupts();

    // Set output pins (default is input)
		pinMode (WATCHDOG, INPUT_PULLUP);
		pinMode (BEEPIN, OUTPUT);
		pinMode (RANGE, OUTPUT);
		pinMode (FONA_KEY, OUTPUT);
		pinMode (FONA_RX, OUTPUT);
		pinMode (SD_POWER, OUTPUT);
#ifdef STALKERv31
		pinMode (BUS_PWR, OUTPUT);
#endif

    digitalWrite (RANGE, LOW);           // sonar off
		digitalWrite (SD_POWER, HIGH);       // SD card off
    digitalWrite (BEEPIN, LOW);          // XBee on
		digitalWrite (FONA_KEY, HIGH);       // Initial state for key pin
#ifdef STALKERv31
    digitalWrite (BUS_PWR, HIGH);        // Peripheral bus on
#endif

		Serial.begin (57600); // Begin debug serial
    fonaSerial.begin (4800); //  Open a serial interface to FONA
		Wire.begin();         // Begin I2C interface
		RTC.begin();          // Begin RTC

		Serial.println ((__FlashStringHelper*)DEVICE_STR);

		Serial.print (F("Battery: "));
		Serial.print (batteryRead());
		Serial.println (F("mV"));
    DEBUG_RAM

		// If the voltage at startup is less than 3.5V, we assume the battery died in the field
		// and the unit is attempting to restart after the panel charged the battery enough to
		// do so. However, running the unit with a low charge is likely to just discharge the
		// battery again, and we will never get enough charge to resume operation. So while the
		// measured voltage is less than 3.5V, we will put the unit to sleep and wake once per 
		// hour to check the charge status.
		//
    wait(1000);
		while (batteryRead() < 3500)
		{
				Serial.println (F("Low power sleep"));
				Serial.flush();
				XBeeOff();
				digitalWrite (RANGE, LOW);        //  Make sure sonar is off
#ifdef STALKERv31
        //digitalWrite (BUS_PWR, LOW);   //  Peripheral bus off
#endif
				RTC.enableInterrupts (EveryHour); //  We'll wake up once an hour
				RTC.clearINTStatus();             //  Clear any outstanding interrupts
				sleep.pwrDownMode();                    //  Set sleep mode to Power Down
				sleep.sleepInterrupt (RTCINTA, FALLING); //  Sleep; wake on falling voltage on RTC pin
		}

		// We will use the FONA to get the current time to set the Stalker's RTC
		if (fonaOn())
    {
      // set ext. audio, to prevent crash on incoming calls
      // https://learn.adafruit.com/adafruit-feather-32u4-fona?view=all#faq-1
      fona.sendCheckReply(F("AT+CHFA=1"), OK);

#ifndef DEBUG
      // Delete any accumulated SMS messages to avoid interference from old commands
      smsDeleteAll();
#endif
      clockSet();

      dweetPostStatus(sonarRead(), solarVoltage(), batteryRead());
    }
    fonaOff();

		now = RTC.now();    //  Get the current time from the RTC
		
    // turn XBee on for an hour
    XBeeOn();
    char buffer[32]; // only 20 required currently
    XBeeOnMessage(buffer);
    Serial.println(buffer);

		RTC.enableInterrupts (EveryMinute);  //  RTC will interrupt every minute
		RTC.clearINTStatus();                //  Clear any outstanding interrupts
		sleep.sleepInterrupt (RTCINTA, FALLING); //  Sleep; wake on falling voltage on RTC pin
}


// This runs every minute, triggered by RTC interrupt
void loop (void)
{
    boolean resetClock = false;

#ifdef STALKERv31
    //digitalWrite (BUS_PWR, HIGH);           //  Peripheral bus on
    //wait(500);
#endif

		now = RTC.now();      //  Get the current time from the RTC

		Serial.print (now.hour());
		Serial.print (F(":"));
		Serial.println (now.minute());

    // The RTC drifts more than the datasheet says, so we
    // reset the time every day at midnight.
    if (now.hour() == 0 && now.minute() == 0)
    {
      resetClock = true;
    }

    // Check if it is time to send a scheduled reading
		if (now.minute() % INTERVAL == 0)
		{
      upload (sonarRead(), resetClock);
    }

    XBee();

		Serial.println(F("sleeping"));
		Serial.flush();                         //  Flush any output before sleep

#ifdef STALKERv31
    //digitalWrite (BUS_PWR, LOW);           //  Peripheral bus off
#endif

		sleep.pwrDownMode();                    //  Set sleep mode to "Power Down"
		RTC.clearINTStatus();                   //  Clear any outstanding RTC interrupts
		sleep.sleepInterrupt (RTCINTA, FALLING); //  Sleep; wake on falling voltage on RTC pin
}


void upload(int16_t distance, boolean resetClock)
{
  uint8_t status = 0;
  uint16_t voltage;
  uint16_t solarV;
  boolean charging;
  int16_t streamHeight;

  // At this point the measured distance is the result of multiple sets of readings,
  // and within each set, up to 75% of individual readings may be rejected as invalid.
  // If the result is STILL an invalid distance, we report the last-
  // known-good reading to EWS, in order to avoid triggering spurious alerts, etc,
  // but post the failed reading to dweet, for diagnostics.
  if ( sonarValidReading(distance) ) {
    Serial.print("setting last known good: ");
    sonarLastGoodReading = distance;
  } else {
    Serial.print("invalid, using last known good: ");
  }
  Serial.println(sonarLastGoodReading);
  streamHeight = sonarStreamHeight(sonarLastGoodReading);

  voltage = batteryRead();
  solarV = solarVoltage();
  charging = solarCharging(solarV);

  Serial.print (F("Battery: "));
  Serial.print (voltage);
  Serial.println (F("mV"));
  Serial.print (F("Solar: "));
  Serial.println (solarV);

  if (fonaOn() || (fonaOff(), delay(5000), fonaOn())) // try twice
  {

    uint8_t attempts = 2; do
    {
      status = ews1294Post(streamHeight, charging, voltage);
    } while (!status && --attempts);

    status &= dweetPostStatus(distance, solarV, voltage);

    // if the upload failed the fona can be left in an undefined state,
    // so we reboot it here to ensure SMS works
    if (!status)
    {
      fonaOff(); wait(2000); fonaOn();
    }

    // process SMS messages
    smsCheck();

    if (resetClock)
    {
      clockSet();
    }
  }
  fonaOff();
}


// Don't allow ewsPost() to be inlined, as the compiler will also attempt to optimize stack
// allocation, and ends up preallocating at the top of the stack. ie it moves the beginning
// of the stack (as seen in setup()) down ~200 bytes, leaving the rest of the app short of ram
boolean __attribute__ ((noinline)) ews1294Post (int16_t streamHeight, boolean solar, uint16_t voltage)
{
    uint16_t status_code = 0;
    uint16_t response_length = 0;
    char post_data[200];

    DEBUG_RAM

    // Construct the body of the POST request:
    sprintf_P (post_data,
      (prog_char *)F("api_token=" EWSTOKEN_ID "&data={\"sensorId\":\"" EWSDEVICE_ID "\",\"streamHeight\":\"%d\",\"charging\":\"%d\",\"voltage\":\"%d\",\"timestamp\":\"%d-%d-%dT%d:%d:%d.000Z\"}\r\n"),
        streamHeight,
        solar,
        voltage,
        now.year(), now.month(), now.date(), now.hour(), now.minute(), now.second()
    );

    Serial.println (F("data:"));
    Serial.println (post_data);

    // ews1294.info does not currently support SSL; if it is added you will need to uncomment the following
    //fona.sendCheckReply (F("AT+HTTPSSL=1"), F("OK"));   //  Turn on SSL
    //fona.sendCheckReply (F("AT+HTTPPARA=\"REDIR\",\"1\""), F("OK"));  //  Turn on redirects (for SSL)

    // Send the POST request we have constructed
    char url[30]; strcpy_P(url, (prog_char*)F("ews1294.info/api/v1/sensorapi"));
    if (fona.HTTP_POST_start (url,
                              F("application/x-www-form-urlencoded"),
                              (uint8_t *)post_data,
                              strlen(post_data),
                              &status_code,
                              &response_length))
    {
      // flush response
      while (response_length > 0)
      {
        fonaFlush();
        response_length--;
      }
    }

    fona.HTTP_POST_end();

    if (status_code == 200)
    {
        Serial.println (F("POST succeeded."));
        return true;
    }
    else
    {
        Serial.print (F("POST failed. Status-code: "));
        Serial.println (status_code);
        return false;
    }
}


boolean dmisPost (int16_t streamHeight, boolean solar, uint16_t voltage)
{
    uint16_t statusCode;
    uint16_t dataLen;
    char postData[200];
    DEBUG_RAM

    // HTTP POST headers
    fona.sendCheckReply (F("AT+HTTPINIT"), OK);

    // TODO test SSL
    //fona.sendCheckReply (F("AT+HTTPSSL=1"), OK);   // SSL required
    //fona.sendCheckReply (F("AT+HTTPPARA=\"REDIR\",\"1\""), F("OK"));  //  Turn on redirects (for SSL)

    // TODO don't need http:// in url, in fact it breaks when using https://
    //fona.sendCheckReply (F("AT+HTTPPARA=\"URL\",\"http://dmis-staging.eu-west-1.elasticbeanstalk.com/api/v1/data/river-gauge\""), OK);

    fona.sendCheckReply (F("AT+HTTPPARA=\"URL\",\"dmis-staging.eu-west-1.elasticbeanstalk.com/api/v1/data/river-gauge\""), OK);
    fona.sendCheckReply (F("AT+HTTPPARA=\"REDIR\",\"1\""), OK);
    fona.sendCheckReply (F("AT+HTTPPARA=\"UA\",\"Tepmachcha/" VERSION "\""), OK);
    fona.sendCheckReply (F("AT+HTTPPARA=\"CONTENT\",\"application/json\""), OK);

    // Note the data_source should match the last element of the url,
    // which must be a valid data_source
    // To add multiple user headers:
    //   http://forum.sodaq.com/t/how-to-make-https-get-and-post/31/18
    fona.sendCheckReply (F("AT+HTTPPARA=\"USERDATA\",\"data_source: river-gauge\\r\\nAuthorization: Bearer " DMISAPIBEARER "\""), OK);

    // json data
    sprintf_P(postData,
      (prog_char*)F("{\"sensorId\":\"" DMISSENSOR_ID "\",\"streamHeight\":%d,\"charging\":%d,\"voltage\":%d,\"timestamp\":\"%d-%02d-%02dT%02d:%02d:%02d.000ICT\"}"),
        streamHeight,
        solar,
        voltage,
        now.year(), now.month(), now.date(), now.hour(), now.minute(), now.second());
    int s = strlen(postData);

    // tell fona to receive data, and how much
    Serial.print (F("data size:")); Serial.println (s);
    fona.print (F("AT+HTTPDATA=")); fona.print (s);
    fona.println (F(",2000")); // timeout
    fona.expectReply (OK);

    // send data
    Serial.print(postData);
    fona.print(postData);
    delay(100);

    // do the POST request
    fona.HTTP_action (1, &statusCode, &dataLen, 10000);

    // report status, response data
    Serial.print (F("http code: ")); Serial.println (statusCode);
    Serial.print (F("reply len: ")); Serial.println (dataLen);
    if (dataLen > 0)
    {
      fona.sendCheckReply (F("AT+HTTPREAD"), OK);
      delay(1000);
    }

    fonaFlush();
    fona.HTTP_term();

    return (statusCode == 201);
}


boolean dweetPostStatus(int16_t distance, uint16_t solar, uint16_t voltage)
{
    char json[136];
    uint16_t streamHeight = sonarStreamHeight(distance);

    // 102 + vars
    sprintf_P(json,
      (prog_char*)F("{\"dist\":%d,\"streamHeight\":%d,\"solarV\":%d,\"voltage\":%d,\"uptime\":%ld,\"version\":\"" VERSION "\",\"internalTemp\":%d,\"freeRam\":%d}"),
        distance,
        streamHeight,
        solar,
        voltage,
        millis()/1000,
        internalTemp(),
        freeRam());
    return dweetPost((prog_char*)F(DWEETDEVICE_ID), json);
}

boolean dweetPostFota(boolean status)
{
    char json[66];

    // 43 + vars
    sprintf_P(json,
      (prog_char*)F("{\"filename\":\"%s\",\"size\":%d,\"status\":%d,\"error\":%d}"),
        file_name,
        file_size,
        status,
        error);
    return dweetPost((prog_char*)F(DWEETDEVICE_ID "-FOTA"), json);
}


boolean dweetPost (prog_char *endpoint, char *postData)
{
    uint16_t statusCode;
    uint16_t dataLen;
    char url[72];
    DEBUG_RAM

    // HTTP POST headers
    fona.sendCheckReply (F("AT+HTTPINIT"), OK);
    fona.sendCheckReply (F("AT+HTTPSSL=1"), OK);   // SSL required
    fona.sendCheckReply (F("AT+HTTPPARA=\"REDIR\",\"1\""), OK);
    fona.sendCheckReply (F("AT+HTTPPARA=\"UA\",\"Tepmachcha/" VERSION "\""), OK);
    fona.sendCheckReply (F("AT+HTTPPARA=\"CONTENT\",\"application/json\""), OK);

    // 48 + endpoint
    sprintf_P(url, (prog_char*)F("AT+HTTPPARA=\"URL\",\"dweet.io/dweet/quietly/for/%S\""), endpoint);
    fona.sendCheckReply (url, OK);

    // json data
    int s = strlen(postData);

    // tell fona to receive data, and how much
    Serial.print (F("data size:")); Serial.println (s);
    fona.print (F("AT+HTTPDATA=")); fona.print (s);
    fona.println (F(",2000")); // timeout
    fona.expectReply (OK);

    // send data
    Serial.print(postData);
    fona.print(postData);
    delay(100);

    // do the POST request
    fona.HTTP_action (1, &statusCode, &dataLen, 10000);

    // report status, response data
    Serial.print (F("http code: ")); Serial.println (statusCode);
    Serial.print (F("reply len: ")); Serial.println (dataLen);

    if (fona.HTTP_readall(&dataLen))
    {
      // flush response
      while (dataLen > 0)
      {
        fonaFlush();
        dataLen--;
      }
    }
    fona.HTTP_term();

    return (statusCode == 204);
}
