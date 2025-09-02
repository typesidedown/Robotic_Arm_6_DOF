// Import required libraries
#include <WiFi.h>
#include <WebSocketsClient.h> // IMPORTANT: This is the WebSocket CLIENT library!
#include <ESP32Servo.h>       // Library for controlling servos on ESP32
#include <ArduinoJson.h>      // Library for parsing and creating JSON messages

// --- Network Credentials ---
// Replace with your network credentials
// const char* ssid = "CHENAB";
// const char* password = "44zMf3QqdU&KC3Mv";

const char* ssid = "TinkerersLab_IITRopar";
const char* password = "TinkerersLab@2024";


// Import required libraries

// --- WebSocket Relay Server Details ---
// IMPORTANT: Replace with your Render.com WebSocket Relay URL and port.
// Use the hostname part of your Render.com URL (e.g., "robotic-arm-relay.onrender.com")

// The library expects only the domain name here.
const char* WEBSOCKET_RELAY_HOST = "robotic-arm-relay.onrender.com"; // <<< REPLACE THIS! (ensure no trailing slash)
const int WEBSOCKET_RELAY_PORT = 443; // 443 for WSS (secure WebSocket), 80 for WS (insecure)
const char* WEBSOCKET_RELAY_PATH = "/ws?type=esp32"; // Path with type parameter for relay identification

// Create WebSocketsClient object
WebSocketsClient webSocket;

// --- Servo Pin Definitions ---
// IMPORTANT: Adjust these GPIO pins to match your robotic arm's wiring
const int JOINT1_PIN = 5; // Base Joint
const int JOINT2_PIN = 4; // Shoulder Joint
const int JOINT3_PIN = 2; // Elbow Joint
const int JOINT4_PIN = 15; // Wrist Roll Joint
const int JOINT5_PIN = 22; // Wrist Pitch Joint
const int JOINT6_PIN = 25; // Wrist Yaw Joint
const int GRIPPER_PIN = 21; // Gripper Servo

// --- Servo Objects ---
Servo joint1Servo;
Servo joint2Servo;
Servo joint3Servo;
Servo joint4Servo;
Servo joint5Servo;
Servo joint6Servo;
Servo gripperServo;

// --- Global State Variables ---
// Store current joint angles and gripper value to send status back to UI
float currentJointAngles[6] = {0, 0, 0, 0, 0, 0}; // Stores angles for Joint 1 to Joint 6
float currentGripperValue = 0; // Stores gripper value as 0-100%

// --- Joint Angle Limits ---
// Define min/max angles for each joint. These should ideally match the ranges
// set in your robotic_arm.html UI sliders for consistent control.
const int JOINT_MIN_ANGLES[] = {0, 0, 0, 0, 0, 0}; // Example min angles
const int JOINT_MAX_ANGLES[] = {180, 180, 180, 180, 180, 180};      // Example max angles

// Gripper servo angle mapping.
const int GRIPPER_SERVO_CLOSED_ANGLE = 0;   // Angle for 0% gripper (closed)
const int GRIPPER_SERVO_OPEN_ANGLE = 180; // Angle for 100% gripper (open)

// --- Timing for Status Updates ---
unsigned long lastStatusSendTime = 0;
const long STATUS_SEND_INTERVAL = 100; // Send status every 100 milliseconds

// --- WebSocket Communication Functions ---

/**
 * @brief Sends the current robot status (joint angles, gripper, etc.) to the connected WebSocket relay.
 */
void sendRobotStatus() {
  if (!webSocket.isConnected()) return;

  StaticJsonDocument<256> doc;
  doc["type"] = "robot_status";
  doc["joint1_current"] = currentJointAngles[0];
  doc["joint2_current"] = currentJointAngles[1];
  doc["joint3_current"] = currentJointAngles[2];
  doc["joint4_current"] = currentJointAngles[3];
  doc["joint5_current"] = currentJointAngles[4];
  doc["joint6_current"] = currentJointAngles[5];
  doc["gripper_current"] = currentGripperValue;
  doc["x_current"] = 0.0;
  doc["y_current"] = 0.0;
  doc["z_current"] = 0.0;
  doc["roll_current"] = 0.0;
  doc["pitch_current"] = 0.0;
  doc["yaw_current"] = 0.0;

  String output;
  serializeJson(doc, output);
  webSocket.sendTXT(output);
}

/**
 * @brief Sends a log message to the connected WebSocket relay.
 */
