#!/usr/bin/env python3
"""
FlipperRNG Debug Logs Monitor
Monitors Flipper Zero debug logs for FlipperRNG entropy collection messages
"""

import serial
import serial.tools.list_ports
import sys
import time
import argparse
import re

def find_flipper_ports():
    """Find all potential Flipper Zero serial ports"""
    ports = []
    
    for port in serial.tools.list_ports.comports():
        # Check for Flipper Zero identifiers
        if any(keyword in (port.description or "").lower() for keyword in ["flipper", "stm32"]):
            ports.append({
                'device': port.device,
                'description': port.description,
                'type': 'CLI' if 'STM32' in (port.description or "") else 'CDC'
            })
        
        # Check VID/PID for Flipper Zero
        if port.vid == 0x0483 and port.pid == 0x5740:
            ports.append({
                'device': port.device, 
                'description': f"Flipper Zero (VID:PID {port.vid:04X}:{port.pid:04X})",
                'type': 'CLI'
            })
    
    return ports

def monitor_debug_logs(port, baudrate=230400, timeout=60):
    """Monitor debug logs from Flipper Zero CLI interface"""
    
    print(f"🔍 FlipperRNG Debug Monitor")
    print(f"📡 Connecting to {port} at {baudrate} baud")
    print(f"⏱️  Monitoring for {timeout} seconds")
    print(f"🎯 Looking for FlipperRNG debug messages")
    print("-" * 60)
    
    try:
        # Connect to Flipper CLI
        ser = serial.Serial(port, baudrate, timeout=1)
        print(f"✅ Connected to {port}")
        
        # Clear any pending data
        ser.read_all()
        time.sleep(0.5)
        
        # Send commands to enable logging
        print("📝 Enabling debug logging...")
        ser.write(b'\r\n')
        time.sleep(0.2)
        ser.write(b'log\r\n')  # Start log command
        time.sleep(0.5)
        
        # Statistics
        flipper_rng_messages = 0
        subghz_messages = 0
        nfc_messages = 0
        worker_messages = 0
        entropy_values = []
        
        start_time = time.time()
        
        print("🔊 Debug log output:")
        print("-" * 60)
        
        while time.time() - start_time < timeout:
            try:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    # Filter for FlipperRNG related messages
                    if any(keyword in line for keyword in ["FlipperRNG", "SubGHz RSSI", "NFC Field", "Worker"]):
                        print(f"🎲 {line}")
                        flipper_rng_messages += 1
                        
                        # Count specific message types
                        if "SubGHz RSSI" in line:
                            subghz_messages += 1
                            
                            # Extract RSSI values
                            rssi_match = re.search(r'RSSI=(-?\d+\.?\d*)', line)
                            if rssi_match:
                                rssi = float(rssi_match.group(1))
                                print(f"   📡 RSSI: {rssi:.1f} dBm")
                        
                        elif "NFC Field" in line:
                            nfc_messages += 1
                            
                        elif "Worker" in line:
                            worker_messages += 1
                            
                        # Extract entropy values
                        entropy_match = re.search(r'entropy=0x([0-9A-Fa-f]+)', line)
                        if entropy_match:
                            entropy = entropy_match.group(1)
                            entropy_values.append(entropy)
                            print(f"   🎯 Entropy: 0x{entropy}")
                    
                    # Also show other interesting log messages
                    elif any(keyword in line for keyword in ["ERROR", "WARN", "CRASH", "Assert"]):
                        print(f"⚠️  {line}")
                        
            except UnicodeDecodeError:
                continue
                
        ser.close()
        
        # Print summary
        print("\n" + "="*60)
        print("📊 DEBUG LOG SUMMARY")
        print("="*60)
        
        if flipper_rng_messages > 0:
            print(f"✅ FlipperRNG messages detected: {flipper_rng_messages}")
            print(f"📡 SubGHz RSSI messages: {subghz_messages}")
            print(f"📶 NFC Field messages: {nfc_messages}")
            print(f"⚙️  Worker messages: {worker_messages}")
            
            if entropy_values:
                print(f"🎲 Entropy values collected: {len(entropy_values)}")
                print(f"   Latest: 0x{entropy_values[-1]}")
                
            print("\n✅ FlipperRNG debug logging is WORKING!")
            
        else:
            print("❌ No FlipperRNG debug messages detected")
            print("\n💡 Troubleshooting:")
            print("   1. Make sure FlipperRNG app is running")
            print("   2. Check if debug logging is enabled in firmware")
            print("   3. Try different baud rates (115200, 230400)")
            print("   4. Verify correct serial port connection")
            print("   5. Check if using CLI interface (not UART output)")
            
        return flipper_rng_messages > 0
        
    except serial.SerialException as e:
        print(f"❌ Connection error: {e}")
        return False
    except KeyboardInterrupt:
        print("\n⏹️  Monitoring stopped by user")
        return False

def main():
    parser = argparse.ArgumentParser(description="Monitor FlipperRNG debug logs")
    parser.add_argument("--port", help="Serial port (auto-detect if not specified)")
    parser.add_argument("--baud", type=int, default=230400, help="Baud rate (default: 230400)")
    parser.add_argument("--timeout", type=int, default=60, help="Monitor timeout in seconds")
    parser.add_argument("--list-ports", action="store_true", help="List available serial ports")
    
    args = parser.parse_args()
    
    if args.list_ports:
        print("🔍 Available serial ports:")
        ports = find_flipper_ports()
        if ports:
            for i, port in enumerate(ports):
                print(f"  {i+1}. {port['device']} - {port['description']} ({port['type']})")
        else:
            print("  No Flipper Zero ports detected")
        return
    
    # Auto-detect port if not specified
    if not args.port:
        ports = find_flipper_ports()
        if ports:
            args.port = ports[0]['device']
            print(f"🔍 Auto-detected Flipper port: {args.port}")
        else:
            print("❌ No Flipper Zero ports found. Use --list-ports to see available ports.")
            print("💡 Try specifying port manually with --port /dev/ttyACM0")
            return
    
    # Try different baud rates if connection fails
    baud_rates = [args.baud, 230400, 115200, 9600]
    
    for baud in baud_rates:
        print(f"\n🔄 Trying {args.port} at {baud} baud...")
        try:
            if monitor_debug_logs(args.port, baud, args.timeout):
                break
        except Exception as e:
            print(f"❌ Failed with {baud} baud: {e}")
            continue
    else:
        print("\n❌ Could not establish connection with any baud rate")
        print("💡 Make sure:")
        print("   - Flipper Zero is connected via USB")
        print("   - FlipperRNG app is running")
        print("   - Debug logging is enabled")

if __name__ == "__main__":
    main()
