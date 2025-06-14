
#include <Arduino.h>


void updateDisplay(const String &line1, const String &line2);
void checkWinner();
void resetRace();
void initializeSteppers();
char getDebouncedKey();
void calibrateSteppers();

/*
  This function determines which cyclist will advance in the current iteration based on the scenario's probabilities
  input: scenarioIndex - selection made by game master
  output: cyclist index - refrence to which stepper will move
*/
int getWeightedRandomCyclist(uint8_t scenarioIndex);