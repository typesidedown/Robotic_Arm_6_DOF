#include <ESP32Servo.h>

Servo servos[6];
int servoPins[6] = {22, 23, 15, 2, 4, 5};
int servoAngles[6] = {90, 90, 90, 90, 90, 90};  // Start at midpoint
void handleCommand(char cmd);
void adjustServo(int index, int change);


void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 6; i++) {
    servos[i].attach(servoPins[i]);
    servos[i].write(servoAngles[i]);
  }
  Serial.println("6 DOF Robotic Arm Control Ready. Use keys q/a, w/s, e/d, r/f, t/g, y/h.");
}

void loop() {
  if (Serial.available()) {
    char cmd = Serial.read();
    handleCommand(cmd);
  }
}

void handleCommand(char cmd) {
  int delta = 5;  // angle change step

  switch (cmd) {
    case 'q': adjustServo(0, delta); break;
    case 'a': adjustServo(0, -delta); break;
    case 'w': adjustServo(1, delta); break;
    case 's': adjustServo(1, -delta); break;
    case 'e': adjustServo(2, delta); break;
    case 'd': adjustServo(2, -delta); break;
    case 'r': adjustServo(3, delta); break;
    case 'f': adjustServo(3, -delta); break;
    case 't': adjustServo(4, delta); break;
    case 'g': adjustServo(4, -delta); break;
    case 'y': adjustServo(5, delta); break;
    case 'h': adjustServo(5, -delta); break;
    default: Serial.println("Unknown command");
  }
}

void adjustServo(int index, int change) {
  servoAngles[index] = constrain(servoAngles[index] + change, 0, 180);
  servos[index].write(servoAngles[index]);
  Serial.printf("Servo %d -> %d degrees\n", index + 1, servoAngles[index]);
}
