#include "mbed.h"
#include "MFRC522.h"
#include <string>

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

#define CARD_RETRIES 4
#define WAIT_PERIOD  500 //How often do we check for new cards when none visible
#define GONE_PERIOD  50 //How often do we check if the card has gone
#define ENABLE_LINE  PTA0

DigitalOut LedRed   (LED_RED);
DigitalOut LedGreen (LED_GREEN);
DigitalOut LedBlue  (LED_BLUE);
DigitalOut ReaderReset (MF_RESET);

Serial     DebugUART(UART_TX, UART_RX);
MFRC522    RfChip   (SPI_MOSI, SPI_MISO, SPI_SCLK, SPI_CS, MF_RESET);

char *idstrbyte = (char*) malloc(2 * sizeof(char));


bool waitForCard() {
  uint8_t failedreads = 0;
  while(1)
  {
    if (failedreads > CARD_RETRIES) {
      return false;
    }
    LedBlue  = 1;
    LedRed   = 0;
    LedGreen = 0;
    // Look for new cards
    if ( ! RfChip.PICC_IsNewCardPresent())
    {
    //   wait_ms(WAIT_PERIOD);
       failedreads++;
       continue;
    }
    if ( ! RfChip.PICC_ReadCardSerial())
    {
      //wait_ms(WAIT_PERIOD);
      failedreads++;
      continue;
    }
    RfChip.PICC_HaltA();
    LedBlue  = 0;
    LedRed   = 1;
    LedGreen = 1;
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
  return true;
}

int main()
{
  /* Set debug UART speed */
  set_time(1256729737);  // Set RTC time to Wed, 28 Oct 2009 11:35:37
  time_t startusage = time(NULL);
  time_t endusage = time(NULL);
  DebugUART.baud(115200);
  DebugUART.printf("Hackspace Access Board v0.1\n\r");
  DebugUART.printf("\n\r");
  std::string idstr;
  std::string lastid = "This is not an id";
  bool cardWasValid = false;
  std::string machineuid = "27854af8-ebe3-5594-9ca0-8aea3f0a3f17";

  /* Init. RC522 Chip */
  RfChip.PCD_Init();

  /* Read RC522 version */
  uint8_t temp = RfChip.PCD_ReadRegister(MFRC522::VersionReg);
  DebugUART.printf("MFRC522 version: %d\n\r", temp & 0x07);
  DebugUART.printf("\n\r");

  while(1)
  {
    if (waitForCard()) {
      // Dump debug info about the card. PICC_HaltA() is automatically called. // UID
      idstr = readUID(RfChip);
      if (lastid != idstr) {
        //Actual new card check!
        lastid = idstr;
        printf("New card seen %s\n\r",idstr.c_str());
        startusage = time(NULL);
      }
    } else {
      if (lastid != "This is not an id") {
        printf("Card %s gone!", lastid.c_str());
        if (cardWasValid) {
          endusage = time(NULL);
          //Log the usage
        }
        lastid = "This is not an id";
      }
      printf("Waiting...\n");
    }
    ReaderReset = 0;
    wait_ms(100);
    ReaderReset = 1;
    RfChip.PCD_Init();
    wait_ms(100);


  }
}
