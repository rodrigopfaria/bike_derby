
#include "Config.h"
#include "FSM_handlers.h"
#include "helper_functions.h"

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

	// Check if any cyclist has reached the finish line
	// If so, set raceFinished to true and display the winner
	// checkWinner();

	if (raceFinished)
	{
		currentState = STATE_RESULT;
		raceStartTime = 0; // Reset for next race
		return;
	}

	// Get a random cyclist based on the current scenario
	// This function will return a cyclist index based on the scenario probabilities
	// This cyclist will be the one to move the most in this iteration
	int luckyCyclist = getWeightedRandomCyclist(currentScenario);

	// Move cyclists
	for (int i = 0; i < NUM_CYCLISTS; i++)
	{
		// Check if the lucky cyclist is the one to move
		// If so, move them the most (3 steps) and check if they reached the finish line
		// Else, move the other cyclists a minimum amount (1 step)
		if (i == luckyCyclist)
		{
			int moveSteps = (random(0, 100) < BOOST_CHANCE) ? 1500 : 500; // 3x boost or normal
			// if (cyclistPositions[luckyCyclist] < RACE_DISTANCE)
			// {
				if (steppers[luckyCyclist].distanceToGo() == 0)
				{
					steppers[luckyCyclist].moveTo(cyclistPositions[luckyCyclist] + moveSteps);
				}
				steppers[luckyCyclist].run();
				cyclistPositions[luckyCyclist] = steppers[luckyCyclist].currentPosition();
			// }
			// Check if the lucky cyclist has reached the finish line
			// else
			// {
			// 	// Stop the stepper if it reaches the finish line
			// 	steppers[luckyCyclist].stop();
			// 	checkWinner();
			// 	if (raceFinished)
			// 	{
			// 		currentState = STATE_RESULT;
			// 		raceStartTime = 0; // Reset for next race
			// 		return;
			// 	}
			// }
		}
		// If not the lucky cyclist, check if the others are still in the race
		// If they are, move them the minimum amount (1 step)
		else
		{
			// if (cyclistPositions[i] < RACE_DISTANCE)
			// {
				// Move other cyclists minimum amount (5 step)
				if (steppers[i].distanceToGo() == 0)
				{
					steppers[i].moveTo(cyclistPositions[i] + 500);
				}
				steppers[i].run();
				cyclistPositions[i] = steppers[i].currentPosition();
			// }
			// Check if any of the other cyclists have reached the finish line
			// If so, stop them and check for the winner
			// else
			// {
			// 	// Stop the stepper if it reaches the finish line
			// 	steppers[i].stop();
			// 	checkWinner();
			// 	if (raceFinished)
			// 	{
			// 		currentState = STATE_RESULT;
			// 		raceStartTime = 0; // Reset for next race
			// 		return;
			// 	}
			// }
		}
	}
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