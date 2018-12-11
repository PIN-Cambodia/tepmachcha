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
		pinMode (SONAR_PWR, OUTPUT);
		pinMode (FONA_KEY, OUTPUT);
		pinMode (FONA_RX, OUTPUT);
    pinMode (VCC_PWR, OUTPUT);

    digitalWrite (SONAR_PWR, LOW);           // sonar off
		digitalWrite (FONA_KEY, HIGH);       // Initial state for key pin
    digitalWrite (VCC_PWR, HIGH);        // Needed to reduce noise in serial communication with the fona board. 
                                         // Note that D9 is a control pin on the Stalker 3.1 - It's not connected 
                                         // to anything as such in our project

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
        fonaOff();
				Serial.println (F("Low power sleep"));
				Serial.flush();
				digitalWrite (SONAR_PWR, LOW);        //  Make sure sonar is off
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

      // Delete any accumulated SMS messages to avoid interference from old commands
      smsDeleteAll();

      // TODO: Post HELO to server
    }
    fonaOff();

		RTC.enableInterrupts (EveryMinute);  //  RTC will interrupt every minute
		RTC.clearINTStatus();                //  Clear any outstanding interrupts
		sleep.sleepInterrupt (RTCINTA, FALLING); //  Sleep; wake on falling voltage on RTC pin
}


// This runs every minute, triggered by RTC interrupt
void loop (void)
{
		DateTime now = RTC.now();      //  Get the current time from the RTC

		Serial.print (now.hour());
		Serial.print (F(":"));
		Serial.println (now.minute());

    // Check if it is time to send a scheduled reading
		if (now.minute() % INTERVAL == 0) {
      upload ();
    }

		Serial.println(F("sleeping"));
		Serial.flush();                         //  Flush any output before sleep

		sleep.pwrDownMode();                    //  Set sleep mode to "Power Down"
		RTC.clearINTStatus();                   //  Clear any outstanding RTC interrupts
		sleep.sleepInterrupt (RTCINTA, FALLING); //  Sleep; wake on falling voltage on RTC pin
}


void upload()
{
  int16_t distance = sonarRead();
  uint8_t status = 0;
  uint16_t voltage;
  uint16_t solarV;
  boolean charging;
  int16_t streamHeight;

  voltage = batteryRead();
  solarV = solarVoltage();
  charging = solarCharging(solarV);

  Serial.print (F("Uploading..."));

  if (fonaOn() || (fonaOff(), delay(5000), fonaOn())) // try twice
  {

    uint8_t attempts = 2; do
    {
      status = ews1294Post(distance, charging, solarV, voltage);
    } while (!status && --attempts);

    // if the upload failed the fona can be left in an undefined state,
    // so we reboot it here to ensure SMS works
    if (!status)
    {
      fonaOff(); wait(2000); fonaOn();
    }

    // process SMS messages
    smsCheck();
  }
  fonaOff();
}


// Don't allow ewsPost() to be inlined, as the compiler will also attempt to optimize stack
// allocation, and ends up preallocating at the top of the stack. ie it moves the beginning
// of the stack (as seen in setup()) down ~200 bytes, leaving the rest of the app short of ram
boolean __attribute__ ((noinline)) ews1294Post (int16_t distance, boolean charging, uint16_t solarV, uint16_t voltage)
{
    uint16_t status_code = 0;
    uint16_t response_length = 0;
    char post_data[255];

    DEBUG_RAM

    // Construct the body of the POST request:
    sprintf_P (post_data,
      (prog_char *)F("{\"apiKey\": \"" EWSAPI_KEY "\", \"source\": \"" EWSDEVICE_ID "\", \"payload\":{\"distance\":%d,\"solarVoltage\":%d, \"charging\":%d,\"voltage\":%d, \"version\":\"" VERSION "\",\"internalTemp\":%d,\"freeRam\":%d}}\r\n"),
        distance,
        solarV,
        charging,
        voltage,
        internalTemp(),
        freeRam()
    );

    Serial.println (post_data);

    // ews1294.info does not currently support SSL; if it is added you will need to uncomment the following
    //fona.sendCheckReply (F("AT+HTTPSSL=1"), F("OK"));   //  Turn on SSL
    //fona.sendCheckReply (F("AT+HTTPPARA=\"REDIR\",\"1\""), F("OK"));  //  Turn on redirects (for SSL)

    // Send the POST request we have constructed
    if (fona.HTTP_POST_start (POST_ENDPOINT,
                              F("application/json"),
                              (uint8_t *)post_data,
                              strlen(post_data),
                              &status_code,
                              &response_length)) {

      
      // Read response
      if(status_code != 201) {
        fonaFlush();
        Serial.print (F("POST failed. Status-code: "));
        Serial.println (status_code);
        
        return false;
      } else {
        Serial.print(F("POST succeeded. Getting timestamp.. "));
        int colons = 0;
        long timestamp = 0;
        long e = 1000000000;            // We use this to build the timestamp

        while (response_length > 0) {
          while (fona.available()) {
            char c = fona.read();
            if(c == ':')
              colons++;
            if(colons == 2 && c >= '0' && c <= '9' && e > 0) {
              // Note -48: Since the ASCII char codes for 0-9 start at 48, we can get the int value of the
              // char by simply subtracting 48 from the ASCII char reference
              timestamp = timestamp + (c-48)*e;
              e = e/10;
            }
            //Serial.print(c);
            response_length--;
            // Break out of fona.available()
            if (! response_length) break;
          }
        }

        // Here's a quirk; RTC starts at year 2000, so we need to subtract 30 years (Unix Epoch 1970) from the timestamp, including the 7 leap year extra days there were between 1970 and 2000
        timestamp = timestamp - (30L*365+7)*24*60*60;

        Serial.print(timestamp);
        Serial.print(F(". Adjusting RTC... "));
        DateTime dt(timestamp);
        Serial.print(F(" ("));
        Serial.print(dt.iso8601());
        Serial.print(F(")..."));
        RTC.adjust(dt);
        Serial.println(F(" Done"));
        
        return true;
      }
    } else {
      Serial.println(F("POST failed"));
      return false;
    }
}
