#!/usr/bin/env python3
"""
Hex Art - Simple hexadecimal visualization of random data
"""

import serial
import sys
import time

def hex_art_monitor(port="/dev/ttyUSB0", baudrate=115200):
    """Simple hex art visualization"""
    
    print(f"🔢 FlipperRNG Hex Art Monitor")
    print(f"📡 Connecting to {port} at {baudrate} baud...")
    
    try:
        ser = serial.Serial(port, baudrate, timeout=2)
        print(f"✅ Connected! Generating hex art...")
        print(f"🎮 Press Ctrl+C to stop")
        print()
        
        frame_count = 0
        total_bytes = 0
        
        while True:
            # Read data
            data = ser.read(256)  # Read 256 bytes
            if len(data) < 16:
                print("⏳ Waiting for data...")
                time.sleep(0.5)
                continue
            
            total_bytes += len(data)
            
            # Clear and display
            print('\n' * 30)  # Simple scroll clear
            
            print(f"🎲 FlipperRNG Hex Art | Frame: {frame_count} | Total bytes: {total_bytes}")
            print("=" * 80)
            
            # Display as formatted hex grid
            for i in range(0, len(data), 16):
                chunk = data[i:i+16]
                
                # Hex representation
                hex_str = ' '.join(f'{b:02X}' for b in chunk)
                
                # ASCII representation (printable chars only)
                ascii_str = ''.join(chr(b) if 32 <= b <= 126 else '.' for b in chunk)
                
                # Address
                addr_str = f'{i:04X}'
                
                print(f"{addr_str}: {hex_str:<48} | {ascii_str}")
            
            print("=" * 80)
            
            # Statistics
            if data:
                avg_val = sum(data) / len(data)
                min_val = min(data)
                max_val = max(data)
                zeros = data.count(0)
                ones = data.count(255)
                
                print(f"📊 Stats: Avg={avg_val:.1f} Min={min_val} Max={max_val} Range={max_val-min_val}")
                print(f"🔍 Special: 0x00={zeros} 0xFF={ones} Entropy={(max_val-min_val)/255*100:.1f}%")
                
                # Simple randomness indicators
                byte_counts = {}
                for b in data:
                    byte_counts[b] = byte_counts.get(b, 0) + 1
                
                most_common = max(byte_counts.values())
                uniqueness = len(byte_counts) / 256 * 100
                
                print(f"🎯 Quality: Unique_bytes={len(byte_counts)}/256 ({uniqueness:.1f}%) Max_repeat={most_common}")
            
            frame_count += 1
            time.sleep(2)  # 2 second refresh
            
    except KeyboardInterrupt:
        print(f"\n✅ Hex art stopped. Total bytes processed: {total_bytes}")
        return 0
    except Exception as e:
        print(f"❌ Error: {e}")
        return 1
    finally:
        if 'ser' in locals():
            ser.close()

if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description="FlipperRNG Hex Art Monitor")
    parser.add_argument("--port", default="/dev/ttyUSB0", help="UART port")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    
    args = parser.parse_args()
    
    print("🔢 FlipperRNG Hex Art Visualizer")
    print("=" * 40)
    print("Clean hex dump with statistics")
    print("No ANSI complexity - just works!")
    print("=" * 40)
    
    sys.exit(hex_art_monitor(args.port, args.baud))
