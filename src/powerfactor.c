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

#define USCALE (1.0)
#define ISCALE (1.0)

#define CYCLES 100
#define CAL_CYCLES 500

#define TIMEOUT_US 3000000
#define CAL_TIMEOUT_US 5000000

volatile int16_t  minU, maxU, minI, maxI;
volatile int32_t  sumU2, sumI2;
volatile int32_t  sumUI;
volatile uint32_t measurementTime;
volatile int32_t __samples;
volatile int16_t __cycles;

int16_t caloffset[2] = {0,0};
int32_t calsum[2];

#define MEASUREMENT_STARTED 1
#define MEASUREMENT_RUNNING 2
#define MEASUREMENT_DONE    4
#define MEASUREMENT_VALID   8 // if not set likely timeout occured
#define MEASUREMENT_CALIBRATE 128

volatile uint8_t measurementState;
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
  if (u > maxU) {
    maxU = u;
  }
  if (u < minU) {
    minU = u;
  }
  if (i > maxI) {
    maxI = i;
  }
  if (i < minI) {
    minI = i;
  }
  sumU2 += (int32_t)u * (int32_t)u;
  sumI2 += (int32_t)i * (int32_t)i;
  sumUI = u * i;
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
    if (u < ZC_THRESHOLD) {
      zcstate = 1;
    }
  }
  return 0;
}

volatile int16_t lastu,lasti;
volatile int16_t lastuc,lastic;

void handleValuesFromADC(int16_t values[2]) // values are U, I
{
  values[0]-=2048;
  values[1]-=2048;
  lastu=values[0];
  lasti=values[1];
  int16_t _u = values[0] - caloffset[0];
  int16_t _i = values[1] - caloffset[1];
  lastuc=_u;
  lastic=_i;

  if (measurementState & MEASUREMENT_CALIBRATE) {
    if (!(measurementState & MEASUREMENT_DONE)) {
      calsum[0] += values[0];
      calsum[1] += values[1];
      __samples++;
      if ((micros() - measurementTime) >= CAL_TIMEOUT_US) {
        caloffset[0] = calsum[0] / __samples;
        caloffset[1] = calsum[1] / __samples;
        measurementState |= MEASUREMENT_DONE;
      }
    }
    return;
  }

  if (!(measurementState & MEASUREMENT_STARTED)) {
    return;
  }
  __samples++;

  if ((micros() - measurementTime) > TIMEOUT_US) {
    measurementTime = micros() - measurementTime;
    measurementState = MEASUREMENT_DONE; // and not valid
    return;
  }

  if (!(measurementState & MEASUREMENT_RUNNING)) {
    // wait until it crosses positive and go into measurement mode
    if (detectZC(_u)) {
      measurementTime = micros();
      measurementState |= MEASUREMENT_RUNNING;
      __samples = 0;
    }
  }

  if (measurementState & MEASUREMENT_RUNNING) {
    if (detectZC(_u)) {
      __cycles++;
    }

    if (__cycles >= CYCLES) {
      measurementState = MEASUREMENT_DONE | MEASUREMENT_VALID;
      measurementTime = micros() - measurementTime;
      return;
    }

    integrateMeasurement(_u, _i);
  }
}

void pfCalibrateStart()
{
  caloffset[0] = 0;
  caloffset[1] = 0;
  calsum[0] = 0;
  calsum[1] = 0;
  __samples = 0;
  measurementTime = micros();
  measurementState = MEASUREMENT_CALIBRATE;
}

uint8_t pfCalibrating()
{
  if ((measurementState & MEASUREMENT_CALIBRATE) &&
      !(measurementState & MEASUREMENT_DONE)) {
    uint32_t spent = micros() - measurementTime;
    uint8_t out = (100 * spent) / CAL_TIMEOUT_US;
    if (!out) {
      out = 1;
    }
    return out;
  } else {
    measurementState = 0;
    return 0;
  }
}

void pfStartMeasure()
{
  // return non zero if in trouble
  measurementTime = micros();
  resetMeasurement();
  measurementState = MEASUREMENT_STARTED; // clears other bits
}

uint8_t pfWaitMeasure()
{
  if (measurementState & MEASUREMENT_STARTED) {
    return 0; // still running
  }

  if (!(measurementState & MEASUREMENT_DONE)) {
    return 2; // not started
  }

  measurementState = 0;

  pfResults.frequency = 1000000.0 * (float) CYCLES / (float)measurementTime;

  pfResults.Upp = (float)(maxU - minU) * USCALE;
  pfResults.Ipp = (float)(maxI - minI) * ISCALE;

  pfResults.Urms = sqrtf((float)sumU2 / (float)__samples) * USCALE;
  pfResults.Irms = sqrtf((float)sumI2 / (float)__samples) * ISCALE;

  pfResults.powerW  = pfResults.Urms * pfResults.Irms;
  pfResults.powerVA = (float)sumUI / (float)__samples * USCALE * ISCALE;
  pfResults.powerFactor = pfResults.powerVA / pfResults.powerW;

  pfResults.samples = __samples;
  pfResults.time = measurementTime;

  if (!(measurementState & MEASUREMENT_VALID)) {
    return 3; // error
  } else {
    return 1;
  };
}


