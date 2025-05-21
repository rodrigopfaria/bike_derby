// Horse Race Project Template - Arduino Mega

#include <Keypad.h>
#include <LiquidCrystal.h>
#include <Arduino.h>
#include <AccelStepper.h>

//------------------------------------
// LCD setup (16x2 LCD example)
const int rs = 51 , en = 53, d4 = 49, d5 = 47, d6 = 45, d7 = 43;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

//------------------------------------
// Keypad setup (4x4 matrix)
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}};
byte rowPins[ROWS] = {38, 40, 42, 44}; // adjust as needed
byte colPins[COLS] = {46, 48, 50, 52};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

//------------------------------------
// Stepper setup (8 motors, DRV8825 drivers)
#define NUM_CYCLISTS 8
AccelStepper steppers[NUM_CYCLISTS] = {
    AccelStepper(AccelStepper::DRIVER, 30, 31),
    AccelStepper(AccelStepper::DRIVER, 32, 33),
    AccelStepper(AccelStepper::DRIVER, 34, 35),
    AccelStepper(AccelStepper::DRIVER, 36, 37),
    AccelStepper(AccelStepper::DRIVER, 0, 1),
    AccelStepper(AccelStepper::DRIVER, 2, 3),
    AccelStepper(AccelStepper::DRIVER, 4, 5),
    AccelStepper(AccelStepper::DRIVER, 6, 7)};

//------------------------------------
// Constants
const int NUM_SCENARIOS = 6;
const int RACE_DISTANCE = 1000; // todo: calibrate total steps to finish line
const int BOOST_CHANCE = 10;    // % Chance of speed boost (double step)
const int STEP_DELAY = 100;     // Base delay between steps (ms)

// Scenario probabilities (must total to 100)
const uint8_t scenarioProbabilities[NUM_SCENARIOS][NUM_CYCLISTS] = {
    {30, 20, 15, 15, 10, 10}, // Scenario 0
    {10, 10, 30, 10, 30, 10}, // Scenario 1
    {25, 25, 10, 10, 15, 15}, // Scenario 2
    {5, 5, 20, 40, 20, 10},   // Scenario 3
    {16, 16, 16, 16, 16, 20}, // Scenario 4
    {10, 25, 15, 25, 15, 10}  // Scenario 5
};

//------------------------------------
// CHECK THIS CODE

// // State
int currentScenario = 0;
int cyclistPositions[NUM_CYCLISTS] = {0};
bool raceFinished = false;

//------------------------------------
// FSM States
enum State
{
  STATE_IDLE,
  STATE_SETUP,
  STATE_READY,
  STATE_RACE,
  STATE_RESULT
};
State currentState = STATE_IDLE;

int winnerHorse = -1;            // Track winner index
// const long RACE_DISTANCE = 2000; // Target position to finish race
int favoredHorse = -1;           // Track user-selected favored horse

//------------------------------------
// Function declarations
void handleIdle();
void handleSetup();
void handleReady();
void handleRace();
void handleResult();
void updateDisplay(const String &line1, const String &line2);
void moveSteppersRandomly();
int checkWinner();
void resetRace();
/*
  This function determines which cyclist will advance in the current iteration based on the scenario's probabilities
  inputs: scenarioIndex - selection made by game master
  output: cyclist index - refrence to which stepper will move
*/
int getWeightedRandomCyclist(uint8_t scenarioIndex);

//------------------------------------
void setup()
{
  Serial.begin(9600);

  lcd.begin(16, 2);
  updateDisplay("Horse Race", "Press A to Start");

  // Initialize steppers
  for (int i = 0; i < NUM_CYCLISTS; i++)
  {
    steppers[i].setMaxSpeed(1000);
    steppers[i].setAcceleration(500);
    steppers[i].setSpeed(200);
  }

  // Select scenario manually for now (will be from buttons later)
  currentScenario = 0;        // Selectable between 0–5
  randomSeed(analogRead(A0)); // Ensure randomness
}

