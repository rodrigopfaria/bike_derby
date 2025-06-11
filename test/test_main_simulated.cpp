#ifndef PLATFORMIO_UNIT_TEST
#if defined(ARDUINO) || defined(__ARDUINO__)
#include <Arduino.h>
#endif
#endif
#include <unity.h>
#include <vector>
#include <string>
#include <queue>

// --- Mock Classes (same as before) ---
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

// --- Simulated Inputs ---
std::queue<char> keypadInputs;
char getSimulatedKey() {
    if (!keypadInputs.empty()) {
        char k = keypadInputs.front();
        keypadInputs.pop();
        return k;
    }
    return '\0';
}
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

// --- FSM Handlers (same as before) ---
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
        updateDisplay("Resetting", "Press A to start");
        currentState = STATE_IDLE;
    }
}

// --- PlatformIO Unity Test ---
void test_full_race_scenario() {
    // Simulate: Startup -> Idle -> Setup -> Ready -> Race -> Result -> Reset
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
    TEST_ASSERT_EQUAL(STATE_RESULT, currentState);
    // Simulate pressing 'C' to reset
    keypadInputs.push('C');
    handleResult();
    TEST_ASSERT_EQUAL(STATE_IDLE, currentState);
}

void setUp(void) {
    // This is run before EACH test
    limitSwitchStates = std::vector<int>(NUM_CYCLISTS, 1);
    mockMillis = 0;
}

void tearDown(void) {
    // This is run after EACH test
}

void test_weighted_random_cyclist() {
    // Test that the function returns a valid index
    for (int s = 0; s < NUM_SCENARIOS; ++s) {
        int idx = getWeightedRandomCyclist(s);
        TEST_ASSERT_TRUE(idx >= 0 && idx < NUM_CYCLISTS);
    }
}

void test_debounced_key() {
    keypadInputs.push('A');
    char key = getDebouncedKey();
    TEST_ASSERT_EQUAL('A', key);
    // Should not return another key until debounceDelay has passed
    keypadInputs.push('B');
    key = getDebouncedKey();
    TEST_ASSERT_EQUAL('\0', key);
    advanceTime(debounceDelay + 1);
    key = getDebouncedKey();
    TEST_ASSERT_EQUAL('B', key);
}

void test_mock_stepper() {
    MockStepper s;
    s.setCurrentPosition(0);
    s.move(5);
    for (int i = 0; i < 5; ++i) s.run();
    TEST_ASSERT_EQUAL(5, s.currentPosition());
    s.move(-3);
    for (int i = 0; i < 3; ++i) s.run();
    TEST_ASSERT_EQUAL(2, s.currentPosition());
}

void test_mock_lcd() {
    lcd.clear();
    lcd.print("Hello");
    lcd.print("World");
    TEST_ASSERT_EQUAL_STRING("Hello", lcd.line1.c_str());
    TEST_ASSERT_EQUAL_STRING("World", lcd.line2.c_str());
}

void test_limit_switch_simulation() {
    limitSwitchStates = {1, 0, 1, 0};
    TEST_ASSERT_EQUAL(1, digitalRead(0));
    TEST_ASSERT_EQUAL(0, digitalRead(1));
    TEST_ASSERT_EQUAL(1, digitalRead(2));
    TEST_ASSERT_EQUAL(0, digitalRead(3));
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_full_race_scenario);
    RUN_TEST(test_weighted_random_cyclist);
    RUN_TEST(test_debounced_key);
    RUN_TEST(test_mock_stepper);
    RUN_TEST(test_mock_lcd);
    RUN_TEST(test_limit_switch_simulation);
    UNITY_END();
}

void loop() {
    // not used
}

#ifndef ARDUINO
int main() {
    setup();
    return 0;
}
#endif