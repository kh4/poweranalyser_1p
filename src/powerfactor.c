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

#define USCALE (0.37)
#define ISCALE (0.01)

#define CYCLES 10
#define CAL_CYCLES 500

#define TIMEOUT_US 3000000
#define CAL_TIMEOUT_US 5000000

volatile int16_t  minU, minI, maxU, maxI;
volatile int64_t  sumU2, sumI2;
volatile int64_t  sumUI;
volatile uint32_t measurementTime;
volatile int32_t samples;
volatile int16_t cycles;

int16_t caloffset[2] = {0,0};
int32_t calsum[2];

#define MEASUREMENT_STARTED 1
#define MEASUREMENT_RUNNING 2
#define MEASUREMENT_VALID   4
#define MEASUREMENT_ERROR   8
#define MEASUREMENT_CALIBRATE 128

volatile uint8_t measurementState;
uint32_t __start;

struct pfResults pfResults;

#define ZC_THRESHOLD -200


void resetMeasurement()
{
  maxU = minU = maxI = minI = 0;
  sumU2 = sumI2 = sumUI = 0;
  samples = cycles = 0;
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
  sumU2 += (int64_t)u * (int64_t)u;
  sumI2 += (int64_t)i * (int64_t)i;
  sumUI = (int64_t)u * (int64_t)i;

  samples++;

}

int detectZC(int16_t u)
{
  static bool zcstate = 0;
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
      calsum[0] += values[0];
      calsum[1] += values[1];
      samples++;
      if ((micros() - measurementTime) >= CAL_TIMEOUT_US) {
        caloffset[0] = calsum[0] / samples;
        caloffset[1] = calsum[1] / samples;
        measurementState = 0;
      }
    return;
  }

  if (!(measurementState & MEASUREMENT_STARTED)) {
    return;
  }

  //  if ((micros() - measurementTime) > TIMEOUT_US) {
  //    measurementTime = micros() - measurementTime;
  //    measurementState = MEASUREMENT_ERROR;
  //   return;
  //}

  if (!(measurementState & MEASUREMENT_RUNNING)) {
    // wait until it crosses positive and go into measurement mode
    if (detectZC(_u)) {
      measurementTime = micros();
      measurementState |= MEASUREMENT_RUNNING;
    }
  }

  if (measurementState & MEASUREMENT_RUNNING) {
    if (detectZC(_u)) {
      cycles++;
    }

    if (cycles >= CYCLES) {
      measurementState = MEASUREMENT_VALID;
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
  samples = 0;
  measurementTime = micros();
  measurementState = MEASUREMENT_CALIBRATE;
}

uint8_t pfCalibrating()
{
  if (measurementState & MEASUREMENT_CALIBRATE) {
    uint32_t spent = micros() - measurementTime;
    uint8_t out = (100 * spent) / CAL_TIMEOUT_US;
    if (!out) {
      out = 1;
    }
    return out;
  } else {
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

  if (measurementState & MEASUREMENT_ERROR) {
    return 3; // error from the interrupt routine, likely timeout
  }

  if (measurementState & MEASUREMENT_VALID) {
    pfResults.frequency = 1000000.0 * (float) CYCLES / (float)measurementTime;

    pfResults.Upp = (float)(maxU - minU) * USCALE * 0.5;
    pfResults.Ipp = (float)(maxI - minI) * ISCALE * 0.5 ;

    pfResults.Urms = sqrtf((float)sumU2 / (float)samples) * USCALE;
    pfResults.Irms = sqrtf((float)sumI2 / (float)samples) * ISCALE;

    pfResults.powerW  = pfResults.Urms * pfResults.Irms;
    pfResults.powerVA = (float)sumUI / (float)samples * USCALE * ISCALE;
    pfResults.powerFactor = pfResults.powerVA / pfResults.powerW;

    pfResults.samples = samples;
    pfResults.time = measurementTime;
    measurementState = 0;
    return 1;
  }

  return 2; // other error;
}


