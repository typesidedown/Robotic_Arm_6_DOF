#include <ESP32Servo.h>      

// Define the restricted motion range for the servo motors
const int MIN = 20;      
const int MAX = 160;     

// Initialize speed to zero (no movement at start)
int speed = 0;


// Initial positions of the robotic arm's joints
int posn_base = 90;
int posn_shoulder = 90;
int posn_elbow = 90;
int posn_wrist = 90;
int posn_gripper_ud = 90;
int posn_gripper = 90;

// Analog pins connected to joystick axes for controlling each link
const int analog_pin_base = 25; // Joystick1 X-axis controls the base
const int analog_pin_shoulder = 26; // Joystick1 Y-axis controls the shoulder
const int analog_pin_elbow = 34;  // Joystick2 X-axis controls the elbow
const int analog_pin_wrist = 35;   // Joystick2 Y-axis controls the wrist
const int analog_pin_gripper_ud = 32;  // Joystick3 Y-axis controls the gripper's up/down motion
const int analog_pin_gripper = 33;  // Joystick3 X-axis controls the gripper's opening/closing

// Digital pins for controlling the servos
const int digital_pin_base = 5;
const int digital_pin_shoulder = 4;
const int digital_pin_elbow = 2;
const int digital_pin_wrist = 15;
const int digital_pin_gripper_ud = 22;
const int digital_pin_gripper = 23;

// Declare Servo objects to control each joint/link of the robotic arm
Servo base_servo;
Servo shoulder_servo;
Servo elbow_servo;
Servo wrist_servo;
Servo gripper_ud_servo;
Servo gripper_servo;

// Store the last updated time for each link's movement
unsigned long last_update_base = 0;
unsigned long last_update_shoulder = 0;
unsigned long last_update_elbow = 0;
unsigned long last_update_wrist = 0;
unsigned long last_update_gripper_ud = 0;
unsigned long last_update_gripper = 0;

// Set the time interval for updating the servo positions
const unsigned long update_interval = 20; // Update every 20 milliseconds

void setup() {
  // Start serial communication for debugging
  Serial.begin(9600);

  // Initialize the analog pins for joystick input
  pinMode(analog_pin_base, INPUT);
  pinMode(analog_pin_shoulder, INPUT);
  pinMode(analog_pin_elbow, INPUT);
  pinMode(analog_pin_wrist, INPUT);
  pinMode(analog_pin_gripper_ud, INPUT);
  pinMode(analog_pin_gripper, INPUT);

  // Attach each servo to its respective digital pin
  base_servo.attach(digital_pin_base);
  shoulder_servo.attach(digital_pin_shoulder);
  elbow_servo.attach(digital_pin_elbow);
  wrist_servo.attach(digital_pin_wrist);
  gripper_ud_servo.attach(digital_pin_gripper_ud);
  gripper_servo.attach(digital_pin_gripper);

  // Initialize all servos to their starting positions
  base_servo.write(posn_base);
  shoulder_servo.write(posn_shoulder);
  elbow_servo.write(posn_elbow);
  wrist_servo.write(posn_wrist);
  gripper_ud_servo.write(posn_gripper_ud);
  gripper_servo.write(posn_gripper);
}

void arm(int &posn, int analog_pin_link, Servo &link_servo, unsigned long &last_update) {
  unsigned long current_time = millis();  // Get the current time
  
  // Only update the position if the specified time interval has passed
  if (current_time - last_update >= update_interval) {    
    int js_read = analogRead(analog_pin_link);  // Read the joystick value
    speed = map(js_read, 0, 4095, -3, 3);  // Map joystick value to a speed range (-3 to 3)

    // Define a dead zone around the center of the joystick to avoid jittering
    if (js_read <= 1900 || js_read >= 3300) {  
      posn += speed;  // Update the position based on joystick input
      posn = constrain(posn, MIN, MAX);  // Constrain the position within the allowed range (MIN to MAX)
      link_servo.write(posn);  // Move the servo to the updated position
    }
    last_update = current_time;  // Update the last updated time for this link
  }
}

void loop() {
  arm(posn_base, analog_pin_base, base_servo, last_update_base);
  arm(posn_shoulder, analog_pin_shoulder, shoulder_servo, last_update_shoulder);
  arm(posn_elbow, analog_pin_elbow, elbow_servo, last_update_elbow);
  arm(posn_wrist, analog_pin_wrist, wrist_servo, last_update_wrist);
  arm(posn_gripper_ud, analog_pin_gripper_ud, gripper_ud_servo, last_update_gripper_ud);
  arm(posn_gripper, analog_pin_gripper, gripper_servo, last_update_gripper);
}