void sendLogMessage(const char* message, const char* level = "info") {
  if (!webSocket.isConnected()) return;

  StaticJsonDocument<256> doc;
  doc["type"] = "log";
  doc["level"] = level;
  doc["message"] = message;

  String output;
  serializeJson(doc, output);
  webSocket.sendTXT(output);
}

/**
 * @brief Sends an error message to the connected WebSocket relay.
 */
void sendErrorMessage(const char* message, const char* code = "UNKNOWN_ERROR") {
  if (!webSocket.isConnected()) return;

  StaticJsonDocument<256> doc;
  doc["type"] = "error";
  doc["code"] = code;
  doc["message"] = message;

  String output;
  serializeJson(doc, output);
  webSocket.sendTXT(output);
}

/**
 * @brief Sets the angles for all six robotic arm joints.
 */
void setJointAngles(float j1, float j2, float j3, float j4, float j5, float j6) {
  j1 = constrain(j1, JOINT_MIN_ANGLES[0], JOINT_MAX_ANGLES[0]);
  j2 = constrain(j2, JOINT_MIN_ANGLES[1], JOINT_MAX_ANGLES[1]);
  j3 = constrain(j3, JOINT_MIN_ANGLES[2], JOINT_MAX_ANGLES[2]);
  j4 = constrain(j4, JOINT_MIN_ANGLES[3], JOINT_MAX_ANGLES[3]);
  j5 = constrain(j5, JOINT_MIN_ANGLES[4], JOINT_MAX_ANGLES[4]);
  j6 = constrain(j6, JOINT_MIN_ANGLES[5], JOINT_MAX_ANGLES[5]);

  joint1Servo.write(j1);
  joint2Servo.write(j2);
  joint3Servo.write(j3);
  joint4Servo.write(j4);
  joint5Servo.write(j5);
  joint6Servo.write(j6);

  currentJointAngles[0] = j1;
  currentJointAngles[1] = j2;
  currentJointAngles[2] = j3;
  currentJointAngles[3] = j4;
  currentJointAngles[4] = j5;
  currentJointAngles[5] = j6;

  char logMsg[150];
  snprintf(logMsg, sizeof(logMsg), "Moved to J1:%.1f, J2:%.1f, J3:%.1f, J4:%.1f, J5:%.1f, J6:%.1f", j1, j2, j3, j4, j5, j6);
  Serial.println(logMsg);
  sendLogMessage(logMsg, "info");
}

/**
 * @brief Sets the gripper position based on a 0-100% value.
 */
void setGripper(float value) {
  value = constrain(value, 0, 100);
  int servoAngle = map(value, 0, 100, GRIPPER_SERVO_CLOSED_ANGLE, GRIPPER_SERVO_OPEN_ANGLE);
  gripperServo.write(servoAngle);
  currentGripperValue = value;

  char logMsg[70];
  snprintf(logMsg, sizeof(logMsg), "Gripper set to %.0f%% (Servo Angle: %d)", value, servoAngle);
  Serial.println(logMsg);
  sendLogMessage(logMsg, "info");
}

/**
 * @brief Handles incoming WebSocket data messages from the relay.
 */
void handleRelayMessage(const char* payload, size_t length) {
    Serial.printf("Received message from relay: %s\n", payload);
    sendLogMessage(String("Received from relay: " + String(payload)).c_str(), "info");

    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      Serial.printf("deserializeJson() failed: %s\n", error.c_str());
      sendErrorMessage("Failed to parse JSON message from relay.", "JSON_PARSE_ERROR");
      return;
    }

    String command = doc["command"].as<String>();

    if (command == "move_joint") {
      float j1 = doc["joint1"] | currentJointAngles[0];
      float j2 = doc["joint2"] | currentJointAngles[1];
      float j3 = doc["joint3"] | currentJointAngles[2];
      float j4 = doc["joint4"] | currentJointAngles[3];
      float j5 = doc["joint5"] | currentJointAngles[4];
      float j6 = doc["joint6"] | currentJointAngles[5];
      setJointAngles(j1, j2, j3, j4, j5, j6);

    } else if (command == "stop") {
      sendLogMessage("Emergency STOP command received! Arm halted.", "error");
      Serial.println("Emergency STOP command received!");
      // Implement stop logic here (e.g., detach servos)
    } else if (command == "go_home") {
      setJointAngles(0, 0, 0, 0, 0, 0); // Example home position
      setGripper(0); // Close gripper at home
      sendLogMessage("Go Home command received. Moving to home position.", "info");
      Serial.println("Go Home command received.");
    } else if (command == "set_gripper") {
      float value = doc["value"] | currentGripperValue;
      setGripper(value);
    } else if (command == "move_preset") {
      float j1 = doc["joint1"] | currentJointAngles[0];
      float j2 = doc["joint2"] | currentJointAngles[1];
      float j3 = doc["joint3"] | currentJointAngles[2];
      float j4 = doc["joint4"] | currentJointAngles[3];
      float j5 = doc["joint5"] | currentJointAngles[4];
      float j6 = doc["joint6"] | currentJointAngles[5];
      float gripperVal = doc["gripper"] | currentGripperValue;
      String presetName = doc["name"].as<String>();

      setJointAngles(j1, j2, j3, j4, j5, j6);
      setGripper(gripperVal);
      char logMsg[100];
      snprintf(logMsg, sizeof(logMsg), "Moving to preset: %s", presetName.c_str());
      sendLogMessage(logMsg, "info");
      Serial.println(logMsg);
    }
    else {
      sendErrorMessage("Unknown command received from relay.", "UNKNOWN_COMMAND");
      Serial.printf("Unknown command: %s\n", command.c_str());
    }
}

