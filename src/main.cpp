// Horse Race Project Template - Arduino Mega

#include <Keypad.h>
#include <LiquidCrystal.h>
#include <Arduino.h>
#include <AccelStepper.h>

//------------------------------------
// secondary stepper motor - dummyStepper
#define IN1_dummyStepper 7
#define IN2_dummyStepper 6
#define IN3_dummyStepper 5
#define IN4_dummyStepper 4

//------------------------------------
// LCD setup (16x2 LCD example)
const int rs = 51, en = 53, d4 = 49, d5 = 47, d6 = 45, d7 = 43;
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
// Stepper setup (6 motors, DRV8825 drivers)
// #define NUM_CYCLISTS 4
// AccelStepper steppers[NUM_CYCLISTS] = {
// 	AccelStepper(AccelStepper::DRIVER, 30, 31),
// 	AccelStepper(AccelStepper::DRIVER, 32, 33),
// 	AccelStepper(AccelStepper::DRIVER, 34, 35),
// 	AccelStepper(AccelStepper::DRIVER, 36, 37)};

#define NUM_CYCLISTS 1
AccelStepper steppers[NUM_CYCLISTS] = {
	AccelStepper(AccelStepper::HALF4WIRE, IN1_dummyStepper, IN3_dummyStepper, IN2_dummyStepper, IN4_dummyStepper)};

// //------------------------------------
// // Limit switch setup
// const int limitSwitchPins[NUM_CYCLISTS] = {22, 23, 24, 25};

//------------------------------------
// Constants
const int NUM_SCENARIOS = 6;
const int RACE_DISTANCE = 1000;			 // todo: calibrate total steps to finish line
const int BOOST_CHANCE = 10;			 // % Chance of speed boost (double step)
const int STEP_DELAY = 100;				 // Base delay between steps (ms)
const unsigned long debounceDelay = 200; // milliseconds

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
const unsigned long raceTimeout = 60000;  // 1 minute
const unsigned long resetTimeout = 30000; // 30 seconds

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

//------------------------------------
// Function declarations
void handleIdle();
void handleSetup();
void handleReady();
void handleRace();
void handleResult();
void updateDisplay(const String &line1, const String &line2);
void checkWinner();
void resetRace();
void initializeSteppers();
char getDebouncedKey();
/*
  This function determines which cyclist will advance in the current iteration based on the scenario's probabilities
  input: scenarioIndex - selection made by game master
  output: cyclist index - refrence to which stepper will move
*/
int getWeightedRandomCyclist(uint8_t scenarioIndex);

//------------------------------------
void setup()
{
	Serial.begin(9600);

	// // Set limit switch pins as input with pull-up resistors
	// for (int i = 0; i < NUM_CYCLISTS; i++)
	// {
	// 	pinMode(limitSwitchPins[i], INPUT_PULLUP); // Assuming active LOW
	// }

	lcd.begin(16, 2);
	updateDisplay("Horse Race", "Press A to Start");

	initializeSteppers();

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
		// cumulative += scenarioProbabilities[scenarioIndex][i];
		// If the probability is greater, than that cyclist advances and the others don't
		if (r < cumulative)
		{
			return i;
		}
	}
	// If no cyclist was selected, return -1
	return -1;
}

// Handle Functions
//------------------------------------
void handleIdle()
{
	// If a key is pressed, check if it's 'A' to enter setup mode
	char key = getDebouncedKey();
	if (key == 'A')
	{
		// Get random seed based on current time at button press
		// This ensures that the random numbers generated are different each time
		randomSeed(millis());
		updateDisplay("Setup Mode", "Select scenario 1-6");
		currentState = STATE_SETUP;
	}
}

