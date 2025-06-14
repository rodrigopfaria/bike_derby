#include <Arduino.h>
#include <Keypad.h>
#include <LiquidCrystal.h>
#include <AccelStepper.h>

//------------------------------------
// IMPORTANT CONFIGURATION FILE
#define NUM_CYCLISTS 1

#define KB_ROWS 4
#define KB_COLS 4

//------------------------------------
// secondary stepper motor - dummyStepper
#define IN1_dummyStepper 7
#define IN2_dummyStepper 6
#define IN3_dummyStepper 5
#define IN4_dummyStepper 4

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

extern State currentState;

// //------------------------------------
// // Limit switch setup
extern int limitSwitchPinsStart[NUM_CYCLISTS]; // adjust as needed
extern int limitSwitchPinsEnd[NUM_CYCLISTS];

//------------------------------------
// Constants
extern const int NUM_SCENARIOS;
extern const int RACE_DISTANCE;           // todo: calibrate total steps to finish line
extern const int BOOST_CHANCE;              // % Chance of speed boost (double step)
extern const int STEP_DELAY;               // Base delay between steps (ms)
extern const unsigned long debounceDelay;  // milliseconds
extern const unsigned long raceTimeout;  // 1 minute
extern const unsigned long resetTimeout; // 30 seconds

//------------------------------------
// LCD setup (16x2 LCD example)
extern const int disp_rs, disp_en, disp_d4, disp_d5, disp_d6, disp_d7;

//------------------------------------
// Keypad setup (4x4 matrix)
extern char keys[KB_ROWS][KB_COLS];
extern byte rowPins[KB_ROWS]; // adjust as needed
extern byte colPins[KB_COLS];

//------------------------------------
// Externalizing Global variables
extern LiquidCrystal lcd;
extern Keypad keypad;

extern AccelStepper steppers[NUM_CYCLISTS];

extern int currentScenario;
extern int cyclistPositions[NUM_CYCLISTS];
extern bool raceFinished;
extern unsigned long lastKeypadTime;   // used for debouncing
extern unsigned long lastRaceStepTime; // used for delay between steps
extern int currentStepDelay;           // current step delay, can be adjusted based on scenario
extern unsigned long raceStartTime;    // used to track race timeout