//------------------------------------
void loop()
{

  // WE NEED TO CREATE A FSM DIAGRAM IN A PAPER ACCORDING TO HOW WE WANT. THIS ONE WORKS AS AN INITIAL SKELETON.

  switch (currentState)
  {
  case STATE_IDLE:
    handleIdle();
    break;
  case STATE_SETUP:
    handleSetup();
    break;
  case STATE_READY:
    handleReady();
    break;
  case STATE_RACE:
    handleRace();
    break;
  case STATE_RESULT:
    handleResult();
    break;
  }

  // Always update steppers
  for (int i = 0; i < NUM_CYCLISTS; i++)
  {
    steppers[i].run();
  }

  Serial.println(currentState);
  

  /*
    BREAK DOWN THE FOLLOWING LOGIC AND PUT INSIDE THE HANDLE FUNCTIONS.
  */
  // if (raceFinished)
  //   return;

  // int cyclist = getWeightedRandomCyclist(currentScenario);

  // int steps = 1;

  // // Speed boost chance
  // if (random(0, 100) < BOOST_CHANCE)
  // {
  //   steps = 2;
  // }

  // // Add slight randomness to step delay (±20 ms)
  // int delayJitter = random(-20, 20);

  // // Move cyclists
  // for (int i = 0; i < steps; i++)
  // {
  //   if (cyclistPositions[cyclist] < RACE_DISTANCE)
  //   {
  //     steppers[cyclist].move(1);
  //     steppers[cyclist].run();
  //     cyclistPositions[cyclist]++;
  //   }
  // }

  // // Check if any cyclist has won
  // for (int i = 0; i < NUM_CYCLISTS; i++)
  // {
  //   if (cyclistPositions[i] >= RACE_DISTANCE)
  //   {
  //     Serial.print("Cyclist ");
  //     Serial.print(i + 1);
  //     Serial.println(" wins!");
  //     raceFinished = true;
  //     break;
  //   }
  // }

  // delay(STEP_DELAY + delayJitter);
}




//------------------------------------
// Function Definitions

// Weighted random cyclist
int getWeightedRandomCyclist(uint8_t scenarioIndex)
{
  int r = random(0, 100);
  int cumulative = 0;
  // Goes through each cyclist's probabilities and checks if it beats a random chance value
  for (int i = 0; i < NUM_CYCLISTS; i++)
  {
    cumulative += scenarioProbabilities[scenarioIndex][i];
    // If the probability is greater, than that cyclist advances and the others don't
    if (r < cumulative)
    {
      return i;
    }
    // Else, if no cyclist's prob beats the random value, nobody advances
  }
  return NUM_CYCLISTS - 1; // fallback
}


//------------------------------------
void handleIdle() {
  char key = keypad.getKey();
  if (key == 'A') {
    updateDisplay("Setup Mode", "Select horse 1-8");
    currentState = STATE_SETUP;
  }
}

void handleSetup() {
  char key = keypad.getKey();
  if (key >= '1' && key <= '8') {
    favoredHorse = key - '1';
    updateDisplay("Horse Selected", "Horse " + String(favoredHorse + 1));
  }
  if (key == 'B') { // 'Ready' button
    if (favoredHorse >= 0) {
      updateDisplay("Ready Mode", "Press D to Go");
      currentState = STATE_READY;
    } else {
      updateDisplay("Select horse", "Then press B");
    }
  }
}

void handleReady() {
  char key = keypad.getKey();
  if (key == 'D') { // Start race
    updateDisplay("Race Starting", "Go!");
    currentState = STATE_RACE;
  }
}

void handleRace() {
  moveSteppersRandomly();

  int winner = checkWinner();
  if (winner >= 0) {
    winnerHorse = winner;
    String resultText = "Winner: Horse " + String(winner + 1);
    if (winner == favoredHorse) {
      resultText += " (Yours!)";
    }
    updateDisplay("Race Finished", resultText);
    currentState = STATE_RESULT;
  }
}

void handleResult() {
  char key = keypad.getKey();
  if (key == 'C') { // Reset
    resetRace();
    updateDisplay("Resetting", "Press A to start");
    currentState = STATE_IDLE;
  }
}

//------------------------------------
void updateDisplay(const String &line1, const String &line2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

void moveSteppersRandomly() {
  for (int i = 0; i < NUM_CYCLISTS; i++) {
    if (!steppers[i].isRunning()) {
      long boost = (i == favoredHorse) ? random(200, 400) : random(100, 300);
      long target = steppers[i].currentPosition() + boost;
      steppers[i].moveTo(target);
    }
  }
}

int checkWinner() {
  for (int i = 0; i < NUM_CYCLISTS; i++) {
    if (steppers[i].currentPosition() >= RACE_DISTANCE) {
      return i; // Return index of winning horse
    }
  }
  return -1; // No winner yet
}

void resetRace() {
  for (int i = 0; i < NUM_CYCLISTS; i++) {
    steppers[i].stop();
    steppers[i].setCurrentPosition(0);
  }
  winnerHorse = -1;
  favoredHorse = -1;
}
