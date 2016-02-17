#include "board.h"

void handleValuesFromADC(int16_t[4]);
void pfStartMeasure();
uint8_t pfWaitMeasure();

struct pfResults {
  float Upp, Ipp, Urms, Irms;
  float powerW, powerVA, powerFactor;
  float frequency;
};
extern struct pfResults pfResults;

