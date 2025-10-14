import serial
import time 
import math

SERIAL_PORT = "COM4" 
BAUD_RATE = 115200

GRIPPER_OPEN_ANGLE = 90
GRIPPER_CLOSED_ANGLE = 0

try:
    esp32 = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=2)
    time.sleep(2)  # Wait for the connection to establish
    print(f"Successfully connected to ESP32 on {SERIAL_PORT}")
except serial.SerialException as e:
    print(f"Error: Could not open serial port {SERIAL_PORT}. {e}")
    print("Please check the port name and ensure the ESP32 is connected.")
    exit()


# x, y= int(input("Enter the coordinates of the Object"))

home_pos = [90,60,100,40,80]


Presets = {0:[90,60,100,40,80],
           7:[90,120,0,40,0],
           8:[90,125,15,40,15],
           9:[90,125,25,40,30],
           10:[90, 140, 50, 40, 30],           
           11:[90,150,80,40,60]}
dist_goable = Presets.keys()

place_pos = [50,130,50,40,30]
place_pos[1] = place_pos[1] - 30
z_max = 10
r_max = 11
r_min = 7
z_min = 3.


def calculate_angles(x,y):
    base_angle = int(math.atan2(y,x) * 180 / math.pi)
    r = math.sqrt(x**2 + y**2)
    if r < 7 or r > 11:
        print("Out of range")
        return None
    else:
        preset_lenght = math.inf
        preset_id = 0
        for i in dist_goable:
            closest_dist = abs(r - i)
            if closest_dist <= preset_lenght:
                preset_lenght = closest_dist 
                preset_id = i
        Std_Angles = Presets[preset_id]
        shoulder_angle = Std_Angles[1]
        Elbow_angle = Std_Angles[2]
        Wrist_yaw_angle = Std_Angles[3]
        wrist_angle = Std_Angles[4]
        return [base_angle,shoulder_angle,Elbow_angle,Wrist_yaw_angle,wrist_angle]


def pnp(pick_pos, place_pos, inter_pos):
    send_angles_to_esp32(home_pos,GRIPPER_OPEN_ANGLE)
    send_angles_to_esp32(pick_pos,GRIPPER_CLOSED_ANGLE)
    send_angles_to_esp32(inter_pos,GRIPPER_CLOSED_ANGLE)
    time.sleep(1)
    send_angles_to_esp32(place_pos,GRIPPER_OPEN_ANGLE)
    send_angles_to_esp32(home_pos,GRIPPER_OPEN_ANGLE)
    return None



def send_angles_to_esp32(angles, gripper_angle):
    """
    Formats and sends the calculated angles to the ESP32.
    Format: "<theta1,theta2,theta3,theta4,theta5,gripper_angle>"
    """
    if angles is None:
        print("Cannot send angles, calculation failed.")
        return

    t1, t2, t3, t4, t5 = map(int, angles)
    g_angle = int(gripper_angle)
    
    command = f"<{t1},{t2},{t3},{t4},{t5},{g_angle}>"
    
    esp32.write(command.encode('utf-8'))
    print(f"Sent command: {command}")

    response = esp32.readline().decode('utf-8').strip()
    print(f"ESP32 Response: {response}")

def main_loop():
    """
    Main operational loop for the controller.
    """
    print("\n--- Robotic Arm Controller ---")
    print("Enter the target coordinates (x, y) in cm.")
    print("Example: 20 5 10")
    print("Type 'exit' to quit.")

    while True:
        try:
            user_input = input("\nEnter coordinates (x y): ")
            if user_input.lower() == 'exit':
                break

            parts = user_input.split()
            if len(parts) != 2:
                print("Invalid input. Please enter two numbers separated by spaces.")
                continue

            x, y = map(int, parts)
            
            print("\nCalculating path to target...")

            
            pick_pos = calculate_angles(x, y)
    
            inter_pos = [pick_pos[0], pick_pos[1] - 40, pick_pos[2], 40, pick_pos[4]]
            if pick_pos is None:
                print("--- Out of range. Ready for next command. ---")
                continue
            else:
                pnp(pick_pos,place_pos,inter_pos) 



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


    
    

