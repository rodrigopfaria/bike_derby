#include <Arduino.h>
#include <AccelStepper.h>

// Constants
const int NUM_CYCLISTS = 6;
const int NUM_SCENARIOS = 6;
const int RACE_DISTANCE = 1000;  //todo: calibrate total steps to finish line
const int BOOST_CHANCE = 10;     // % Chance of speed boost (double step)
const int STEP_DELAY = 100;      // Base delay between steps (ms)

// Scenario probabilities (must total to 100)
const uint8_t scenarioProbabilities[NUM_SCENARIOS][NUM_CYCLISTS] = {
  {30, 20, 15, 15, 10, 10},  // Scenario 0
  {10, 10, 30, 10, 30, 10},  // Scenario 1
  {25, 25, 10, 10, 15, 15},  // Scenario 2
  {5, 5, 20, 40, 20, 10},    // Scenario 3
  {16, 16, 16, 16, 16, 20},  // Scenario 4
  {10, 25, 15, 25, 15, 10}   // Scenario 5
};

// Pin definitions for stepper drivers
const int stepPins[NUM_CYCLISTS] = {2, 4, 6, 8, 10, 12};
const int dirPins[NUM_CYCLISTS]  = {3, 5, 7, 9, 11, 13}; 

// Stepper objects
AccelStepper steppers[NUM_CYCLISTS];

// State
int currentScenario = 0;
int cyclistPositions[NUM_CYCLISTS] = {0};
bool raceFinished = false;


void setup() {
  Serial.begin(9600);
  
  // Initialize steppers
  for (int i = 0; i < NUM_CYCLISTS; i++) {
    steppers[i] = AccelStepper(AccelStepper::DRIVER, stepPins[i], dirPins[i]);
    steppers[i].setMaxSpeed(1000);
    steppers[i].setAcceleration(500);
    steppers[i].setSpeed(200);
  }

  // Select scenario manually for now (will be from buttons later)
  currentScenario = 0;  // Selectable between 0–5
  randomSeed(analogRead(A0)); // Ensure randomness
}

// put function declarations here:
/*
  This function determines which cyclist will advance in the current iteration based on the scenario's probabilities
  inputs: scenarioIndex - selection made by game master
  output: cyclist index - refrence to which stepper will move
*/
int getWeightedRandomCyclist(uint8_t scenarioIndex);

void loop() {
  // put your main code here, to run repeatedly:
  if (raceFinished) return;

  int cyclist = getWeightedRandomCyclist(currentScenario);

  int steps = 1;

  // Speed boost chance
  if (random(0, 100) < BOOST_CHANCE) {
    steps = 2;
  }

  // Add slight randomness to step delay (±20 ms)
  int delayJitter = random(-20, 20);

  // Move cyclists
  for (int i = 0; i < steps; i++) {
    if (cyclistPositions[cyclist] < RACE_DISTANCE) {
      steppers[cyclist].move(1);
      steppers[cyclist].run();
      cyclistPositions[cyclist]++;
    }
  }

  // Check if any cyclist has won
  for (int i = 0; i < NUM_CYCLISTS; i++) {
    if (cyclistPositions[i] >= RACE_DISTANCE) {
      Serial.print("Cyclist ");
      Serial.print(i + 1);
      Serial.println(" wins!");
      raceFinished = true;
      break;
    }
  }

  delay(STEP_DELAY + delayJitter);
}

// put function definitions here:
// Weighted random cyclist
int getWeightedRandomCyclist(uint8_t scenarioIndex) {
  int r = random(0, 100);
  int cumulative = 0;
  // Goes through each cyclist's probabilities and checks if it beats a random chance value
  for (int i = 0; i < NUM_CYCLISTS; i++) {
    cumulative += scenarioProbabilities[scenarioIndex][i];
    // If the probability is greater, than that cyclist advances and the others don't
    if (r < cumulative) {
      return i;
    }
    // Else, if no cyclist's prob beats the random value, nobody advances
  }
  return NUM_CYCLISTS - 1;  // fallback
}