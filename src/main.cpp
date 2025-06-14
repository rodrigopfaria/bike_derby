// Horse Race Project Template - Arduino Mega

#include "Config.h"
#include "FSM_handlers.h"
#include "helper_functions.h"

//------------------------------------
// Constants
const int NUM_SCENARIOS = 6;
const int RACE_DISTANCE = 1000;           // todo: calibrate total steps to finish line
const int BOOST_CHANCE = 10;              // % Chance of speed boost (double step)
const int STEP_DELAY = 100;               // Base delay between steps (ms)
const unsigned long debounceDelay = 200;  // milliseconds
const unsigned long raceTimeout = 60000;  // 1 minute
const unsigned long resetTimeout = 30000; // 30 seconds

// LCD setup (16x2 LCD example)
const int disp_rs = 51, disp_en = 53, disp_d4 = 49, disp_d5 = 47, disp_d6 = 45, disp_d7 = 43;

// Keypad setup (4x4 matrix)
char keys[KB_ROWS][KB_COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}};
byte rowPins[KB_ROWS] = {38, 40, 42, 44}; // adjust as needed
byte colPins[KB_COLS] = {46, 48, 50, 52};

// // Limit switch setup
int limitSwitchPinsStart[NUM_CYCLISTS] = {41}; // adjust as needed
int limitSwitchPinsEnd[NUM_CYCLISTS] = {39};


LiquidCrystal lcd(disp_rs, disp_en, disp_d4, disp_d5, disp_d6, disp_d7);
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, KB_ROWS, KB_COLS);

// Stepper setup (6 motors, DRV8825 drivers)
// // define number of cyclist in config.h file
// AccelStepper steppers[NUM_CYCLISTS] = {
// 	AccelStepper(AccelStepper::DRIVER, 30, 31),
// 	AccelStepper(AccelStepper::DRIVER, 32, 33),
// 	AccelStepper(AccelStepper::DRIVER, 34, 35),
// 	AccelStepper(AccelStepper::DRIVER, 36, 37)};

AccelStepper steppers[NUM_CYCLISTS] = {
	AccelStepper(AccelStepper::HALF4WIRE, IN1_dummyStepper, IN3_dummyStepper, IN2_dummyStepper, IN4_dummyStepper)};

// // Scenario probabilities (must total to 100)
// const uint8_t scenarioProbabilities[NUM_SCENARIOS][NUM_CYCLISTS] = {
// 	{30, 20, 15, 15}, // Scenario 0
// 	{10, 10, 30, 10}, // Scenario 1
// 	{25, 25, 10, 10}, // Scenario 2
// 	{5, 5, 20, 40},	  // Scenario 3
// 	{16, 16, 16, 16}, // Scenario 4
// 	{10, 25, 15, 25}  // Scenario 5
// };

// Global variables
int currentScenario = 0;
int cyclistPositions[NUM_CYCLISTS] = {0};
bool raceFinished = false;
unsigned long lastKeypadTime = 0;		  // used for debouncing
unsigned long lastRaceStepTime = 0;		  // used for delay between steps
int currentStepDelay = STEP_DELAY;		  // current step delay, can be adjusted based on scenario
unsigned long raceStartTime = 0;		  // used to track race timeout


State currentState = STATE_IDLE;





//------------------------------------
void setup()
{
	Serial.begin(9600);

	// // Set limit switch pins as input with pull-up resistors
	for (int i = 0; i < NUM_CYCLISTS; i++)
	{
		pinMode(limitSwitchPinsStart[i], INPUT_PULLUP);
		pinMode(limitSwitchPinsEnd[i], INPUT_PULLUP);
	}

	lcd.begin(16, 2);

	initializeSteppers();
	updateDisplay("CALIBRATING", "....");
	calibrateSteppers();

	updateDisplay("Horse Race", "Press A to Start");
	currentScenario = -1; // Selectable between 0–5
}



//------------------------------------
void loop()
{
	switch (currentState)
	{
	// Default state is IDLE, waiting for user input
	case STATE_IDLE:
		handleIdle();
		break;
	// Setup state, waiting for user to select a scenario
	case STATE_SETUP:
		handleSetup();
		break;
	// Ready state, waiting for user to press 'D' to start the race
	case STATE_READY:
		handleReady();
		break;
	// Move steppers and check for winner
	case STATE_RACE:
		handleRace();
		break;
	// Display winner and reset once button is pressed
	case STATE_RESULT:
		handleResult();
		break;
	// If an unknown state is reached, reset to IDLE
	default:
		updateDisplay("Unkown state", "Default state");
		handleIdle();
		break;
	}

	Serial.println(currentState);
}
