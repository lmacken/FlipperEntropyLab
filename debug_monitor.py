#!/usr/bin/env python3
"""
FlipperRNG Debug Monitor
Connects to Flipper Zero and monitors debug logs
"""

import serial
import sys
import time
import argparse

def find_flipper_port():
    """Try to auto-detect Flipper Zero serial port"""
    import serial.tools.list_ports
    
    for port in serial.tools.list_ports.comports():
        if "Flipper" in port.description or "STM32" in port.description:
            return port.device
        # Common VID/PID for Flipper Zero
        if port.vid == 0x0483 and port.pid == 0x5740:
            return port.device
    
    # Default fallbacks
    import platform
    if platform.system() == "Linux":
        return "/dev/ttyACM0"
    elif platform.system() == "Windows":
        return "COM3"
    elif platform.system() == "Darwin":  # macOS
        return "/dev/tty.usbmodem*"
    
    return None

def monitor_logs(port, baudrate=230400):
    """Connect to Flipper and monitor logs"""
    print(f"Connecting to {port} at {baudrate} baud...")
    
    try:
        ser = serial.Serial(port, baudrate, timeout=1)
        print("Connected! Sending log command...")
        
        # Clear any pending data
        ser.read_all()
        
        # Send log command
        ser.write(b'\r\n')
        time.sleep(0.1)
        ser.write(b'log\r\n')
        time.sleep(0.1)
        
        print("Monitoring logs (Ctrl+C to exit)...")
        print("-" * 60)
        
        # Read logs continuously
        buffer = ""
        while True:
            if ser.in_waiting:
                data = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
                buffer += data
                
                # Process complete lines
                while '\n' in buffer:
                    line, buffer = buffer.split('\n', 1)
                    line = line.strip()
                    
                    # Highlight FlipperRNG logs
                    if "FlipperRNG" in line:
                        if "[E]" in line:
                            print(f"\033[91m{line}\033[0m")  # Red for errors
                        elif "[W]" in line:
                            print(f"\033[93m{line}\033[0m")  # Yellow for warnings
                        elif "[I]" in line:
                            print(f"\033[92m{line}\033[0m")  # Green for info
                        else:
                            print(f"\033[96m{line}\033[0m")  # Cyan for our app
                    else:
                        print(line)
            else:
                time.sleep(0.01)
                
    except serial.SerialException as e:
        print(f"Error: {e}")
        print("\nTroubleshooting:")
        print("1. Check if Flipper is connected via USB")
        print("2. Close qFlipper if it's running")
        print("3. Try: sudo chmod 666 /dev/ttyACM0 (Linux)")
        print("4. Install pyserial: pip install pyserial")
        return 1
    except KeyboardInterrupt:
        print("\n\nStopping log monitor...")
        ser.close()
        return 0
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()

def main():
    parser = argparse.ArgumentParser(description='Monitor FlipperRNG debug logs')
    parser.add_argument('-p', '--port', help='Serial port (auto-detect if not specified)')
    parser.add_argument('-b', '--baud', type=int, default=230400, help='Baud rate (default: 230400)')
    parser.add_argument('--list-ports', action='store_true', help='List available serial ports')
    
    args = parser.parse_args()
    
    if args.list_ports:
        import serial.tools.list_ports
        print("Available serial ports:")
        for port in serial.tools.list_ports.comports():
            print(f"  {port.device}: {port.description}")
        return 0
    
    # Determine port
    port = args.port
    if not port:
        port = find_flipper_port()
        if not port:
            print("Could not auto-detect Flipper Zero port.")
            print("Please specify with -p option or connect Flipper Zero via USB")
            print("Use --list-ports to see available ports")
            return 1
        print(f"Auto-detected port: {port}")
    
    return monitor_logs(port, args.baud)

if __name__ == "__main__":
    sys.exit(main())
