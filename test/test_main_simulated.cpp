#include <iostream>
#include <vector>
#include <string>
#include <queue>
#include <cassert>

// Mock Stepper
class MockStepper {
public:
    int pos = 0;
    int target = 0;
    int speed = 0;
    bool stopped = false;

    void setMaxSpeed(int) {}
    void setAcceleration(int) {}
    void setSpeed(int s) { speed = s; }
    void moveTo(int t) { target = t; }
    void move(int steps) { target = pos + steps; }
    void run() {
        if (pos < target) pos++;
        else if (pos > target) pos--;
    }
    void stop() { stopped = true; }
    void setCurrentPosition(int p) { pos = p; }
    int currentPosition() { return pos; }
    int distanceToGo() { return target - pos; }
};

// Mock LCD
class MockLCD {
public:
    std::string line1, line2;
    void clear() { line1 = line2 = ""; }
    void setCursor(int, int) {}
    void print(const std::string& s) {
        if (line1.empty()) line1 = s;
        else line2 = s;
    }
};

// Simulated Keypad
std::queue<char> keypadInputs;
char getSimulatedKey() {
    if (!keypadInputs.empty()) {
        char k = keypadInputs.front();
        keypadInputs.pop();
        return k;
    }
    return '\0';
}

// Simulated limit switches (HIGH = not at start, LOW = at start)
std::vector<int> limitSwitchStates;
int digitalRead(int idx) {
    return limitSwitchStates[idx];
}

// --- Constants and Globals ---
#define NUM_CYCLISTS 4
#define NUM_SCENARIOS 6
#define RACE_DISTANCE 10
#define BOOST_CHANCE 10
#define STEP_DELAY 100
#define debounceDelay 200

MockStepper steppers[NUM_CYCLISTS];
MockLCD lcd;
int currentScenario = -1;
int cyclistPositions[NUM_CYCLISTS] = {0};
bool raceFinished = false;
unsigned long lastKeypadTime = 0;
unsigned long lastRaceStepTime = 0;
int currentStepDelay = STEP_DELAY;
unsigned long raceStartTime = 0;
const unsigned long raceTimeout = 60000;
const unsigned long resetTimeout = 30000;
enum State { STATE_IDLE, STATE_SETUP, STATE_READY, STATE_RACE, STATE_RESULT };
State currentState = STATE_IDLE;

const unsigned int scenarioProbabilities[NUM_SCENARIOS][NUM_CYCLISTS] = {
    {30, 20, 15, 35},
    {10, 10, 30, 50},
    {25, 25, 10, 40},
    {5, 5, 20, 70},
    {16, 16, 16, 52},
    {10, 25, 15, 50}
};

// --- Mock millis() ---
unsigned long mockMillis = 0;
unsigned long millis() { return mockMillis; }
void advanceTime(unsigned long ms) { mockMillis += ms; }

// --- Mock random ---
int random(int min, int max) { return min + (mockMillis % (max - min)); }

// --- Mock display ---
void updateDisplay(const std::string& l1, const std::string& l2) {
    lcd.clear();
    lcd.print(l1);
    lcd.print(l2);
    std::cout << "[LCD] " << l1 << " | " << l2 << std::endl;
}

// --- Debounce function ---
char getDebouncedKey() {
    char key = getSimulatedKey();
    if (key) {
        unsigned long now = millis();
        if (now - lastKeypadTime > debounceDelay) {
            lastKeypadTime = now;
            return key;
        }
    }
    return '\0';
}

// --- Weighted random cyclist ---
int getWeightedRandomCyclist(unsigned int scenarioIndex) {
    int r = random(0, 100);
    int cumulative = 0;
    for (int i = 0; i < NUM_CYCLISTS; i++) {
        cumulative += scenarioProbabilities[scenarioIndex][i];
        if (r < cumulative) return i;
    }
    return -1;
}

// --- FSM Handlers (simplified for test) ---
void handleIdle() {
    char key = getDebouncedKey();
    if (key == 'A') {
        updateDisplay("Setup Mode", "Select scenario 1-6");
        currentState = STATE_SETUP;
    }
}

void handleSetup() {
    char key = getDebouncedKey();
    if (key >= '1' && key <= '6') {
        currentScenario = key - '1';
        updateDisplay("Scenario Selected", "SCN " + std::to_string(currentScenario + 1));
    }
    if (key == 'B' && currentScenario >= 0) {
        raceFinished = false;
        for (int i = 0; i < NUM_CYCLISTS; i++) {
            steppers[i].stop();
            cyclistPositions[i] = 0;
            steppers[i].setCurrentPosition(0);
        }
        updateDisplay("Ready Mode", "Press D to Go");
        currentState = STATE_READY;
    }
}

