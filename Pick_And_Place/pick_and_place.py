import serial
import time
import math

# --- Configuration ---
# IMPORTANT: Replace with your ESP32's serial port and baud rate.
# On Windows, it might be "COM3", "COM4", etc.
# On macOS or Linux, it might be "/dev/tty.usbserial-XXXX" or "/dev/ttyUSB0".
SERIAL_PORT = "COM4" 
BAUD_RATE = 115200

# IMPORTANT: Measure and update these values for your specific robotic arm (in cm).
L1 = 10.0  # Length of the base to the first joint (shoulder)
L2 = 13.0  # Length of the first joint to the second joint (elbow)
L3 = 10.0  # Length of the second joint to the gripper (wrist)

# Gripper open/close angles (adjust as needed)
GRIPPER_OPEN_ANGLE = 90
GRIPPER_CLOSED_ANGLE = 20

# --- NEW: Servo Angle Limits ---
BASE_MIN, BASE_MAX = 0, 180
SHOULDER_MIN, SHOULDER_MAX = 40, 170
ELBOW_MIN, ELBOW_MAX = 0, 180
WRIST_MIN, WRIST_MAX = 0, 160


# --- Serial Communication Setup ---
try:
    esp32 = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=2)
    time.sleep(2)  # Wait for the connection to establish
    print(f"Successfully connected to ESP32 on {SERIAL_PORT}")
except serial.SerialException as e:
    print(f"Error: Could not open serial port {SERIAL_PORT}. {e}")
    print("Please check the port name and ensure the ESP32 is connected.")
    exit()

def calculate_inverse_kinematics(x, y, z):
    """
    Calculates the joint angles for a 4-DOF robotic arm.
    This is a simplified geometric IK solver.
    Assumes the 4th DOF is to keep the gripper parallel to the ground.
    
    Args:
        x (float): Target X coordinate.
        y (float): Target Y coordinate.
        z (float): Target Z coordinate.

    Returns:
        tuple: A tuple of angles (theta1, theta2, theta3, theta4) in degrees,
               or None if the position is unreachable.
    """
    try:
        # --- Angle 1: Base rotation (theta1) ---
        theta1 = math.degrees(math.atan2(y, x))

        # --- Projecting into the 2D plane ---
        r = math.sqrt(x**2 + y**2)
        z_eff = z - L1

        # --- Angle 3: Elbow joint (theta3) ---
        D = math.sqrt(r**2 + z_eff**2)

        if D > (L2 + L3):
            print("Warning: Position is too far to reach.")
            return None

        alpha = math.acos((L2**2 + L3**2 - D**2) / (2 * L2 * L3))
        theta3 = 180 - math.degrees(alpha)

        # --- Angle 2: Shoulder joint (theta2) ---
        beta = math.acos((L2**2 + D**2 - L3**2) / (2 * L2 * D))
        gamma = math.atan2(z_eff, r)
        theta2 = math.degrees(gamma + beta)

        theta2 = 180 - math.degrees(gamma + beta)
        theta3 = 180 - theta3  # Adjusting theta3 to match the arm's configuration 

        # --- Angle 4: Wrist joint (theta4) ---
        theta4 = 180 - (theta2 + theta3)

        if theta4 < 0:
            theta4 += 90
        if theta1 < 0:
            theta1 += 180
        if theta2 < 0 :
            theta2 += 90
        # elif theta2 > 90:
        #     theta2 = 180 - theta2
        if theta3 < 0:
            theta3 += 90
      # Ensure theta4 is positive

        # --- MODIFIED: Angle Validation with new limits ---
        # Check if the calculated angles are within the physical limits of the servos.
        if not (BASE_MIN <= theta1 <= BASE_MAX):
            print(f"Warning: Base angle ({theta1:.2f}) out of range [{BASE_MIN}, {BASE_MAX}].")
            return None
        if not (SHOULDER_MIN <= theta2 <= SHOULDER_MAX):
            print(f"Warning: Shoulder angle ({theta2:.2f}) out of range [{SHOULDER_MIN}, {SHOULDER_MAX}].")
            return None
        if not (ELBOW_MIN <= theta3 <= ELBOW_MAX):
            print(f"Warning: Elbow angle ({theta3:.2f}) out of range [{ELBOW_MIN}, {ELBOW_MAX}].")
            return None
        if not (WRIST_MIN <= theta4 <= WRIST_MAX):
            print(f"Warning: Wrist angle ({theta4:.2f}) out of range [{WRIST_MIN}, {WRIST_MAX}].")
            return None

        return (theta1, theta2, theta3, theta4)

    except ValueError as e:
        print(f"Error during calculation: {e}. Position may be unreachable.")
        return None


def send_angles_to_esp32(angles, gripper_angle):
    """
    Formats and sends the calculated angles to the ESP32.
    Format: "<theta1,theta2,theta3,theta4,gripper_angle>"
    """
    if angles is None:
        print("Cannot send angles, calculation failed.")
        return

    t1, t2, t3, t4 = map(int, angles)
    g_angle = int(gripper_angle)
    
    command = f"<{t1},{t2},{t3},{t4},{g_angle}>"
    
    esp32.write(command.encode('utf-8'))
    print(f"Sent command: {command}")

    response = esp32.readline().decode('utf-8').strip()
    print(f"ESP32 Response: {response}")


def main_loop():
    """
    Main operational loop for the controller.
    """
    print("\n--- Robotic Arm Controller ---")
    print("Enter the target coordinates (x, y, z) in cm.")
    print("Example: 20 5 10")
    print("Type 'exit' to quit.")

    while True:
        try:
            user_input = input("\nEnter coordinates (x y z): ")
            if user_input.lower() == 'exit':
                break

            parts = user_input.split()
            if len(parts) != 3:
                print("Invalid input. Please enter three numbers separated by spaces.")
                continue

            x, y, z = map(float, parts)
            
            print("\nCalculating path to target...")
            
            target_angles = calculate_inverse_kinematics(x, y, z)
            if target_angles is None:
                print("--- Calculation failed. Ready for next command. ---")
                continue 
            
            print("Step 1: Moving to target position...")
            send_angles_to_esp32(target_angles, GRIPPER_OPEN_ANGLE)
            time.sleep(15) 

            print("Step 2: Closing gripper...")
            send_angles_to_esp32(target_angles, GRIPPER_CLOSED_ANGLE)
            time.sleep(2) 

            print("Step 3: Moving to home position...")
            # A safe home position respecting the new constraints
            home_angles = (90, 90, 90, 90) 
            send_angles_to_esp32(home_angles, GRIPPER_CLOSED_ANGLE)
            time.sleep(5)

            print("\n--- Pick and Place Sequence Complete ---")
            print("Ready for next command.")


        except ValueError:
            print("Invalid input. Please enter numbers only.")
        except KeyboardInterrupt:
            break
        except Exception as e:
            print(f"An unexpected error occurred: {e}")

    print("\nClosing serial connection.")
    esp32.close()


if __name__ == "__main__":
    main_loop()
