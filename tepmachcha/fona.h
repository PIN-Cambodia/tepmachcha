#include "Adafruit_FONA.h"
#include <SoftwareSerial.h>
extern SoftwareSerial fonaSS;
extern SoftwareSerial *fonaSerial;
extern Adafruit_FONA fona;

void fonaGetIMEI(char* imei);
void fonaPrintIMEI();
void fonaGetICCID(char* iccid);
void fonaPrintICCID();
void fonaSetupGPRS();
void fonaWaitForNetwork();
uint8_t fonaGetRSSI();
void fonaGetTime(char* buffer);
void fonaEnableNTPTimeSync();
bool fonaOn();
bool fonaOff();
bool fonaPOST(char* endpoint, char* post_data);
bool fonaPOSTWithRetry(char* endpoint, char* post_data, int retries);
