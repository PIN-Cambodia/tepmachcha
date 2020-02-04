// Various config
#define FIRMWARE_VERSION "4.0.0"
#define HARDWARE_MODEL "TEP"
#define HARDWARE_VERSION "4.0"
#define HARDWARE_SEQUENCE_ID "009" // <--- Change this per device to give a unique ID (you could alternatively use the board IMEI)
#define DEVICE_ID HARDWARE_MODEL "v" HARDWARE_VERSION "-" HARDWARE_SEQUENCE_ID
#define API_KEY "wfYGuHWTJ4XscdNVgogeoQtp"
#define SETUP_ENDPOINT "api.iot.ews1294.info/v1/ping" // NOTE: Needs to support http, https is not supported - DO NOT include http://
#define DATAPOINT_ENDPOINT "api.iot.ews1294.info/v1/data-point" // NOTE: Needs to support http, https is not supported - DO NOT include http://
#define FONA_APN   "smart"

#define FONA_RST 4 // Connected, but not used in the program
#define FONA_RTS 5 // Connected, but not used in the program
#define FONA_RX 6
#define FONA_TX 7
#define FONA_KEY 8 // Not currently in use - Needs to be cut & soldered

#define LED_PIN 13

#define SONAR_READ 3
#define SONAR_POWER A0

#include "SeeeduinoStalker.h"
extern Stalker stalker;