void handleSetup()
{
	char key = getDebouncedKey();
	if (key >= '1' && key <= '6')
	{
		// convert char to int (1-6) and set currentScenario
		// subtract 1 to get 0-5 range for array access
		// currentScenario = key - '0' - 1; // '1' to '6' -> 0 to 5
		currentScenario = key - '1';

		// update display with selected scenario
		updateDisplay("Press B to ready", "Scenario " + String(currentScenario + 1));
	}

	// 'Ready' button
	if (key == 'B')
	{
		// // Check if a scenario is selected
		// if (currentScenario >= 0)
		// {
			// Reset positions and raceFinished flag
			raceFinished = false;
			for (int i = 0; i < NUM_CYCLISTS; i++)
			{
				// Stop all steppers
				steppers[i].stop();
				// Reset stepper settings
				initializeSteppers();
				// Reset positions
				cyclistPositions[i] = 0;
				steppers[i].setCurrentPosition(0);
			}
			updateDisplay("Ready!!", "Press D to Start");
			currentState = STATE_READY;
		// }
		// else
		// {
		// 	updateDisplay("Select horse", "Then press B");
		// }
	}
}

// todo: add check for correct position of the steppers
void handleReady()
{
	char key = getDebouncedKey();
	// Start race
	if (key == 'D')
	{
		updateDisplay("Race Starting", "Press C to Abort");
		currentState = STATE_RACE;
	}
}

void handleRace()
{
	// Race timeout check
	if (raceStartTime == 0)
		raceStartTime = millis();
	if (millis() - raceStartTime > raceTimeout)
	{
		updateDisplay("Race Timeout", "Check motors!");
		currentState = STATE_RESULT;
		return;
	}

	// Keyboard polling check for abort key
	// todo: check if thats enough polling
	char key = getDebouncedKey();
	if (key == 'C') // 'RESET' key
	{
		resetRace();
		updateDisplay("Resetting", "Press A to start");
		currentState = STATE_IDLE;
		return;
	}

	steppers[0].moveTo(500000); 
	steppers[0].run();

	// // Only proceed if enough time has passed since the last step
	// unsigned long now = millis();
	// if (now - lastRaceStepTime < currentStepDelay)
	// {
	// 	// Still waiting, just return and let loop() call us again
	// 	return;
	// }
	// lastRaceStepTime = now;
	// // Randomize delay to simulate different speeds
	// currentStepDelay = STEP_DELAY + random(-20, 20);

	// // Check if any cyclist has reached the finish line
	// // If so, set raceFinished to true and display the winner
	// checkWinner();

	// if (raceFinished)
	// {
	// 	currentState = STATE_RESULT;
	// 	raceStartTime = 0; // Reset for next race
	// 	return;
	// }

	// // Get a random cyclist based on the current scenario
	// // This function will return a cyclist index based on the scenario probabilities
	// // This cyclist will be the one to move the most in this iteration
	// int luckyCyclist = getWeightedRandomCyclist(currentScenario);

	// // Move cyclists
	// for (int i = 0; i < NUM_CYCLISTS; i++)
	// {
	// 	// Check if the lucky cyclist is the one to move
	// 	// If so, move them the most (3 steps) and check if they reached the finish line
	// 	// Else, move the other cyclists a minimum amount (1 step)
	// 	if (i == luckyCyclist)
	// 	{
	// 		int moveSteps = (random(0, 100) < BOOST_CHANCE) ? 15 : 5; // 3x boost or normal
	// 		if (cyclistPositions[luckyCyclist] < RACE_DISTANCE)
	// 		{
	// 			if (steppers[luckyCyclist].distanceToGo() == 0)
	// 			{
	// 				steppers[luckyCyclist].move(moveSteps);
	// 			}
	// 			steppers[luckyCyclist].run();
	// 			cyclistPositions[luckyCyclist] = steppers[luckyCyclist].currentPosition();
	// 		}
	// 		// Check if the lucky cyclist has reached the finish line
	// 		else
	// 		{
	// 			// Stop the stepper if it reaches the finish line
	// 			steppers[luckyCyclist].stop();
	// 			checkWinner();
	// 			if (raceFinished)
	// 			{
	// 				currentState = STATE_RESULT;
	// 				raceStartTime = 0; // Reset for next race
	// 				return;
	// 			}
	// 		}
	// 	}
	// 	// If not the lucky cyclist, check if the others are still in the race
	// 	// If they are, move them the minimum amount (1 step)
	// 	else
	// 	{
	// 		if (cyclistPositions[i] < RACE_DISTANCE)
	// 		{
	// 			// Move other cyclists minimum amount (5 step)
	// 			if (steppers[i].distanceToGo() == 0)
	// 			{
	// 				steppers[i].move(5);
	// 			}
	// 			steppers[i].run();
	// 			cyclistPositions[i] = steppers[i].currentPosition();
	// 		}
	// 		// Check if any of the other cyclists have reached the finish line
	// 		// If so, stop them and check for the winner
	// 		else
	// 		{
	// 			// Stop the stepper if it reaches the finish line
	// 			steppers[i].stop();
	// 			checkWinner();
	// 			if (raceFinished)
	// 			{
	// 				currentState = STATE_RESULT;
	// 				raceStartTime = 0; // Reset for next race
	// 				return;
	// 			}
	// 		}
	// 	}
	// }
}

