#include "arduino-mk.h"

//  Tepmachcha version number
#define VERSION "2.8.0"

//  Customize this for each installation
#include "config.h"           //  Site configuration

#include <DS1337.h>           //  For the Stalker's real-time clock (RTC)
#include <Sleep_n0m1.h>       //  Sleep library
#include <SoftwareSerial.h>   //  For serial communication with FONA
#include <Wire.h>             //  I2C library for communication with RTC
#include <Fat16.h>            //  FAT16 library for SD card
#include <EEPROM.h>           //  EEPROM lib
#include "Adafruit_FONA.h"

// save RAM by reducing HW serial rcv buffer (default 64)
// Note the XBee has a buffer of 100 bytes, but as we don't
// receive much from XBee we can reduce the buffer.
#define SERIAL_BUFFER_SIZE 32

#define RTCINTA 0    //  RTC INTA
#define RTCINTB 1    //  RTC INTB

// tepmachcha DAI
#ifdef SHIELDv1
#define RTCINT1  2  //  Onboard Stalker RTC pin
#define FONA_RST 3  //  FONA RST pin
#define SD_POWER 4  //  optional power to SD card
#define BEEPIN   5  //  XBee power pin
#define FONA_RX  6  //  UART pin into FONA
#define PING     7  //  Sonar ping pin
#define RANGE    8  //  Sonar range pin -- pull low to turn off sonar
#define FONA_TX  9  //  UART pin from FONA

#define FONA_RTS A1 //  FONA RTS pin
#define FONA_KEY A2 //  FONA Key pin
#define FONA_PS  A3 //  FONA power status pin
#define SOLAR    A6 //  Solar charge state
#define BATT     A7 //  Battery level
#endif

// tepmachcha v2
#ifdef SHIELDv2
#define RTCINT1  2  //  Onboard Stalker RTC pin
#define WATCHDOG 3  //  (RTCINT2) low to reset
#define FONA_RST A1 //  FONA RST pin
#define SD_POWER 4  //  optional power to SD card
#define BEEPIN   5  //  XBee power pin
#define FONA_RX  6  //  UART pin into FONA
#define PING     A0 //  Sonar ping pin
#define FONA_TX  7  //  UART pin from FONA
#define RANGE    8  //  Sonar range pin -- pull low to turn off sonar
#define BUS_PWR  9  //  Peripheral bus power for 3.1

#define FONA_RTS na //  FONA RTS pin - check
#define FONA_KEY A2 //  FONA Key pin
#define FONA_PS  A3 //  FONA power status pin
#define SOLAR    A6 //  Solar charge state
#define BATT     A7 //  Battery level
#endif

#define DEBUG_RAM     debugFreeRam();

// Expand #define macro value to a string
#define STR_EXPAND(tok) #tok
#define STR(tok) STR_EXPAND(tok)

// Device string includes date and time; helps identify version
// Note: C compiler concatenates adjacent strings
#define DEVICE "Tepmachcha v" VERSION " " __DATE__ " " __TIME__ " ID:" EWSDEVICE_ID " " STR(SENSOR_HEIGHT) "cm"

// tepmachcha
extern const char DEVICE_STR[] PROGMEM;

// File
extern char file_name[13];              // 8.3
extern uint16_t file_size;

extern DateTime now;
extern DS1337 RTC;         //  Create the DS1337 real-time clock (RTC) object
extern Sleep sleep;        //  Create the sleep object

// fona
#define OK ((__FlashStringHelper*)OK_STRING)
extern const char OK_STRING[] PROGMEM;
extern SoftwareSerial fonaSerial;
extern Adafruit_FONA fona;
