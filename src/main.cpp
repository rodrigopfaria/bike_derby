// Horse Race Project Template - Arduino Mega

#include <Keypad.h>
#include <LiquidCrystal.h>
#include <Arduino.h>
#include <AccelStepper.h>

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
#define NUM_CYCLISTS 4
AccelStepper steppers[NUM_CYCLISTS] = {
	AccelStepper(AccelStepper::DRIVER, 30, 31),
	AccelStepper(AccelStepper::DRIVER, 32, 33),
	AccelStepper(AccelStepper::DRIVER, 34, 35),
	AccelStepper(AccelStepper::DRIVER, 36, 37)
};

//------------------------------------
// Limit switch setup
const int limitSwitchPins[NUM_CYCLISTS] = {22, 23, 24, 25};

//------------------------------------
// Constants
const int NUM_SCENARIOS = 6;
const int RACE_DISTANCE = 1000; // todo: calibrate total steps to finish line
const int BOOST_CHANCE = 10;	// % Chance of speed boost (double step)
const int STEP_DELAY = 100;		// Base delay between steps (ms)
const unsigned long debounceDelay = 200; // milliseconds

// Scenario probabilities (must total to 100)
const uint8_t scenarioProbabilities[NUM_SCENARIOS][NUM_CYCLISTS] = {
	{30, 20, 15, 15}, // Scenario 0
	{10, 10, 30, 10}, // Scenario 1
	{25, 25, 10, 10}, // Scenario 2
	{5, 5, 20, 40},	  // Scenario 3
	{16, 16, 16, 16}, // Scenario 4
	{10, 25, 15, 25}  // Scenario 5
};

// Global variables
int currentScenario = 0;
int cyclistPositions[NUM_CYCLISTS] = {0};
bool raceFinished = false;
unsigned long lastKeypadTime = 0;	// used for debouncing

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

	// Set limit switch pins as input with pull-up resistors
    for (int i = 0; i < NUM_CYCLISTS; i++) {
        pinMode(limitSwitchPins[i], INPUT_PULLUP); // Assuming active LOW
    }

	lcd.begin(16, 2);
	updateDisplay("Horse Race", "Press A to Start");

	initializeSteppers();

	currentScenario = -1;		// Selectable between 0–5
	randomSeed(analogRead(A0)); // Ensure randomness
}

//------------------------------------
void loop()
{

	/**
	 * todo: add a state between RACE and RESULT where the remaining riders are moved to the finish line
   * (or wait for them to finish in the RACE state before moving to RESULT)
	 */

	switch (currentState)
	{
	case STATE_IDLE:	// Default state is IDLE, waiting for user input
		handleIdle();
		break;
	case STATE_SETUP:	// Setup state, waiting for user to select a scenario
		handleSetup();		
		break;
	case STATE_READY:	// Ready state, waiting for user to press 'D' to start the race
		handleReady();
		break;
	case STATE_RACE:	// Move steppers and check for winner
		handleRace();
		break;
	case STATE_RESULT:	// Display winner and reset once button is pressed
		handleResult();
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
		cumulative += scenarioProbabilities[scenarioIndex][i];
		// If the probability is greater, than that cyclist advances and the others don't
		if (r < cumulative)
		{
			return i;
		}
	}
	// If no cyclist was selected, return -1
	return - 1;
}


// Handle Functions
//------------------------------------
void handleIdle()
{
	// If a key is pressed, check if it's 'A' to enter setup mode
	char key = getDebouncedKey();
	if (key == 'A')
	{
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
		updateDisplay("Scenario Selected", "SCN " + String(currentScenario + 1));
	}

	// 'Ready' button
	if (key == 'B') 
	{ 
		// Check if a scenario is selected
		if (currentScenario >= 0)
		{
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
			updateDisplay("Ready Mode", "Press D to Go");
			currentState = STATE_READY;
		}
		else
		{
			updateDisplay("Select horse", "Then press B");
		}
	}
}

