import cv2
import mediapipe as mp
import serial
import time

# --- CONFIGURATION ---
# IMPORTANT: Change this to the serial port your ESP32 is connected to.
# On Windows, it will be 'COMx' (e.g., 'COM3')
# On macOS or Linux, it will be '/dev/tty.usbmodem...' or '/dev/ttyUSB...'
SERIAL_PORT = 'COM4'  # <-- CHANGE THIS
SERIAL_BAUDRATE = 115200
WEBCAM_INDEX = 1  # 0 for the default webcam, 1 for an external one, etc.

# Time delay between sending commands to prevent flooding the serial port
COMMAND_DELAY = 0.2  # seconds

# --- SERIAL COMMUNICATION SETUP ---
try:
    # Initialize the serial connection
    ser = serial.Serial(SERIAL_PORT, SERIAL_BAUDRATE, timeout=1)
    print(f"Successfully connected to {SERIAL_PORT}")
    # Give the ESP32 time to reset
    time.sleep(2)
except serial.SerialException as e:
    print(f"Error: Could not open serial port {SERIAL_PORT}.")
    print(f"Please check the port name and ensure the device is connected.")
    print(f"Details: {e}")
    exit()

# --- MEDIAPIPE HAND TRACKING SETUP ---
mp_hands = mp.solutions.hands
mp_drawing = mp.solutions.drawing_utils
hands = mp_hands.Hands(
    max_num_hands=1,  # We only need to track one hand
    min_detection_confidence=0.8,
    min_tracking_confidence=0.7
)

# --- WEBCAM SETUP ---
cap = cv2.VideoCapture(WEBCAM_INDEX)
if not cap.isOpened():
    print(f"Error: Could not open webcam with index {WEBCAM_INDEX}.")
    exit()

# --- MAIN LOGIC ---
last_command_time = 0

def get_finger_count(hand_landmarks, frame_shape):
    """
    Counts the number of fingers held up.
    It checks if the fingertip is above its corresponding knuckle.
    """
    if hand_landmarks is None:
        return 0

    # Landmark indices for fingertips and their bases
    tip_ids = [4, 8, 12, 16, 20] # Thumb, Index, Middle, Ring, Pinky
    count = 0

    # Get image dimensions
    height, width, _ = frame_shape

    # Get all landmark coordinates
    landmarks = hand_landmarks.landmark

    # --- Thumb ---
    # The thumb is special. We check if its tip is to the left (for right hand)
    # or right (for left hand) of its base.
    # A simple way is to check the x-coordinate.
    # landmarks[0] is the wrist. We can use it to guess if it's a left or right hand.
    if landmarks[tip_ids[0]].x < landmarks[tip_ids[0] - 1].x: # A simple check for right hand
         if landmarks[tip_ids[0]].x < landmarks[tip_ids[0] - 1].x:
              count += 1
    else: # for left hand
         if landmarks[tip_ids[0]].x > landmarks[tip_ids[0] - 1].x:
              count += 1

    # --- Other Four Fingers ---
    for i in range(1, 5):
        # Check if the fingertip's y-coordinate is above the knuckle's y-coordinate
        if landmarks[tip_ids[i]].y < landmarks[tip_ids[i] - 2].y:
            count += 1

    return count

def send_command(command):
    """Sends a command to the ESP32 over serial."""
    global last_command_time
    current_time = time.time()
    if current_time - last_command_time > COMMAND_DELAY:
        ser.write(command.encode())
        print(f"Sent command: '{command}'")
        last_command_time = current_time

print("\n--- Starting Gesture Control ---")
print("Show gestures to the camera.")
print("Press 'q' to quit.")

while cap.isOpened():
    success, frame = cap.read()
    if not success:
        print("Ignoring empty camera frame.")
        continue

    # Flip the frame horizontally for a mirror-like view
    frame = cv2.flip(frame, 1)

    # Convert the BGR image to RGB for MediaPipe
    rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)

    # Process the frame to find hands
    results = hands.process(rgb_frame)

    # Default command is to do nothing
    command_to_send = None
    gesture_text = "No Hand Detected"

    if results.multi_hand_landmarks:
        # We only use the first detected hand
        hand_landmarks = results.multi_hand_landmarks[0]

        # Draw the hand skeleton on the frame
        mp_drawing.draw_landmarks(
            frame,
            hand_landmarks,
            mp_hands.HAND_CONNECTIONS,
            mp_drawing.DrawingSpec(color=(121, 22, 76), thickness=2, circle_radius=4),
            mp_drawing.DrawingSpec(color=(250, 44, 250), thickness=2, circle_radius=2),
        )

        # Get the number of fingers up
        finger_count = get_finger_count(hand_landmarks, frame.shape)
        gesture_text = f"{finger_count} Fingers"

        # Get the horizontal position of the wrist (landmark 0)
        # Frame width is used to determine if the hand is on the left or right side
        wrist_x = hand_landmarks.landmark[0].x * frame.shape[1]
        center_x = frame.shape[1] / 2
        
        # Determine direction based on hand position
        is_right_side = wrist_x > center_x

        # Map finger count to servo commands
        if finger_count == 1: # Control Servo 1 (Base)
            command_to_send = 'q' if is_right_side else 'a'
        elif finger_count == 0: # Control Servo 2
            command_to_send = 'e' if is_right_side else 'd'
        elif finger_count == 3: # Control Servo 3
            command_to_send = 'w' if is_right_side else 's'
        elif finger_count == 4: # Control Servo 4
            command_to_send = 'r' if is_right_side else 'f'
        elif finger_count == 5: # Control Servo 5
            command_to_send = 't' if is_right_side else 'g'
        elif finger_count == 2: # Fist gesture for Servo 6 (Gripper)
            command_to_send = 'y' if is_right_side else 'h'

        if command_to_send:
            send_command(command_to_send)
            
        # Display direction indicator
        direction_text = "RIGHT (+)" if is_right_side else "LEFT (-)"
        cv2.putText(frame, direction_text, (int(center_x) - 50, 70), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)


    # Display the detected gesture on the screen
    cv2.putText(frame, gesture_text, (10, 50), cv2.FONT_HERSHEY_SIMPLEX, 1.5, (255, 0, 0), 3)

    # Show the frame
    frame = cv2.resize(frame, (1000, 800))  # Resize for better visibility
    cv2.imshow('Hand Gesture Robot Control', frame)

    # Check for 'q' key press to exit
    if cv2.waitKey(5) & 0xFF == ord('q'):
        break

# --- CLEANUP ---
print("Exiting program.")
hands.close()
cap.release()
cv2.destroyAllWindows()
ser.close()
