#include <Arduino.h>
#include <ESP32Servo.h>

// ============================================================================
// 1. PINS AND DEFINITIONS
// ============================================================================
const int TRIG_PIN  = 5;  // HC-SR04 Ultrasonic Trigger pin
const int ECHO_PIN  = 18; // HC-SR04 Ultrasonic Echo pin
const int SERVO_PIN = 13; // Servo signal pin

Servo myServo;

// Standard pulse widths for SG90 servo in microseconds
const int MIN_PULSE_WIDTH = 500;  // 0 degrees
const int MAX_PULSE_WIDTH = 2500; // 180 degrees

// Physical system constants
const float SETPOINT_CM = 15.0f; // Target position of ball from sensor (cm)
const int CENTER_ANGLE   = 90;   // Neutral angle when beam is level

// State variables for PD controller (Option 3)
float lastError = 0.0f;
unsigned long lastTimeMs = 0;


// ============================================================================
// 2. HELPER FUNCTIONS
// ============================================================================

/**
 * Safely writes an angle (0 to 180) to the servo.
 */
void writeServoAngle(int angle) {
  int clampedAngle = constrain(angle, 0, 180);
  myServo.write(clampedAngle);
}

/**
 * Reads distance from HC-SR04 sensor in centimeters.
 * Returns -1.0 if reading times out or is out of range.
 */
float readDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Pulse timing with ~26ms timeout (max distance approx 4.5m)
  long duration = pulseIn(ECHO_PIN, HIGH, 26000);
  if (duration == 0) return -1.0f;
  
  return (float)duration * 0.0343f / 2.0f;
}

/**
 * OPTION 1: Binary (Bang-Bang) Control
 * Switches between two extreme tilt angles depending on whether ball is 
 * closer or further than the setpoint.
 */
int transferBinary(float currentDistance, float setpoint = SETPOINT_CM, int tiltLow = 65, int tiltHigh = 115) {
  if (currentDistance < setpoint) {
    return tiltHigh; // Ball too close -> tilt beam away from sensor
  } else {
    return tiltLow;  // Ball too far -> tilt beam toward sensor
  }
}

/**
 * OPTION 2: Proportional (P) Control
 * Calculates tilt angle proportional to position error (P-only control).
 */
int transferP(float currentDistance, float setpoint = SETPOINT_CM, float Kp = 3.5f, int centerAngle = CENTER_ANGLE) {
  float error = currentDistance - setpoint; // Positive if ball is too far, negative if too close
  float outputAngle = centerAngle + (Kp * error);
  return constrain((int)outputAngle, 0, 180);
}

/**
 * OPTION 3: Proportional-Derivative (PD) Control
 * Reacts to position error (P) and rate of position change / speed (D).
 */
int transferPD(float currentDistance, float setpoint = SETPOINT_CM, float Kp = 3.5f, float Kd = 1.2f, int centerAngle = CENTER_ANGLE) {
  unsigned long currentTimeMs = millis();
  float dt = (currentTimeMs - lastTimeMs) / 1000.0f;
  
  // Guard against division by zero on rapid loops
  if (dt <= 0.001f) dt = 0.001f;

  float error = currentDistance - setpoint;
  float derivative = (error - lastError) / dt;

  float output = (Kp * error) + (Kd * derivative);

  // Store state for next derivative calculation
  lastError  = error;
  lastTimeMs = currentTimeMs;

  int targetAngle = centerAngle + (int)output;
  return constrain(targetAngle, 0, 180);
}


// ============================================================================
// 3. SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);

  // Configure HC-SR04 pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Attach servo motor
  myServo.attach(SERVO_PIN, MIN_PULSE_WIDTH, MAX_PULSE_WIDTH);
  writeServoAngle(CENTER_ANGLE);

  lastTimeMs = millis();
  Serial.println("Ball & Beam Feedback Control Setup Complete.");
}


// ============================================================================
// 4. LOOP
// ============================================================================
void loop() {
  // --------------------------------------------------------------------------
  // 4.1 GET INPUT
  // --------------------------------------------------------------------------
  float distanceCM = readDistanceCM();

  // Ignore invalid / noise sensor readings
  if (distanceCM <= 0.0f || distanceCM > 45.0f) {
    delay(20);
    return;
  }


  // --------------------------------------------------------------------------
  // 4.2 TRANSFORM FUNCTION (STUDENT TASK)
  // --------------------------------------------------------------------------
  int targetAngle = CENTER_ANGLE;

  // --- UNCOMMENT ONE OPTION BELOW TO RUN ---

  // OPTION 1: Binary (Bang-Bang) Control
  // targetAngle = transferBinary(distanceCM, SETPOINT_CM, 65, 115);

  // OPTION 2: Proportional (P) Control (Tweak gain Kp)
  // targetAngle = transferP(distanceCM, SETPOINT_CM, 3.5);

  // OPTION 3: Proportional-Derivative (PD) Control (Tweak gains Kp and Kd)
  // targetAngle = transferPD(distanceCM, SETPOINT_CM, 3.5, 1.2);


  // --------------------------------------------------------------------------
  // 4.3 WRITE ANGLE TO SERVO
  // --------------------------------------------------------------------------
  writeServoAngle(targetAngle);

  // Serial Monitor / Plotter debug output
  Serial.print("Setpoint:");
  Serial.print(SETPOINT_CM);
  Serial.print("\tDistance:");
  Serial.print(distanceCM);
  Serial.print("\tTargetAngle:");
  Serial.println(targetAngle);

  delay(20); // Control refresh rate (~50Hz loop)
}