// todo: add check for correct position of the steppers
void handleReady()
{
	char key = getDebouncedKey();
	// Start race
	if (key == 'D')
	{ 
		updateDisplay("Race Starting", "Go!");
		currentState = STATE_RACE;
	}
}

void handleRace()
{

  // Keyboard polling check for abort key
  // todo: check if thats enough polling
  char key = keypad.getKey();
  if (key == 'C') // 'RESET' key
  { 
    resetRace();
    updateDisplay("Resetting", "Press A to start");
    currentState = STATE_IDLE;
    return;
  }

	int steps = 1;	// Default step size

	// Check if any cyclist has reached the finish line
	// If so, set raceFinished to true and display the winner
	checkWinner();

	if (raceFinished) 
	{
		currentState = STATE_RESULT;
		return;
	}
		
	// Get a random cyclist based on the current scenario
	// This function will return a cyclist index based on the scenario probabilities
	// This cyclist will be the one to move the most in this iteration
	int luckyCyclist = getWeightedRandomCyclist(currentScenario);
	

	// Speed boost chance
	if (random(0, 100) < BOOST_CHANCE)
	{
		steps = 3;	// Triple step for lucky cyclist
	}

	// Add a different, slight randomness to step delay on every itereation (±20 ms)
	int delayJitter = random(-20, 20);

	// Move cyclists
	for (int j = 0; j < NUM_CYCLISTS; j++)
	{
		// Check if the lucky cyclist is the one to move
		// If so, move them the most (3 steps) and check if they reached the finish line
		// Else, move the other cyclists a minimum amount (1 step)
		if (j == luckyCyclist)
		{
			for (int i = 0; i < steps; i++)
			{
				if (cyclistPositions[luckyCyclist] < RACE_DISTANCE)
				{
					steppers[luckyCyclist].move(5);
					steppers[luckyCyclist].run();
					cyclistPositions[luckyCyclist]++;
				}
				// Check if the lucky cyclist has reached the finish line
				else
				{
					// Stop the stepper if it reaches the finish line
					steppers[luckyCyclist].stop();
					checkWinner();
					if (raceFinished) 
					{	
						currentState = STATE_RESULT;
						return;
					}
				}
			}
		}
		// If not the lucky cyclist, check if the others are still in the race
		// If they are, move them the minimum amount (1 step)
		else 
		{
			if (cyclistPositions[j] < RACE_DISTANCE)
			{
				// Move other cyclists minimum amount (1 step)
				steppers[j].move(1);
				steppers[j].run();
				cyclistPositions[j]++;
			}
			// Check if any of the other cyclists have reached the finish line
			// If so, stop them and check for the winner
			else
			{
				// Stop the stepper if it reaches the finish line
				steppers[j].stop();
				checkWinner();
				if (raceFinished) 
				{	
					currentState = STATE_RESULT;
					return;
				}
			}
		}
	}
	// Delay between steps
	// Randomize delay to simulate different speeds
	delay(STEP_DELAY + delayJitter);
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
    for (int i = 0; i < NUM_CYCLISTS; i++) {
        steppers[i].setMaxSpeed(1000);
        steppers[i].setAcceleration(500);
        steppers[i].setSpeed(-300); // Negative speed to rewind
        steppers[i].moveTo(-RACE_DISTANCE); // Move back enough distance
    }

    bool allAtStart = false;
    while (!allAtStart) {
        allAtStart = true;
        for (int i = 0; i < NUM_CYCLISTS; i++) {
            if (digitalRead(limitSwitchPins[i]) == HIGH) { // Not at start
                steppers[i].run();
                allAtStart = false;
            } else {
                steppers[i].stop();
                steppers[i].setCurrentPosition(0); // Reset position
            }
        }
        // Optional: add a small delay to avoid busy-waiting
        delay(2);
    }

    // Reset positions and state
    for (int i = 0; i < NUM_CYCLISTS; i++) {
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
char getDebouncedKey() {
    char key = getDebouncedKey();
    if (key) {
        unsigned long now = millis();
        if (now - lastKeypadTime > debounceDelay) {
            lastKeypadTime = now;
            return key;
        }
    }
    return NO_KEY;
}