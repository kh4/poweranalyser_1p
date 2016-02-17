#include "board.h"

// inputs:
//  voltage via dividers 400k into 3k3 up and down + 470pF filter
//    output 4.108mV/V; ADC full range (3v3) maps to 803.3v
//      => voltage = 803.3 * adcval / 4096
//  current from ADC758-50B 40mV/A
//     full range maps to 82.5A
//     => current = 82.5 * adcval / 4096
//
//  We use 'raw adc unit' as long as possible ;)
//
//

#define USCALE (803.3 / 4096.0)
#define ISCALE (82.5 / 4096.0)

#define CYCLES 100

volatile int16_t  minU, maxU, minI, maxI;
volatile int32_t  sumU2, sumI2;
volatile int32_t  sumUI;
volatile uint32_t measurementTime;
volatile uint32_t __samples;
volatile uint8_t  __state = 0;
volatile uint16_t __cycles;
uint32_t __start;


struct pfResults pfResults;

#define ZC_THRESHOLD -200


void resetMeasurement()
{
  maxU = minU = maxI = minI = 0;
  sumU2 = sumI2 = sumUI = 0;
  __samples = __cycles = 0;
}

void integrateMeasurement(int16_t u, int16_t i) // u in 0.1V, i in 1mA
{
  if (u > maxU) maxU = u;
  if (u < minU) minU = u;
  if (i > maxI) maxI = i;
  if (i < minI) minI = i;
  sumU2 += (int32_t)u * (int32_t)u;
  sumI2 += (int32_t)i * (int32_t)i;
  sumUI = u * i;
  __samples++;
}

int detectZC(int16_t u)
{
  bool zcstate = 0;
  if (zcstate) {
    if (u >= 0) {
      zcstate = 0;
      return 1;
    }
  } else {
    if (u < ZC_THRESHOLD)
      zcstate = 1;
  }
  return 0;
}


void handleValuesFromADC(int16_t values[4]) // values are U, I, I*10, I*100
{
  int16_t _u = values[0];
  int16_t _i = values[1];
  int16_t _i10 = values[2];
  int16_t _i100 = values[3];

  if ((__state == 0) || (__state == 3))
    return;

  if (__state == 1) {
    // wait until it crosses positive and go into measurement mode
    if (detectZC(_u)) {
      measurementTime = micros();
      __state = 2;
    } else {
      return;
    }
  }

  if (detectZC(_u)) __cycles++;

  if (__cycles >= CYCLES) {
    __state = 3; // measurement done
    measurementTime = micros() - measurementTime;
    return;
  }

  integrateMeasurement(_u, _i);

}

#define NOZC_TIME 2000 // 2s
#define TIMEOUT  5000

void pfStartMeasure()
{ // return non zero if in trouble
  __start = millis();
  resetMeasurement();
  __state = 1;

}

uint8_t pfWaitMeasure()
{
  // sampling with ZC detection
  if ((__state == 1) || (__state == 2)) {
    if ((millis() - __start) > TIMEOUT) {
      __state = 0;
      return 2; // error
    } else {
      return 1; // not ready
    }
  }
  __state = 0;

  pfResults.frequency = 1000000.0 * (float) CYCLES / (float)measurementTime;

  pfResults.Upp = (float)(maxU - minU) * 0.1;   // unit is 0.1V
  pfResults.Ipp = (float)(maxI - minI) * 0.001; // unit is mA

  pfResults.Urms = sqrtf((float)sumU2 / (float)__samples) * 0.1;
  pfResults.Irms = sqrtf((float)sumI2 / (float)__samples) * 0.001;

  pfResults.powerW  = pfResults.Urms * pfResults.Irms;
  pfResults.powerVA = (float)sumUI / (float)__samples * 0.0001;
  pfResults.powerFactor = pfResults.powerVA / pfResults.powerW;

  return 0;
}


