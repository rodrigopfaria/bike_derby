#include "Config.h"
#include "helper_functions.h"

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

void calibrateSteppers()
{
	for (int i = 0; i < NUM_CYCLISTS; i++)
	{
		// Move backward to find the start limit switch
		steppers[i].setMaxSpeed(1000);
		steppers[i].setAcceleration(8000);
		steppers[i].moveTo(-10000); // move far enough to reach limit

		while (digitalRead(limitSwitchPinsStart[i]) == HIGH)
		{
			steppers[i].run();
		}

		steppers[i].stop(); // stop immediately
		while (steppers[i].isRunning())
			steppers[i].run(); // wait until stopped

		steppers[i].setCurrentPosition(0); // save as initial (home) position

		delay(500); // brief pause

		// Move forward to find the end limit switch
		steppers[i].moveTo(10000); // move far enough forward

		while (digitalRead(limitSwitchPinsEnd[i]) == HIGH)
		{
			steppers[i].run();
		}

		steppers[i].stop();
		while (steppers[i].isRunning())
			steppers[i].run();

		long endPosition = steppers[i].currentPosition(); // store max range if needed

		Serial.print("Stepper ");
		Serial.print(i);
		Serial.print(" calibrated. Max steps: ");
		Serial.println(endPosition);

		// move back to home
		steppers[i].moveTo(0);
		while (steppers[i].distanceToGo() != 0)
		{
			steppers[i].run();
		}
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