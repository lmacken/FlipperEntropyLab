#!/usr/bin/env python3
"""
Test script to verify SubGHz RSSI entropy collection is working.
This script monitors UART output for SubGHz debug messages.
"""

import serial
import time
import re
import sys

def test_subghz_entropy(port='/dev/ttyUSB0', baudrate=115200, timeout=30):
    """Test SubGHz RSSI entropy collection by monitoring debug logs."""
    
    print(f"🔍 Testing SubGHz RSSI Entropy Collection")
    print(f"📡 Monitoring {port} at {baudrate} baud for {timeout} seconds...")
    print(f"🎯 Looking for SubGHz RSSI debug messages")
    print()
    
    try:
        # Open serial connection
        ser = serial.Serial(port, baudrate, timeout=1)
        print(f"✅ Connected to {port}")
        
        # Statistics
        subghz_messages = 0
        rssi_readings = []
        frequencies_seen = set()
        entropy_values = []
        
        start_time = time.time()
        
        while time.time() - start_time < timeout:
            try:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    print(f"📝 {line}")
                    
                    # Look for SubGHz RSSI debug messages
                    if "SubGHz RSSI:" in line:
                        subghz_messages += 1
                        
                        # Extract frequency and RSSI values
                        freq_match = re.search(r'Freq=(\d+)', line)
                        rssi_match = re.search(r'RSSI=(-?\d+\.?\d*)', line)
                        lqi_match = re.search(r'LQI=(\d+)', line)
                        
                        if freq_match:
                            freq = int(freq_match.group(1))
                            frequencies_seen.add(freq)
                            
                        if rssi_match:
                            rssi = float(rssi_match.group(1))
                            rssi_readings.append(rssi)
                            
                        # Look for entropy collection
                        entropy_match = re.search(r'entropy=0x([0-9A-Fa-f]+)', line)
                        if entropy_match:
                            entropy = entropy_match.group(1)
                            entropy_values.append(entropy)
                    
                    # Look for worker messages about SubGHz
                    elif "bits_from_subghz_rssi" in line or "SubGHz" in line:
                        print(f"🔊 SubGHz related: {line}")
                        
            except UnicodeDecodeError:
                continue
                
        ser.close()
        
        # Print results
        print("\n" + "="*60)
        print("🧪 TEST RESULTS")
        print("="*60)
        
        if subghz_messages > 0:
            print(f"✅ SubGHz RSSI messages detected: {subghz_messages}")
            print(f"📡 Frequencies sampled: {len(frequencies_seen)}")
            for freq in sorted(frequencies_seen):
                print(f"   - {freq:,} Hz ({freq/1e6:.1f} MHz)")
            
            if rssi_readings:
                print(f"📊 RSSI readings: {len(rssi_readings)}")
                print(f"   - Min: {min(rssi_readings):.1f} dBm")
                print(f"   - Max: {max(rssi_readings):.1f} dBm")
                print(f"   - Avg: {sum(rssi_readings)/len(rssi_readings):.1f} dBm")
                
            if entropy_values:
                print(f"🎲 Entropy values collected: {len(entropy_values)}")
                print(f"   - Latest: 0x{entropy_values[-1]}")
                
            print("\n✅ SubGHz RSSI entropy collection is WORKING!")
            
        else:
            print("❌ No SubGHz RSSI messages detected")
            print("💡 Possible issues:")
            print("   - SubGHz source not enabled in configuration")
            print("   - Debug logging level too low")
            print("   - SubGHz hardware not available")
            print("   - Wrong UART port or settings")
            
        print("="*60)
        
    except serial.SerialException as e:
        print(f"❌ Serial connection error: {e}")
        print(f"💡 Make sure Flipper is connected to {port}")
        return False
    except KeyboardInterrupt:
        print("\n⏹️  Test interrupted by user")
        return False
        
    return subghz_messages > 0

if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description="Test SubGHz RSSI entropy collection")
    parser.add_argument("--port", default="/dev/ttyUSB0", help="Serial port (default: /dev/ttyUSB0)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--timeout", type=int, default=30, help="Test timeout in seconds (default: 30)")
    
    args = parser.parse_args()
    
    success = test_subghz_entropy(args.port, args.baud, args.timeout)
    sys.exit(0 if success else 1)