void handleResult()
{
	char key = getDebouncedKey();
	// Check if reset key was pressed
	if (key == 'C')
	{
		resetRace();
		updateDisplay("Resetting", "Press A to start");
		currentState = STATE_IDLE;
	}
}

// Other helper functions
//------------------------------------
void updateDisplay(const String &line1, const String &line2)
{
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print(line1);
	lcd.setCursor(0, 1);
	lcd.print(line2);
}

void checkWinner()
{
	for (int i = 0; i < NUM_CYCLISTS; i++)
	{
		if (steppers[i].currentPosition() >= RACE_DISTANCE)
		{
			raceFinished = true;
			String resultText = "Winner: Horse " + String(i + 1);
			updateDisplay("Race Finished", resultText);
			return;
		}
	}
}

void resetRace()
{
	// Move each bike backwards until its limit switch is triggered
	for (int i = 0; i < NUM_CYCLISTS; i++)
	{
		steppers[i].setMaxSpeed(1000);
		steppers[i].setAcceleration(500);
		steppers[i].setSpeed(-300);			// Negative speed to rewind
		steppers[i].moveTo(-RACE_DISTANCE); // Move back enough distance
	}

	unsigned long resetStart = millis();

	bool allAtStart = false;
	while (!allAtStart)
	{

		// Timeout check
		if (millis() - resetStart > resetTimeout)
		{
			updateDisplay("Reset Error", "Check switches!");
			break; // Exit loop if timeout
		}

		allAtStart = true;
		for (int i = 0; i < NUM_CYCLISTS; i++)
		{
			// if (digitalRead(limitSwitchPins[i]) == HIGH)
			// { // Not at start
			// 	steppers[i].run();
			// 	allAtStart = false;
			// }
			// else
			// {
			// 	steppers[i].stop();
			// 	steppers[i].setCurrentPosition(0); // Reset position
			// }
		}
		// Optional: add a small delay to avoid busy-waiting
		delay(2);
	}

	// Reset positions and state
	for (int i = 0; i < NUM_CYCLISTS; i++)
	{
		cyclistPositions[i] = 0;
		steppers[i].setCurrentPosition(0);
	}
	raceFinished = false;
	currentScenario = -1;
}

void initializeSteppers()
{
	for (int i = 0; i < NUM_CYCLISTS; i++)
	{
		steppers[i].setMaxSpeed(1000);
		steppers[i].setAcceleration(500);
		steppers[i].setSpeed(200);
	}
}

// Debounce function for keypad input
char getDebouncedKey()
{
	char key = keypad.getKey();
	if (key)
	{
		unsigned long now = millis();
		if (now - lastKeypadTime > debounceDelay)
		{
			lastKeypadTime = now;
			return key;
		}
	}
	return NO_KEY;
}