#include "mbed.h"
#include "MFRC522.h"
#include <string>
#include "mbed.h"
#include "EthernetInterface.h"
#include "HTTPClient.h"
#include "NTPClient.h"

#if defined(TARGET_K64F)

/* K64F Pins for MFRC522 SPI interface */
#define SPI_MOSI    PTD2
#define SPI_MISO    PTD3
#define SPI_SCLK    PTD1
#define SPI_CS      PTD0

/* KL25Z Pin for MFRC522 reset */
#define MF_RESET    PTC4

/* KL25Z Pins for UART Debug port */
#define UART_RX     USBRX
#define UART_TX     USBTX

#elif defined(TARGET_LPC11U24)

/* LPC11U24 Pins for MFRC522 SPI interface */
#define SPI_MOSI    P0_9
#define SPI_MISO    P0_8
#define SPI_SCLK    P1_29
#define SPI_CS      P0_2

/* LPC11U24 Pin for MFRC522 reset */
#define MF_RESET    P1_13

/* LPC11U24 Pins for UART Debug port */
#define UART_RX     P0_18
#define UART_TX     P0_19

/* LED Pins */
#define LED_RED     P0_7
#define LED_GREEN   P1_22

#endif

#define CARD_RETRIES 20
#define WAIT_PERIOD  50 //How often do we check for new cards when none visible
#define GONE_PERIOD  50 //How often do we check if the card has gone
#define ENABLE_LINE  PTC3

DigitalOut LedRed   (LED_RED);
DigitalOut LedGreen (LED_GREEN);
DigitalOut LedBlue  (LED_BLUE);
DigitalOut ReaderReset (MF_RESET);
DigitalOut EnableLine (ENABLE_LINE);

Serial     DebugUART(UART_TX, UART_RX);
MFRC522    RfChip   (SPI_MOSI, SPI_MISO, SPI_SCLK, SPI_CS, MF_RESET);

char *idstrbyte = (char*) malloc(2 * sizeof(char));
std::string machineuid = "27854af8-ebe3-5594-9ca0-8aea3f0a3f17";
EthernetInterface eth;
NTPClient ntp;


bool waitForCard() {
  uint8_t failedreads = 0;
  while(1)
  {
    if (failedreads > CARD_RETRIES) {
      return false;
    }
    // Look for new cards
    if ( ! RfChip.PICC_IsNewCardPresent())
    {
       wait_ms(WAIT_PERIOD);
       failedreads++;
       continue;
    }
    if ( ! RfChip.PICC_ReadCardSerial())
    {
      wait_ms(WAIT_PERIOD);
      failedreads++;
      continue;
    }
    RfChip.PICC_HaltA();
    return true;
  }
}

std::string readUID(MFRC522 RfChip) {
  std::string idstr = "";
  printf("Card UID: ");
  for (uint8_t i = 0; i < RfChip.uid.size; i++)
  {
    printf(" %02X", RfChip.uid.uidByte[i]);
    sprintf(idstrbyte, "%02X", RfChip.uid.uidByte[i]);
    idstr.append(idstrbyte);
  }
  printf("\n\r");
  return(idstr);
}

bool validCard(std::string idstr) {
  HTTPClient http;
  char str[512];
  sprintf(str,"http://usage.ranyard.info/canuse/TestMachine/%s",idstr.c_str());
  std::string url = str;
  int ret = http.get(url.c_str(), str, 128);
  //  int ret = http.get("https://developer.mbed.org/media/uploads/donatien/hello.txt", str, 128);
  if (!ret)
    {
      printf("Return\n\r%s",str);
      return (strncmp(str,"true",4) == 0);
    }
    else
    {
      printf("Error - ret = %d - HTTP return code = %d\n", ret, http.getHTTPResponseCode());
      return false;
    }
  //printf("URL was %s\n\rReturn was %i\n\rContent was %s",url.c_str(),ret,str);
  //return (ret == 200);
}

