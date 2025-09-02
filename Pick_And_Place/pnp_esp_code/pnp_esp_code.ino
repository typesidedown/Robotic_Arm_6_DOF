// You need to install the ESP32Servo library from the Arduino Library Manager
#include <ESP32Servo.h>

// --- Configuration ---
// IMPORTANT: Define the GPIO pins your servos are connected to on the ESP32
const int SERVO_PIN_1 = 5; // Base
const int SERVO_PIN_2 = 4; // Shoulder
const int SERVO_PIN_3 = 2; // Elbow
const int SERVO_PIN_4 = 15; // Wrist
const int GRIPPER_PIN = 22; // Gripper

// Create servo objects
Servo servo1;
Servo servo2;
Servo servo3;
Servo servo4;
Servo gripperServo;

// Variables to store the current and target angles for each servo
int targetAngle1 = 90, currentAngle1 = 90;
int targetAngle2 = 90, currentAngle2 = 90;
int targetAngle3 = 90, currentAngle3 = 90;
int targetAngle4 = 90, currentAngle4 = 90;
int targetGripperAngle = 90, currentGripperAngle = 90;

// --- Function to move a servo smoothly ---
void moveServoSlowly(Servo &servo, int &currentAngle, int targetAngle, int delayTime) {
  // Determine the direction of movement
  if (targetAngle > currentAngle) {
    for (int angle = currentAngle; angle <= targetAngle; angle++) {
      servo.write(angle);
      delay(delayTime);
    }
  } else {
    for (int angle = currentAngle; angle >= targetAngle; angle--) {
      servo.write(angle);
      delay(delayTime);
    }
  }
  // Update the current angle
  currentAngle = targetAngle;
}

void setup() {
  // Start serial communication
  Serial.begin(115200);
  Serial.println("ESP32 Robotic Arm Controller Initialized.");

  // Allow allocation of all timers
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  // Attach servos to their pins
  // You might need to adjust the min/max pulse widths for your specific servos
  // servo.attach(pin, min_pulse_width, max_pulse_width);
  servo1.attach(SERVO_PIN_1);
  servo2.attach(SERVO_PIN_2);
  servo3.attach(SERVO_PIN_3);
  servo4.attach(SERVO_PIN_4);
  gripperServo.attach(GRIPPER_PIN);

  // Move to initial "home" position
  Serial.println("Moving to home position...");
  servo1.write(currentAngle1);
  servo2.write(currentAngle2);
  servo3.write(currentAngle3);
  servo4.write(currentAngle4);
  gripperServo.write(currentGripperAngle);
  Serial.println("Ready to receive commands.");
}

void loop() {
  // Check if there is data available to read from the serial port
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('>');
    
    // Check if the command starts with the expected character
    if (command.startsWith("<")) {
      // Remove the start character
      command.remove(0, 1);
      
      Serial.print("Received raw command: ");
      Serial.println(command);

      // Parse the string to get the integer angles
      // sscanf is a C function to parse formatted strings
      int success = sscanf(command.c_str(), "%d,%d,%d,%d,%d", 
                            &targetAngle1, &targetAngle2, &targetAngle3, &targetAngle4, &targetGripperAngle);

      if (success == 5) { // Check if all 5 values were parsed correctly
        Serial.println("Command parsed successfully. Moving servos...");

        // --- Move the arm servos to their target positions sequentially ---
        // The delay of 15ms creates a smooth, deliberate movement.
        // Adjust this value to make the movement faster or slower.
        moveServoSlowly(servo1, currentAngle1, targetAngle1, 15);
        moveServoSlowly(servo2, currentAngle2, targetAngle2, 15);
        moveServoSlowly(servo3, currentAngle3, targetAngle3, 15);
        moveServoSlowly(servo4, currentAngle4, targetAngle4, 15);
        
        Serial.println("Arm movement complete.");

        // --- Move the gripper after the arm is in position ---
        moveServoSlowly(gripperServo, currentGripperAngle, targetGripperAngle, 20);
        Serial.println("Gripper action complete.");

        // Send an acknowledgment back to the Python script
        Serial.println("OK: Movement Finished");

      } else {
        Serial.println("Error: Command format incorrect.");
      }
    }
  }
}
