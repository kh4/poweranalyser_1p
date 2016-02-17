#include "board.h"

#define LOOPDELAY 200

int8_t screen = 0;
int8_t oldscreen = 99;

static void _putc(void *p, char c)
{
  uartWrite(c);
}


void checkBootLoaderEntry(bool wait)
{
  uint32_t start = millis();
  do {
    if (uartAvailable() && ('R' == uartRead())) {
      lcdClear();
      lcdWriteLine(0, "Entering bootloader.");
      systemReset(true);
      while (1);
    }
  }  while (wait && ((millis() - start) < 2000));
}

void calibrate()
{
  lcdWriteLine(0,"Calibrating ADC");
  // tbd
  lcdWriteLine(1,"Done");
}

int main(void)
{
  systemInit();
  init_printf(NULL, _putc);
  uartInit(115200);
  lcdInit();
  checkBootLoaderEntry(true);
  ledInit();
  // rotaryInit();
  adcInit(handleValuesFromADC);
  delay(10);
  calibrate();
  // loop
  pfStartMeasure();
  while (1) {
    uint8_t result;
    delay(10);
    checkBootLoaderEntry(false);
    result = pfWaitMeasure();
    if (result == 2) {
      printf("Error..\n");
      pfStartMeasure();
    } else if (result==0) {
      printf("Results...\n");
      pfStartMeasure();
    }
  }
}
