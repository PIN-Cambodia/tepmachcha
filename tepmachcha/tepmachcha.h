#include "arduino-mk.h"

//  Tepmachcha version number
#define VERSION "3.0.0"

//  Customize this for each installation
#include "config.h"           //  Site configuration

#include <DS1337.h>           //  For the Stalker's real-time clock (RTC)
#include <Sleep_n0m1.h>       //  Sleep library
#include <SoftwareSerial.h>   //  For serial communication with FONA
#include <SeeeduinoStalker.h> //  Seeduino Stalker v3.1 library, primarily useful for battery level and solar charging information
#include <Wire.h>             //  I2C library for communication with RTC
#include <Fat16.h>            //  FAT16 library for SD card
#include <EEPROM.h>           //  EEPROM lib
#include "Adafruit_FONA.h"    //  Fona library
#include <ArduinoJson.h>      //  JSON library

#define SERIAL_BUFFER_SIZE 32

#define FONA_RX  2    //  RX is into the module connected by default to Digital #2
#define FONA_TX  3    //  TX is out of the module, connected to Digital #3
#define FONA_RST 4    //  Hard reset pin. By default it has a high pull-up (module not in reset). If you absolutely got the module in a bad space, toggle this pin low for 100ms to perform a hard reset. We tie this to Digital #4, and the library does a hard-reset so you always have a fresh setup.
#define FONA_RTS 5    //  (Ready To Send) - This is the module's flow control pin, you can use this to control how fast data is sent out from the module to the Arduino, good when you want to read only a few bytes at a time.

#define SONAR_PWR 6   //  Connected to sonar pin 4, pull HIGH to turn on, LOW to turn off
#define SONAR_PW  7   //  Connected to sonar pin 2, Pulse Width Output: This pin outputs a pulse width representation of the distance with a scale factor of 1uS per mm. The pulse width output is sent with a value within 0.5% of the serial output.
#define BUS_PWR  9    //  Stalker 3.1 VCC power manager - needed to communicate with the fona without too much noise (TODO: To confirm)
#define SD_POWER 4    //  Power manager to the SD card

#define DEBUG_RAM     debugFreeRam();

// Expand #define macro value to a string
#define STR_EXPAND(tok) #tok
#define STR(tok) STR_EXPAND(tok)

// Device string includes date and time; helps identify version
// Note: C compiler concatenates adjacent strings
#define DEVICE "Tepmachcha v" VERSION " " __DATE__ " " __TIME__ " ID:" EWSDEVICE_ID

// tepmachcha
extern const char DEVICE_STR[] PROGMEM;

// File
extern char file_name[13];              // 8.3
extern uint16_t file_size;

extern DS1337 RTC;         //  Create the DS1337 real-time clock (RTC) object
extern Sleep sleep;        //  Create the sleep object

// fona
#define OK ((__FlashStringHelper*)OK_STRING)
extern const char OK_STRING[] PROGMEM;
extern SoftwareSerial fonaSerial;
extern Adafruit_FONA fona;
