#include "board.h"

void handleValuesFromADC(int16_t[4]);
void pfCalibrateStart();
uint8_t pfCalibrating();
void pfStartMeasure();
uint8_t pfWaitMeasure();

struct pfResults {
  float Upp, Ipp, Urms, Irms;
  float powerW, powerVA, powerFactor;
  float frequency;
  uint32_t samples,time;
};

extern struct pfResults pfResults;
