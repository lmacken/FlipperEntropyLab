#!/usr/bin/env python3
"""
Interactive FlipperRNG Debug Monitor
Provides interactive CLI access with FlipperRNG log highlighting
"""

import serial
import sys
import time
import threading
import argparse
import select

def find_flipper_port():
    """Try to auto-detect Flipper Zero serial port"""
    import serial.tools.list_ports
    
    for port in serial.tools.list_ports.comports():
        if "Flipper" in (port.description or "") or "STM32" in (port.description or ""):
            return port.device
        if port.vid == 0x0483 and port.pid == 0x5740:
            return port.device
    
    return "/dev/ttyACM0"  # Default fallback

def read_thread(ser):
    """Thread to read from serial port and display output"""
    buffer = ""
    
    while True:
        try:
            if ser.in_waiting:
                data = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
                buffer += data
                
                # Process complete lines
                while '\n' in buffer:
                    line, buffer = buffer.split('\n', 1)
                    line = line.strip()
                    
                    if line:  # Only print non-empty lines
                        # Highlight FlipperRNG logs
                        if "FlipperRNG" in line:
                            if "[E]" in line:
                                print(f"\033[91m{line}\033[0m")  # Red for errors
                            elif "[W]" in line:
                                print(f"\033[93m{line}\033[0m")  # Yellow for warnings
                            elif "[I]" in line:
                                print(f"\033[92m{line}\033[0m")  # Green for info
                            else:
                                print(f"\033[96m{line}\033[0m")  # Cyan for FlipperRNG
                        else:
                            print(line)
                        
                        # Flush output immediately
                        sys.stdout.flush()
            else:
                time.sleep(0.01)
                
        except Exception as e:
            print(f"Read error: {e}")
            break

def interactive_monitor(port, baudrate=230400):
    """Interactive CLI monitor with FlipperRNG log highlighting"""
    
    print(f"🔍 Interactive FlipperRNG Debug Monitor")
    print(f"📡 Connecting to {port} at {baudrate} baud")
    print(f"💡 Type 'log info' then 'log' to enable debug logging")
    print(f"🎯 FlipperRNG logs will be highlighted in color")
    print("-" * 60)
    
    try:
        # Connect to Flipper
        ser = serial.Serial(port, baudrate, timeout=0.1)
        print(f"✅ Connected to {port}")
        print(f"💬 You can now type CLI commands...")
        print(f"📝 Recommended: Type 'log info' then 'log' to enable logging")
        print("-" * 60)
        
        # Start read thread
        read_t = threading.Thread(target=read_thread, args=(ser,), daemon=True)
        read_t.start()
        
        # Send initial setup
        ser.write(b'\r\n')
        time.sleep(0.1)
        
        # Interactive input loop
        try:
            while True:
                # Check if input is available (non-blocking)
                if sys.stdin in select.select([sys.stdin], [], [], 0.1)[0]:
                    user_input = sys.stdin.readline().strip()
                    
                    if user_input.lower() in ['exit', 'quit', 'bye']:
                        break
                    
                    # Send user input to Flipper
                    ser.write(f"{user_input}\r\n".encode())
                    
                    # Special handling for log setup
                    if user_input == "log info":
                        print("🔧 Log level set to info")
                    elif user_input == "log":
                        print("📊 Log streaming enabled - FlipperRNG logs will appear when app runs")
                
        except KeyboardInterrupt:
            print("\n⏹️  Stopping interactive monitor...")
            
        ser.close()
        return 0
        
    except serial.SerialException as e:
        print(f"❌ Connection error: {e}")
        print(f"💡 Make sure Flipper is connected to {port}")
        print(f"💡 Close qFlipper if it's running")
        print(f"💡 Check permissions: sudo usermod -a -G dialout $USER")
        return 1
    except Exception as e:
        print(f"❌ Unexpected error: {e}")
        return 1

def main():
    parser = argparse.ArgumentParser(description="Interactive FlipperRNG Debug Monitor")
    parser.add_argument("--port", help="Serial port (auto-detect if not specified)")
    parser.add_argument("--baud", type=int, default=230400, help="Baud rate (default: 230400)")
    parser.add_argument("--list-ports", action="store_true", help="List available serial ports")
    
    args = parser.parse_args()
    
    if args.list_ports:
        import serial.tools.list_ports
        print("🔍 Available serial ports:")
        for port in serial.tools.list_ports.comports():
            print(f"  📡 {port.device}: {port.description}")
        return 0
    
    # Auto-detect port if not specified
    port = args.port or find_flipper_port()
    
    print("🎮 Interactive FlipperRNG Debug Monitor")
    print("=" * 50)
    print("📋 Quick Start:")
    print("   1. Type: log info")
    print("   2. Type: log") 
    print("   3. Launch FlipperRNG app on device")
    print("   4. Start generator")
    print("   5. Watch for colored FlipperRNG logs!")
    print("   6. Type 'exit' to quit")
    print("=" * 50)
    
    return interactive_monitor(port, args.baud)

if __name__ == "__main__":
    sys.exit(main())
