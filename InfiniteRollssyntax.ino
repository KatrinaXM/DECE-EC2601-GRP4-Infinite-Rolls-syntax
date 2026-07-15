
#include <Servo.h>

// ---------- Pin definitions ----------
// Roll 1
const int TRIG1 = 4;
const int ECHO1 = 2;
const int RED1 = 12;
const int GREEN1 = 8;

// Roll 2
const int TRIG2 = 11;
const int ECHO2 = 10;
const int RED2 = 3;
const int GREEN2 = 7;
const int YELLOW2 = 5;
const int SERVO_PIN = 9;

// Outer door
const int OUTER_RED = 13;
const int OUTER_GREEN = 6;

// ---------- Distance thresholds (cm) ----------
const int FULL_DIST = 30;      // reference: roll is brand new / full
const int RESET_DIST = 32;     // below this we consider "new roll inserted"
const int ROLL1_LOW = 45;      // roll1 green -> red
const int ROLL2_LOW = 45;      // roll2 green -> yellow
const int ROLL2_CRITICAL = 50; // roll2 yellow -> red
const int EMPTY_DIST = 60;     // roll considered fully finished

// ---------- State ----------
enum LedState { GREEN, YELLOW, RED };

LedState led1State = GREEN;
LedState led2State = GREEN;

bool roll1EmptyLatch = false;
bool roll2EmptyLatch = false;

bool servo1Done = false; // has servo already reacted to roll1 finishing?
bool servo2Done = false; // has servo already reacted to roll2 finishing?

int activeRoll = 1; // roll currently being dispensed (1 or 2) - user starts with roll 1

Servo dispenserServo;

// ---------- Setup ----------
void setup() {
  Serial.begin(9600);

  pinMode(TRIG1, OUTPUT);
  pinMode(ECHO1, INPUT);
  pinMode(TRIG2, OUTPUT);
  pinMode(ECHO2, INPUT);

  pinMode(RED1, OUTPUT);
  pinMode(GREEN1, OUTPUT);
  pinMode(RED2, OUTPUT);
  pinMode(GREEN2, OUTPUT);
  pinMode(YELLOW2, OUTPUT);

  pinMode(OUTER_RED, OUTPUT);
  pinMode(OUTER_GREEN, OUTPUT);

  dispenserServo.attach(SERVO_PIN);
  dispenserServo.write(0); // starting position: dispensing roll 1

  // Start state: both rolls assumed full -> both green
  digitalWrite(GREEN1, HIGH);
  digitalWrite(RED1, LOW);
  digitalWrite(GREEN2, HIGH);
  digitalWrite(YELLOW2, LOW);
  digitalWrite(RED2, LOW);
  digitalWrite(OUTER_GREEN, HIGH);
  digitalWrite(OUTER_RED, LOW);
}

// ---------- Helper: read distance from HC-SR04 ----------
long readDistanceCM(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000); // 30ms timeout (~5m range)
  if (duration == 0) {
    return -1; // no echo received, treat as invalid reading
  }
  long distance = duration * 0.034 / 2;
  return distance;
}

// ---------- Main loop ----------
void loop() {
  long distance1 = readDistanceCM(TRIG1, ECHO1);
  long distance2 = readDistanceCM(TRIG2, ECHO2);

  // Ignore invalid/no-echo readings by skipping updates this cycle
  if (distance1 > 0) {
    updateRoll1(distance1);
  }
  if (distance2 > 0) {
    updateRoll2(distance2);
  }

  handleServoSwitching();
  updateOuterLED();

  Serial.print("Roll1: "); Serial.print(distance1); Serial.print("cm  ");
  Serial.print("Roll2: "); Serial.print(distance2); Serial.print("cm  ");
  Serial.print("Active: "); Serial.println(activeRoll);

  delay(200); // sensor poll interval
}

// ---------- Roll 1 logic (Green / Red only) ----------
void updateRoll1(long distance1) {
  // Latch "empty" once it hits the empty distance
  if (distance1 >= EMPTY_DIST) {
    roll1EmptyLatch = true;
  } else if (distance1 <= RESET_DIST) {
    // A fresh roll has been inserted -> clear the latch, allow servo to react again next time
    roll1EmptyLatch = false;
    servo1Done = false;
  }

  if (roll1EmptyLatch || distance1 >= ROLL1_LOW) {
    led1State = RED;
    digitalWrite(GREEN1, LOW);
    digitalWrite(RED1, HIGH);
  } else {
    led1State = GREEN;
    digitalWrite(RED1, LOW);
    digitalWrite(GREEN1, HIGH);
  }
}

// ---------- Roll 2 logic (Green / Yellow / Red) ----------
void updateRoll2(long distance2) {
  if (distance2 >= EMPTY_DIST) {
    roll2EmptyLatch = true;
  } else if (distance2 <= RESET_DIST) {
    roll2EmptyLatch = false;
    servo2Done = false;
  }

  if (roll2EmptyLatch || distance2 >= ROLL2_CRITICAL) {
    led2State = RED;
    digitalWrite(GREEN2, LOW);
    digitalWrite(YELLOW2, LOW);
    digitalWrite(RED2, HIGH);
  } else if (distance2 >= ROLL2_LOW) {
    led2State = YELLOW;
    digitalWrite(GREEN2, LOW);
    digitalWrite(RED2, LOW);
    digitalWrite(YELLOW2, HIGH);
  } else {
    led2State = GREEN;
    digitalWrite(RED2, LOW);
    digitalWrite(YELLOW2, LOW);
    digitalWrite(GREEN2, HIGH);
  }
}

// ---------- Servo switching: only reacts to the ACTIVE roll finishing ----------
void handleServoSwitching() {
  if (activeRoll == 1 && roll1EmptyLatch && !servo1Done) {
    // Roll 1 (in use) just finished -> swing dispenser to roll 2
    dispenserServo.write(270);
    servo1Done = true;
    activeRoll = 2;
  } else if (activeRoll == 2 && roll2EmptyLatch && !servo2Done) {
    // Roll 2 (in use) just finished -> swing dispenser back to roll 1
    dispenserServo.write(0);
    servo2Done = true;
    activeRoll = 1;
  }
  // Note: if the NON-active roll becomes empty/is replenished, nothing
  // happens to the servo - it only moves when the currently active roll
  // (the one actually being used) reaches the empty distance.
}

// ---------- Outer door LED: GREEN only if both rolls are GREEN ----------
void updateOuterLED() {
  if (led1State == GREEN && led2State == GREEN) {
    digitalWrite(OUTER_GREEN, HIGH);
    digitalWrite(OUTER_RED, LOW);
  } else {
    // Covers R+R, R+G, Y+G, R+Y, G+R etc. -> all resolve to RED
    digitalWrite(OUTER_GREEN, LOW);
    digitalWrite(OUTER_RED, HIGH);
  }
}
