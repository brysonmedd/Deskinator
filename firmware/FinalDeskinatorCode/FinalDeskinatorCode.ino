#include <Wire.h>
#include <Servo.h>


// Pin Definitions
// #define MOTOR1 2
// #define MOTOR2 3
// #define MOTOR3 4
// #define MOTOR4 5
#define SERVO 6
#define PIR 7
// #define ENA 9
// #define ENB 10
#define RED_LED 11
#define GREEN_LED 12
#define VACUUM 13
#define IR A0
#define BUZZER 8


// L298N – left motor
const int MOTOR1 = 2;   // L298N IN1
const int MOTOR2 = 3;   // L298N IN2
const int ENA = 9;   // L298N ENA (PWM)


// L298N – right motor
const int MOTOR3 = 4;   // L298N IN3
const int MOTOR4 = 5;   // L298N IN4
const int ENB = 10;   // L298N ENB (PWM)


#define EDGE_THRESHOLD 25  // Distance in cm that indicates an edge


// Servo Motor
Servo myServo;


// Robot States
enum State {IDLE, MOVING, PATH_FINDING, TURNING_LEFT, TURNING_RIGHT, COMPLETE};
State currentState = IDLE;


// Runtime Configuration
unsigned long startTime = 0;
const unsigned long runTime = 180000; // 3 minutes


// Sensor Variables
int dist_cm;
int PIR_val = 0;






// Turning Configuration
const int TURN_SPEED = 255;//150;
const int MOVE_SPEED = 255;//120;
const int TURN_STEP_DELAY = 50; // Small delay for incremental turning
const int SENSOR_CHECK_INTERVAL = 100; // Check edge every 100ms while moving
unsigned long lastSensorCheck = 0;
unsigned long turnStartTime = 0;
const int MAX_TURN_TIME = 1500; // Maximum time for a 90-degree turn






void setup() {
  Serial.begin(9600);


  // Establishing all output pins
  pinMode(MOTOR1, OUTPUT);
  pinMode(MOTOR2, OUTPUT);
  pinMode(MOTOR3, OUTPUT);
  pinMode(MOTOR4, OUTPUT);
  pinMode(SERVO, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(VACUUM, OUTPUT);
  pinMode(BUZZER, OUTPUT);


  // Establishing Input Pin
  pinMode(PIR, INPUT);


  // Attaching Servo
  myServo.attach(SERVO);
  myServo.write(90); // Center position


  // Writing Initial Values
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, HIGH);
  digitalWrite(VACUUM, LOW);
  stopMotors();


  Serial.println("System ready. Waiting for motion detection...");
}


void loop() {
  unsigned long elapsed = (currentState == IDLE) ? 0 : (millis() - startTime);


  // Check if runtime has expired (only if robot is running)
  if (currentState != IDLE && currentState != COMPLETE && elapsed >= runTime) {
    currentState = COMPLETE;
    Serial.print("Runtime completed. Elapsed: ");
    Serial.print(elapsed);
    Serial.println(" ms");
  }


  // State Machine
  switch (currentState) {
    case IDLE:
      handleIdleState();
      break;


    case MOVING:
      handleMovingState(elapsed);
      break;


    case PATH_FINDING:
      handlePathFindingState(elapsed);
      break;


    case TURNING_LEFT:
      handleTurningLeftState(elapsed);
      break;


    case TURNING_RIGHT:
      handleTurningRightState(elapsed);
      break;


    case COMPLETE:
      handleCompleteState();
      break;
  }
}


void handleIdleState() {
  // Wait for PIR motion detection
  PIR_val = digitalRead(PIR);
  if (PIR_val == HIGH) {
    // Start the robot
    currentState = MOVING;
    startTime = millis();
    lastSensorCheck = millis();


    // Activate LEDs and Vacuum
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED, LOW);
    digitalWrite(VACUUM, HIGH);


    Serial.println("Motion detected! Starting cleaning cycle...");
  }
}


void handleMovingState(unsigned long elapsed) {
  if (elapsed >= runTime) return;


  // Continuously check for edges while moving
  if (millis() - lastSensorCheck >= SENSOR_CHECK_INTERVAL) {
    lastSensorCheck = millis();
   
    // Get distance reading
    dist_cm = getDistance(analogRead(IR));
   
    Serial.print("Moving - Distance: ");
    Serial.print(dist_cm);
    Serial.println(" cm");


    // Check if edge detected
    if (dist_cm > EDGE_THRESHOLD) {
      Serial.println("Edge detected! Entering path-finding mode...");
      stopMotors();
      currentState = PATH_FINDING;
      return;
    }
  }


  // Continue moving forward
  analogWrite(ENA, MOVE_SPEED); // Setting Motor Speed
  analogWrite(ENB, MOVE_SPEED);
  digitalWrite(MOTOR1, LOW);
  digitalWrite(MOTOR2, HIGH);
  digitalWrite(MOTOR3, HIGH);
  digitalWrite(MOTOR4, LOW);
  Serial.println("hello- forward");
}