/**
 * @brief WebSocket event handler for the client.
 */
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WS] Disconnected!\n");
      // Note: No need to call sendLogMessage here as we are disconnected.
      break;
    case WStype_CONNECTED:
      Serial.printf("[WS] Connected to url: %s\n", payload);
      sendLogMessage("ESP32 Controller connected to relay!", "success");
      sendRobotStatus(); // Send initial status
      break;
    case WStype_TEXT:
      handleRelayMessage((const char*)payload, length);
      break;
    case WStype_BIN:
      Serial.printf("[WS] get binary length: %u\n", length);
      break;
    case WStype_ERROR:
      Serial.printf("[WS] Error: %s\n", payload);
      break;
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
      break;
  }
}

/**
 * @brief Arduino setup function.
 */
void setup(){
  Serial.begin(115200);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  joint1Servo.setPeriodHertz(50);
  joint2Servo.setPeriodHertz(50);
  joint3Servo.setPeriodHertz(50);
  joint4Servo.setPeriodHertz(50);
  joint5Servo.setPeriodHertz(50);
  joint6Servo.setPeriodHertz(50);
  gripperServo.setPeriodHertz(50);

  joint1Servo.attach(JOINT1_PIN, 500, 2500);
  joint2Servo.attach(JOINT2_PIN, 500, 2500);
  joint3Servo.attach(JOINT3_PIN, 500, 2500);
  joint4Servo.attach(JOINT4_PIN, 500, 2500);
  joint5Servo.attach(JOINT5_PIN, 500, 2500);
  joint6Servo.attach(JOINT6_PIN, 500, 2500);
  gripperServo.attach(GRIPPER_PIN, 500, 2500);

  setJointAngles(0, 0, 0, 0, 0, 0);
  setGripper(0);

  // --- WiFi Connection ---
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi..");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");
  Serial.print("ESP Local IP Address: ");
  Serial.println(WiFi.localIP());

  // --- WebSocket Client Setup ---
  // Use webSocket.beginSSL() for WSS (secure) connections, webSocket.begin() for WS (insecure)
  
  if (WEBSOCKET_RELAY_PORT == 443) {
    // For WSS (wss://), we use beginSSL. Passing no CA certificate or fingerprint
    // allows connection to servers where the ESP32 can't validate the root CA.
    // *** MODIFIED: Removed the incorrect webSocket.setInsecure() call. ***
    webSocket.beginSSL(WEBSOCKET_RELAY_HOST, WEBSOCKET_RELAY_PORT, WEBSOCKET_RELAY_PATH, "", "arduino");
    Serial.printf("Attempting to connect to WSS relay: %s:%d%s\n", WEBSOCKET_RELAY_HOST, WEBSOCKET_RELAY_PORT, WEBSOCKET_RELAY_PATH);
  } else {
    webSocket.begin(WEBSOCKET_RELAY_HOST, WEBSOCKET_RELAY_PORT, WEBSOCKET_RELAY_PATH, "arduino");
    Serial.printf("Attempting to connect to WS relay: %s:%d%s\n", WEBSOCKET_RELAY_HOST, WEBSOCKET_RELAY_PORT, WEBSOCKET_RELAY_PATH);
  }

  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
  webSocket.setExtraHeaders("User-Agent: ESP32-RoboticArm");

  Serial.println("WebSocket client setup complete.");
}

/**
 * @brief Arduino loop function.
 */
void loop() {
  webSocket.loop();

  if (millis() - lastStatusSendTime > STATUS_SEND_INTERVAL) {
    sendRobotStatus();
    lastStatusSendTime = millis();
  }
}