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
  uint8_t i = 0;
  char line[21];
  line[1] = 0;
  lcdClear();
  lcdWriteLine(0,"Calibrating ADC");
  pfCalibrateStart();
  while ((i=pfCalibrating())) {
    sprintf(line,"%03d%%",i);
    lcdWriteLine(1,line);
    checkBootLoaderEntry(false);
    delay(100);
  }
  lcdWriteLine(1,"READY");
  delay(500);
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

        //  01234567890123456789
        //1 000.0 Vr  00.00 Ar
        //2 000.0 Vpp 00.00 App
        //3 0000.0W   0000.0VA
        //4 00.0Hz pf=0.00

        sprintf(line,"%5.1f Vr  %5.2 Ar",
                pfResults.Urms,pfResults.Irms);
        lcdWriteLine(0,line);
        sprintf(line,"%5.1f Vpp %5.2 App",
                pfResults.Upp,pfResults.Ipp);
        lcdWriteLine(1,line);
        sprintf(line,"%6.1f W   %6.1 VA",
                pfResults.powerW,pfResults.powerVA);
        lcdWriteLine(2,line);
        sprintf(line,"%4.1Hz pf=%4.2f",
                pfResults.frequency,
                pfResults.powerFactor);
        lcdWriteLine(3,line);
      }
      pfStartMeasure();
    }
  }
}