void handleReady() {
    char key = getDebouncedKey();
    if (key == 'D') {
        updateDisplay("Race Starting", "Go!");
        currentState = STATE_RACE;
    }
}

void checkWinner() {
    for (int i = 0; i < NUM_CYCLISTS; i++) {
        if (steppers[i].currentPosition() >= RACE_DISTANCE) {
            raceFinished = true;
            updateDisplay("Race Finished", "Winner: Horse " + std::to_string(i + 1));
            return;
        }
    }
}

void handleRace() {
    if (raceStartTime == 0) raceStartTime = millis();
    if (millis() - raceStartTime > raceTimeout) {
        updateDisplay("Race Timeout", "Check motors!");
        currentState = STATE_RESULT;
        return;
    }
    char key = getDebouncedKey();
    if (key == 'C') {
        // Simulate reset
        updateDisplay("Resetting", "Press A to start");
        currentState = STATE_IDLE;
        return;
    }
    unsigned long now = millis();
    if (now - lastRaceStepTime < currentStepDelay) return;
    lastRaceStepTime = now;
    currentStepDelay = STEP_DELAY + random(-20, 20);

    checkWinner();
    if (raceFinished) {
        currentState = STATE_RESULT;
        raceStartTime = 0;
        return;
    }
    int luckyCyclist = getWeightedRandomCyclist(currentScenario);
    for (int i = 0; i < NUM_CYCLISTS; i++) {
        if (i == luckyCyclist) {
            int moveSteps = (random(0, 100) < BOOST_CHANCE) ? 3 : 1;
            if (cyclistPositions[luckyCyclist] < RACE_DISTANCE) {
                if (steppers[luckyCyclist].distanceToGo() == 0) {
                    steppers[luckyCyclist].move(moveSteps);
                }
                steppers[luckyCyclist].run();
                cyclistPositions[luckyCyclist] = steppers[luckyCyclist].currentPosition();
            }
        } else {
            if (cyclistPositions[i] < RACE_DISTANCE) {
                if (steppers[i].distanceToGo() == 0) {
                    steppers[i].move(1);
                }
                steppers[i].run();
                cyclistPositions[i] = steppers[i].currentPosition();
            }
        }
    }
}

void handleResult() {
    char key = getDebouncedKey();
    if (key == 'C') {
        // Simulate reset
        updateDisplay("Resetting", "Press A to start");
        currentState = STATE_IDLE;
    }
}

// --- Test Harness ---
void runTestScenario() {
    // Simulate: Startup -> Idle -> Setup -> Ready -> Race -> Result -> Reset
    std::cout << "=== Test: Full Race ===" << std::endl;
    currentState = STATE_IDLE;
    currentScenario = -1;
    for (int i = 0; i < NUM_CYCLISTS; i++) {
        steppers[i].setCurrentPosition(0);
        cyclistPositions[i] = 0;
    }
    // Simulate pressing 'A' to enter setup
    keypadInputs.push('A');
    // Simulate selecting scenario 2 ('3')
    keypadInputs.push('3');
    // Simulate pressing 'B' to go to ready
    keypadInputs.push('B');
    // Simulate pressing 'D' to start race
    keypadInputs.push('D');

    // Main loop simulation
    int loopCount = 0;
    while (currentState != STATE_RESULT && loopCount < 1000) {
        switch (currentState) {
            case STATE_IDLE: handleIdle(); break;
            case STATE_SETUP: handleSetup(); break;
            case STATE_READY: handleReady(); break;
            case STATE_RACE: handleRace(); break;
            case STATE_RESULT: handleResult(); break;
        }
        advanceTime(50); // Simulate time passing
        loopCount++;
    }
    assert(currentState == STATE_RESULT);
    std::cout << "Race finished. Winner displayed above." << std::endl;

    // Simulate pressing 'C' to reset
    keypadInputs.push('C');
    handleResult();
    std::cout << "Reset complete. Back to idle." << std::endl;
}

int main() {
    // Simulate all limit switches as HIGH (not at start)
    limitSwitchStates = std::vector<int>(NUM_CYCLISTS, 1);

    runTestScenario();

    // Additional tests can be added here
    return 0;
}