
/*#include "tepmachcha.h"

// call into bootloader jumptable at top of flash
#define write_flash_page (*((void(*)(const uint32_t address))(0x7ffa/2)))
#define flash_firmware (*((void(*)(const char *))(0x7ffc/2)))
#define EEPROM_FILENAME_ADDR (E2END - 1)

uint8_t error;
const uint8_t CHIP_SELECT = SS;  // SD chip select pin (SS = pin 10)
SdCard card;
Fat16 file;
char file_name[13];              // 8.3
uint16_t file_size;


boolean fileInit(void)
{
  digitalWrite (SD_POWER, LOW);        //  SD card on
  wait (1000);

  // init sd card
  if (!card.begin(CHIP_SELECT))
  {
    Serial.print(F("failed card.begin ")); Serial.println(card.errorCode);
	  return false;
  }
  
  // initialize FAT16 volume
  // if (!Fat16::init(&card)) // JACK
  if (!Fat16::init(&card, 1))
  {
	  Serial.println(F("Fat16::init failed"));
	  return false;
  }

  return true;
}


boolean fileOpen(uint8_t mode)
{
  Serial.print(F("opening file (mode 0x"));
  Serial.print(mode, HEX);
  Serial.print(F("):"));
  Serial.println(file_name);
  return file.open(file_name, mode);
}

boolean fileOpenWrite(void) { return(fileOpen(O_CREAT | O_WRITE | O_TRUNC)); }

boolean fileOpenRead(void)  { return(fileOpen(O_READ)); }

boolean fileClose(void)
{
  file.close();
  digitalWrite (SD_POWER, HIGH);        //  SD card off
}


uint32_t fileCRC(uint32_t len)
{
  char c;
  uint32_t crc = ~0L;

  for(int i = 0; i < len; i++)
  {
    c = file.read();
    crc = crc_update(crc, c);
  }
  return ~crc;
}


// write firmware filename to EEPROM and toggle boot-from-SDcard flag at EEPROM[E2END]
void eepromWrite(void)
{
  uint8_t x;

  for (x = 0; x < 12 && file_name[x] != 0; x++)
  {
    //if (EEPROM.read( (EEPROM_FILENAME_ADDR - x) != file_name[x] ))
      EEPROM.write( (EEPROM_FILENAME_ADDR - x), file_name[x] );
  }
  EEPROM.write(E2END, 0); // 0 triggers an attempt to flash from SD card on power-on or reset
}

// Read len bytes from fona serial and write to file buffer
uint16_t fonaReadBlock(uint16_t len)
{
  uint16_t n = 0;
  uint32_t timeout = millis() + 2500;

  xon();
  while (n < len && millis() < timeout)
  {
    if (fona.available())
    {
      if (!file.write(fona.read())) break;
      n++;
    }
  }
  xoff();

  return n;
}


boolean fonaFileSize()
{
  fona.sendCheckReply (F("AT+FSFLSIZE=C:\\User\\ftp\\tmp.bin"), OK);
}


#define BLOCK_ATTEMPTS 3
#define BLOCK_SIZE 512
boolean fonaFileCopy(uint16_t len)
{
  uint32_t address = 0;
  uint16_t size = BLOCK_SIZE;
  uint16_t n;
  uint8_t retry_attempts = BLOCK_ATTEMPTS;
  DEBUG_RAM

  while (address < len)
  {
    fonaFlush();  // flush any notifications

    if (!retry_attempts--) return false;

    file.seekSet(address);   // rewind to beginning of block
    Serial.print(address); Serial.print(':');

    fona.print(F("AT+FSREAD=C:\\User\\ftp\\tmp.bin,1,"));
    fona.print(size);
    fona.print(',');
    fona.println(address);

    // fona returns \r\n from submitting command
    if (fonaRead() != '\r') continue;  // start again if it's something else
    if (fonaRead() != '\n') continue;

    n = fonaReadBlock(size);
    Serial.print(n);

    if (n == size && fona.expectReply(OK))
    {
      if (!file.sync())
      {
        return false;
      }
    } else { // didn't get a full block, or missing OK status response
      Serial.println();
      continue;
    }

    // success, reset attempts counter, move to next block
    retry_attempts = BLOCK_ATTEMPTS;
    address += size;
    if (address + size > len)  // CHECKME should by >= ??
    {
      size = len - address;    // CHECKME len-address, or len-address-1 ??
    }
  }
  return true;
}


void ftpEnd(void)
{
  fona.sendCheckReply (F("AT+FTPQUIT"), OK);
}


// Fetch firmware from FTP server to FONA's internal filesystem
boolean ftpGet(void)
{
  // configure FTP
  fona.sendCheckReply (F("AT+SSLOPT=0,1"), OK); // 0,x dont check cert, 1,x client auth
  fona.sendCheckReply (F("AT+FTPSSL=0"), OK);   // 0 ftp, 1 implicit (port is an FTPS port), 2 explicit
  fona.sendCheckReply (F("AT+FTPCID=1"), OK);
  fona.sendCheckReply (F("AT+FTPMODE=1"), OK);     // 0 ACTIVE, 1 PASV
  fona.sendCheckReply (F("AT+FTPTYPE=\"I\""), OK); // "I" binary, "A" ascii
  fona.sendCheckReply (F("AT+FTPSERV=\"" FTPSERVER "\""), OK);
  fona.sendCheckReply (F("AT+FTPUN=\"" FTPUSER "\""), OK);
  fona.sendCheckReply (F("AT+FTPPW=\"" FTPPW "\""), OK);
  fona.sendCheckReply (F("AT+FTPGETPATH=\"" FTPPATH "\""), OK);

  // remote filename
  Serial.print(F("AT+FTPGETNAME=\""));
  Serial.print(file_name);
  Serial.println(F("\""));

  fona.print(F("AT+FTPGETNAME=\""));
  fona.print(file_name);
  fona.println(F("\""));
  fona.expectReply (OK);

  // local file path on fona
  fona.sendCheckReply (F("AT+FSDEL=C:\\User\\ftp\\tmp.bin"), OK); // delete previous download file

  // start the download to local file
  if ( !fona.sendCheckReply (F("AT+FTPGETTOFS=0,\"tmp.bin\""), OK))
  {
    return false;
  }

  // Wait for download complete; FTPGETOFS status 0
  uint32_t timeout = millis() + 90000;
  while( !fona.sendCheckReply (F("AT+FTPGETTOFS?"), F("+FTPGETTOFS: 0")) ) {
    delay(2000);
    if (millis() > timeout)
    {
      ftpEnd();
      return false;
    }
  }
  delay(2000);
  ftpEnd();

  // Check the file exists
  if (fona.sendCheckReply (F("AT+FSFLSIZE=C:\\User\\ftp\\tmp.bin"), F("ERROR")))
  {
    return false;
  }

  return true;
}


boolean firmwareGet(void)
{ 
  Serial.println(F("Fetching FW"));

  if (!ftpGet()) error = 10; else
  {
    uint8_t tries=3 ; do
    {
      if (!fileInit()) error = 20; else
      {
        if (!fileOpenWrite()) error = 30; else
        {
          if (!fonaFileCopy(file_size)) error = 40; else
          {
            fileClose();
            error = 0;
            return true;
          }
        }
      }
      fileClose();
    } while (--tries);
  }
  Serial.println(F("fona copy failed"));
  return false;
}


void reflash (void) {
    Serial.println(F("update eeprom"));
    eepromWrite();

    Serial.println(F("reflash"));

    delay(100);

    // Jump to bootloader
    flash_firmware(file_name);
}
*/
