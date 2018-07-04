#include "tepmachcha.h"

DateTime now;
DS1337 RTC;         //  Create the DS1337 real-time clock (RTC) object


uint8_t daysInMonth(uint8_t month)  // month 1..12
{
  static uint8_t const daysInMonthP [] PROGMEM = { 31,28,31,30,31,30,31,31,30,31,30,31 };
  return pgm_read_byte(daysInMonthP + month - 1);
}


void clockSet (void)
{
		char theDate[17];

		Serial.println (F("Fetching GSM time"));
		wait (1000);    //  Give time for any trailing data to come in from FONA
		fonaFlush();    //  Flush any trailing data

		fona.sendCheckReply (F("AT+CIPGSMLOC=2,1"), OK);    //  Query GSM location service for time
		
		fona.parseInt();                    //  Ignore first int - should be status (0)
		int secondInt = fona.parseInt();    //  Ignore second int -- necessary on some networks/towers
		int netYear = fona.parseInt();      //  Get the results -- GSMLOC year is 4-digit
		int netMonth = fona.parseInt();
		int netDay = fona.parseInt();
		int netHour = fona.parseInt();      //  GSMLOC is _supposed_ to get UTC; we will adjust
		int netMinute = fona.parseInt();
		int netSecond = fona.parseInt();    //  Our seconds may lag slightly, of course

		if (netYear < 2017 || netYear > 2050 || netHour > 23) //  If that obviously didn't work...
		{
				Serial.println (F("Recombobulating..."));

				netSecond = netMinute;  //  ...shift everything up one to capture that second int
				netMinute = netHour;
				netHour = netDay;
				netDay = netMonth;
				netMonth = netYear;
				netYear = secondInt;
		}

		if (netYear < 2017 || netYear > 2050)  // If that still didn't work...
		{
				// get time from the NTP pool instead: 
				// (https://en.wikipedia.org/wiki/Network_Time_Protocol)
        //
				fona.enableNTPTimeSync (true, F("0.daimakerlab.pool.ntp.org"));
				Serial.println (F("GSM location failed, trying NTP sync"));
				
				wait (15000);                 // Wait for NTP server response
				
				fona.println (F("AT+CCLK?")); // Query FONA's clock for resulting NTP time              
        //AT+CCLK="31/12/00,17:59:59+22"
				netYear = fona.parseInt();    // Capture the results
				netMonth = fona.parseInt();
				netDay = fona.parseInt();
				netHour = fona.parseInt();    // We asked NTP for UTC and will adjust below
				netMinute = fona.parseInt();
				netSecond = fona.parseInt();  // Our seconds may lag slightly
		}


    if (netYear > 2000) { netYear -= 2000; }  // Adjust from YYYY to YY
		if (netYear > 17 && netYear < 50 && netHour < 24)  // Date looks valid
		{
				// Adjust UTC to local time
        int8_t localhour = (netHour + UTCOFFSET);
				if ( localhour < 0)                   // TZ takes us back a day
				{
				    netHour = localhour + 24;         // hour % 24
            if (--netDay == 0)                // adjust the date to UTC - 1
            {
              if (--netMonth == 0)
              {
                netMonth = 12;
                netYear--;
              }
              netDay = daysInMonth(netMonth);
            }
				}
				else if (localhour > 23)              // TZ takes us to the next day
        {
            netHour = localhour - 24;         // hour % 24
            if (++netDay > daysInMonth(netMonth)) // adjust the date to UTC + 1
            {
              if (++netMonth > 12)
              {
                 netMonth = 1;
                 netYear++;
              }
              netDay = 1;
            }
        }
        else                                  // TZ is same day
        {
            netHour = localhour;              // simply add TZ offset
        }

				Serial.print (F("Net time: "));
				sprintf_P(theDate, (prog_char*)F("%d/%d/%d %d:%d"), netDay, netMonth, netYear, netHour, netMinute);
				Serial.print (theDate);

				Serial.println(F(". Adjusting RTC"));
				DateTime dt(netYear, netMonth, netDay, netHour, netMinute, netSecond, 0);
				RTC.adjust(dt);     //  Adjust date-time as defined above
		}
		else
		{
				Serial.println (F("Sync failed, using RTC"));
		}

		wait (200);              //  Give FONA a moment to catch its breath
}
