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
  lcdClear();
  lcdWriteLine(0,"Calibrating ADC");
  pfCalibrateStart();
  while (!pfCalibrateReady());
  lcdWriteLine(1,"Done");
}

char line[21];

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
    if (result) {
      if (result > 1) {
        lcdClear();
        if (result == 3) {
          lcdWriteLine(0,"Timeout");
        } else {
          lcdWriteLine(0,"Other Error");
        }
        sprintf(line,"S: %d T: %d",
                pfResults.samples, pfResults.time);
        lcdWriteLine(1,line);
      } else {
        lcdClear();
        sprintf(line,"%6.1f V  %6.3 A",
                pfResults.Urms,pfResults.Irms);
        lcdWriteLine(0,line);
        sprintf(line,"%6.1f W  %6.1 VA",
                pfResults.powerW,pfResults.powerVA);
        lcdWriteLine(1,line);
        sprintf(line,"pf=%4.2f",
                pfResults.powerFactor);
        lcdWriteLine(2,line);
        sprintf(line,"Freq. %6.2f",
                pfResults.frequency);
        lcdWriteLine(3,line);
      }
      pfStartMeasure();
    }
  }
}

/*
struct pfResults {
  float Upp, Ipp, Urms, Irms;
  float powerW, powerVA, powerFactor;
  float frequency;
  uint32_t samples,time;
};

*/
