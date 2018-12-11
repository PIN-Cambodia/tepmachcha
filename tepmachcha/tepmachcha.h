#include "arduino-mk.h"

//  Tepmachcha version number
#define VERSION "3.0.0"

//  Customize this for each installation
#include "config.h"           //  Site configuration

#include <DS1337.h>           //  For the Stalker's real-time clock (RTC)
#include <Sleep_n0m1.h>       //  Sleep library
#include <SoftwareSerial.h>   //  For serial communication with FONA
#include <Wire.h>             //  I2C library for communication with RTC
#include <Fat16.h>            //  FAT16 library for SD card
#include <EEPROM.h>           //  EEPROM lib
#include "Adafruit_FONA.h"
#include <ArduinoJson.h>

#define SERIAL_BUFFER_SIZE 32

#define WATCHDOG  3  //  (RTCINT2) low to reset
#define FONA_RST  4  //  FONA RST pin
#define FONA_RX   2  //  RX is into the module connected by default to Digital #2
#define SONAR     A0 //  Sonar pulse pin
#define FONA_TX   3  //  TX is out of the module, connected to Digital #3
#define SONAR_PWR 8  //  Sonar range pin -- pull low to turn off sonar
#define VCC_PWR   9  //  VCC power needed to communicate with the fona (Fona adapts to the voltage of VCC)

#define FONA_RTS 5 //   Tou can use this to control how fast data is sent out from the module to the Arduino, good when you want to read only a few bytes at a time.
#define FONA_KEY A2 //  FONA Key pin (TODO)
#define FONA_PS  A3 //  FONA power status pin (TODO)
#define SOLAR    A6 //  Solar charge state
#define BATT     A7 //  Battery level

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
