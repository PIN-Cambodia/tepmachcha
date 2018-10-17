#include "arduino-mk.h"

//  Tepmachcha version number
#define VERSION "2.2.1"

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

#define RTCINT1  2  //  Onboard Stalker RTC pin
#define WATCHDOG 3  //  (RTCINT2) low to reset
#define FONA_RST A1 //  FONA RST pin
#define FONA_RX  6  //  UART pin into FONA
#define PING     A0 //  Sonar ping pin
#define FONA_TX  7  //  UART pin from FONA
#define RANGE    8  //  Sonar range pin -- pull low to turn off sonar
#define BUS_PWR  9  //  VCC power needed to communicate with the fona without too much noise

#define FONA_RTS na //  FONA RTS pin - check
#define FONA_KEY A2 //  FONA Key pin
#define FONA_PS  A3 //  FONA power status pin
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