void handlePathFindingState(unsigned long elapsed) {
  if (elapsed >= runTime) return;


  Serial.println("Checking both directions...");


  // Stop all movement
  stopMotors();
  delay(200);


  // Check left side (servo at 180)
  myServo.write(180);
  delay(750); // Wait for servo to reach position and sensor to stabilize
  int leftDist = getDistance(analogRead(IR));
 
  Serial.print("Left distance: ");
  Serial.print(leftDist);
  Serial.println(" cm");


  // Return servo to center
  myServo.write(90);
  delay(300);


  // Check right side (servo at 0)
  myServo.write(0);
  delay(1000);
  int rightDist = getDistance(analogRead(IR));
 
  Serial.print("Right distance: ");
  Serial.print(rightDist);
  Serial.println(" cm");


  // Return servo to center
  myServo.write(90);
  delay(300);


  // Decide which direction to turn
  bool leftClear = (leftDist <= EDGE_THRESHOLD);
  bool rightClear = (rightDist <= EDGE_THRESHOLD);


  if (leftClear && rightClear) {
    // Both directions clear - prefer left
    Serial.println("Both directions clear - turning left");
    currentState = TURNING_LEFT;
    turnStartTime = millis();
  } else if (leftClear) {
    // Only left is clear
    Serial.println("Left is clear - turning left");
    currentState = TURNING_LEFT;
    turnStartTime = millis();
  } else if (rightClear) {
    // Only right is clear
    Serial.println("Right is clear - turning right");
    currentState = TURNING_RIGHT;
    turnStartTime = millis();
  } else {
    // Neither direction clear - back up and try again
    Serial.println("No clear path - backing up");
    backUp();
    // Stay in PATH_FINDING to check again
  }
}


void handleTurningLeftState(unsigned long elapsed) {
  if (elapsed >= runTime) return;


  // Check if turn is complete
  if (millis() - turnStartTime >= MAX_TURN_TIME) {
    Serial.println("Left turn complete - checking path ahead");
    stopMotors();
    delay(200);
   
    // Check if path ahead is clear after turn
    myServo.write(90);
    delay(300);
    dist_cm = getDistance(analogRead(IR));
   
    if (dist_cm > EDGE_THRESHOLD) {
      // Edge still ahead after turn - enter path finding
      Serial.println("Edge still detected after turn - re-evaluating");
      currentState = PATH_FINDING;
    } else {
      // Path is clear - resume moving
      Serial.println("Path clear - resuming forward motion");
      currentState = MOVING;
      lastSensorCheck = millis();
    }
    return;
  }


  // Continue turning left (no edge checking during turn)
  //analogWrite(ENB, TURN_SPEED);
  digitalWrite(MOTOR1, LOW);
  digitalWrite(MOTOR2, LOW);
  digitalWrite(MOTOR3, HIGH);
  digitalWrite(MOTOR4, LOW);
  analogWrite(ENB, TURN_SPEED);
  Serial.println("hello- left");
}


void handleTurningRightState(unsigned long elapsed) {
  if (elapsed >= runTime) return;


  // Check if turn is complete
  if (millis() - turnStartTime >= MAX_TURN_TIME) {
    Serial.println("Right turn complete - checking path ahead");
    stopMotors();
    delay(200);
   
    // Check if path ahead is clear after turn
    myServo.write(90);
    delay(300);
    dist_cm = getDistance(analogRead(IR));
   
    if (dist_cm > EDGE_THRESHOLD) {
      // Edge still ahead after turn - enter path finding
      Serial.println("Edge still detected after turn - re-evaluating");
      currentState = PATH_FINDING;
    } else {
      // Path is clear - resume moving
      Serial.println("Path clear - resuming forward motion");
      currentState = MOVING;
      lastSensorCheck = millis();
    }
    return;
  }


  // Continue turning right (no edge checking during turn)
  //analogWrite(ENA, TURN_SPEED);
  digitalWrite(MOTOR1, HIGH);
  digitalWrite(MOTOR2, LOW);
  digitalWrite(MOTOR3, LOW);
  digitalWrite(MOTOR4, LOW);
  analogWrite(ENA, TURN_SPEED);
  Serial.println("hello- right");
}


void handleCompleteState() {
  // Deactivate everything
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, HIGH);
  digitalWrite(VACUUM, LOW);
  stopMotors();
  myServo.write(90);
  tone(BUZZER, 350); // Send sound signal...
  delay(1000);         // ...for 1 sec
  noTone(BUZZER);     // Stop sound...
 
  Serial.println("Cleaning cycle complete. System stopped.");
 
  // Stay in COMPLETE state (do nothing further)
  delay(1000); // Prevent Serial spam
}


void stopMotors() {
  digitalWrite(MOTOR1, LOW);
  digitalWrite(MOTOR2, LOW);
  digitalWrite(MOTOR3, LOW);
  digitalWrite(MOTOR4, LOW);
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}


void backUp() {
  // Set motor speed for backing up
  analogWrite(ENA, MOVE_SPEED);
  analogWrite(ENB, MOVE_SPEED);


  // Drive backward
  digitalWrite(MOTOR1, HIGH);
  digitalWrite(MOTOR2, LOW);
  digitalWrite(MOTOR3, LOW);
  digitalWrite(MOTOR4, HIGH);


  // Back up for a short duration
  delay(500);


  // Stop
  stopMotors();
  delay(200);
}




float getDistance(int Signal) {
  int raw = analogRead(A0);
  float voltage = raw * (5.0 / 1023.0);
  float distance = 27.86 * pow(voltage, -1.15); // empirical fit
  Serial.println(distance);
  return distance;
}
