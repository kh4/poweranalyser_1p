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
  printf("Initializing...\n");
  calibrate();
  printf("Running...\n");
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
	sprintf(line,"Error %d", result);
	lcdWriteLine(0,line);
	delay(500);
      } else {
        int16_t t1,t2,t3,t4;
        char   s1,s2;
        lcdClear();

        //  01234567890123456789
        //1 000.0 Vr  00.00 Ar
        //2 000.0 Vpp 00.00 App
        //3 0000.0W   0000.0VA
        //4 00.0Hz pf=0.00

        t1 = abs(pfResults.Urms * 10);
        t2 = t1 % 10;
        t1 = t1 / 10;
        s1 = (pfResults.Urms < 0)?'-':' ';

        t3 = abs(pfResults.Irms * 100);
        t4 = t3 % 100;
        t3 = t3 / 100;
        s2 = (pfResults.Irms < 0)?'-':' ';

        sprintf(line,"%c%03d.%01d Vr  %c%02d.%02d Ar",
                s1,t1,t2,s2,t3,t4);
        lcdWriteLine(0,line);

        t1 = abs(pfResults.Upp * 10);
        t2 = t1 % 10;
        t1 = t1 / 10;
        s1 = (pfResults.Upp < 0)?'-':' ';
        t3 = abs(pfResults.Ipp * 100);
        t4 = t3 % 100;
        t3 = t3 / 100;
        s2 = (pfResults.Ipp < 0)?'-':' ';
        sprintf(line,"%c%03d.%01d Vp  %c%02d.%02d Ap",
                s1,t1,t2,s2,t3,t4);
        lcdWriteLine(1,line);

        t1 = abs(pfResults.powerW * 10);
        t2 = t1 % 10;
        t1 = t1 / 10;
        s1 = (pfResults.powerW < 0)?'-':' ';
        t3 = abs(pfResults.powerVA * 10);
        t4 = t3 % 10;
        t3 = t3 / 10;
        s2 = (pfResults.powerVA < 0)?'-':' ';
        sprintf(line,"%c%04d.%01d W %c%04d.%01d VA",
                s1,t1,t2,s2,t3,t4);

        lcdWriteLine(2,line);
        t1 = pfResults.frequency * 10;
        t2 = t1 % 10;
        t1 = t1 / 10;
        t3 = abs(pfResults.powerFactor * 100);
        t4 = t3 % 100;
        t3 = t3 / 100;
        s2 = (pfResults.powerFactor < 0)?'-':' ';
        sprintf(line,"%3d.%1dHz pf=%4d.%2d",
                t1,t2,s2,t3,t4);
	sprintf(line,"n=%d",pfResults.samples);
        lcdWriteLine(3,line);
      }
      pfStartMeasure();
    }
  }
}