bool logIt(time_t start, time_t end, std::string idstr) {
  HTTPClient http;
  char str[512];
  sprintf(str,"http://usage.ranyard.info/log/bycard/%s",idstr.c_str());
  HTTPMap map;
  HTTPText inText(str, 512);
  char ctimestr_start[80];
  char ctimestr_end[80];
  struct tm * timeinfo;
  map.put("machineuid", machineuid.c_str());
  printf("machineuid : %s\n\r",machineuid.c_str());
  timeinfo = localtime (&start);
  strftime(ctimestr_start,80,"%Y-%m-%d %H:%M:%S",timeinfo);
  map.put("starttime", ctimestr_start);
  printf("starttime : %s\n\r",ctimestr_start);
  timeinfo = localtime (&end);
  strftime(ctimestr_end,80,"%Y-%m-%d %H:%M:%S",timeinfo);
  map.put("endtime", ctimestr_end);
  printf("endtime : %s\n\r",ctimestr_end);
    printf("\nTrying to post data...\n");
    int ret = http.post(str, map, &inText);
    if (!ret)
    {
      printf("Executed POST successfully - read %d characters\n", strlen(str));
      printf("Result: %s\n", str);
    }
    else
    {
      printf("Error - ret = %d - HTTP return code = %d\n", ret, http.getHTTPResponseCode());
    }

}

int main()
{
  EnableLine = 0; // No fire on startup!
  LedBlue  = 1;
  LedRed   = 1;
  LedGreen = 1;
  /* Set debug UART speed */
  //set_time(1256729737);  // Set RTC time to Wed, 28 Oct 2009 11:35:37
  time_t startusage = time(NULL);
  time_t endusage = time(NULL);
  DebugUART.baud(115200);
  DebugUART.printf("Hackspace Access Board v0.1\n\r");
  DebugUART.printf("\n\r");
  std::string idstr;
  std::string lastid = "This is not an id";
  bool cardWasValid = false;
  eth.init(); //Use DHCP
  printf("Connecting to ethernet...");
  int connect_status = eth.connect();
  while ( 0 != connect_status) {
    connect_status = eth.connect();
    printf(".");
  }
  printf("done\n\rEthernet connected as %s\r\n", eth.getIPAddress());
  printf("Trying to update time...\r\n");
    if (ntp.setTime("0.pool.ntp.org") == 0)
    {
      printf("Set time successfully\r\n");
      time_t ctTime;
      ctTime = time(NULL);
      printf("Time is set to (UTC): %s\r\n", ctime(&ctTime));
    }
    else
    {
      printf("Error\r\n");
    }

  /* Init. RC522 Chip */
  RfChip.PCD_Init();

  /* Read RC522 version */
  uint8_t temp = RfChip.PCD_ReadRegister(MFRC522::VersionReg);
  DebugUART.printf("MFRC522 version: %d\n\r", temp & 0x07);
  DebugUART.printf("\n\r");
    //Blue when waiting...
    LedBlue  = 0;
    LedRed   = 1;
    LedGreen = 1;

  while(1)
  {
    if (waitForCard()) {
      // Dump debug info about the card. PICC_HaltA() is automatically called. // UID
      idstr = readUID(RfChip);
      if (lastid != idstr) {
        //Actual new card check!
        lastid = idstr;
        printf("New card seen %s\n\r",idstr.c_str());
        if (validCard(idstr)) {
          startusage = time(NULL);
          printf("Valid card!\n\r");
          cardWasValid = true;
          LedRed = 1;
          LedBlue = 1;
          LedGreen = 0;
          EnableLine = 1;
        } else {
          printf("NOT A Valid card!\n\r");
          LedRed = 0;
          LedBlue = 1;
          LedGreen = 1;
        }
      }
    } else {
      //Blue when waiting...
      LedBlue  = 0;
      LedRed   = 1;
      LedGreen = 1;
      EnableLine = 0;
      if (lastid != "This is not an id") {
        printf("Card %s gone!\n\r", lastid.c_str());
        if (cardWasValid) {
          endusage = time(NULL);
          unsigned int usage = difftime(endusage,startusage);
          printf("I felt used for %u seconds\n\r",usage);
          //Log the usage
          logIt(startusage,endusage,lastid);
        }
        lastid = "This is not an id";
        cardWasValid = false;
      }
      printf("Waiting...\n\r");
    }
    ReaderReset = 0;
    wait_ms(300);
    ReaderReset = 1;
    RfChip.PCD_Init();
    wait_ms(300);


  }
}
