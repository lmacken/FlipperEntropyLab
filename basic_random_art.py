#!/usr/bin/env python3
"""
Basic Random Art - Simple ASCII visualization without complex formatting
"""

import serial
import sys
import time

def basic_random_art(port="/dev/ttyUSB0", baudrate=115200):
    """Very simple random art using basic ASCII and colors"""
    
    # Simple ASCII characters that work everywhere
    chars = [' ', '.', ':', ';', '!', '*', '#', '@']
    
    # Basic ANSI colors (8 colors, widely supported)
    colors = [31, 32, 33, 34, 35, 36, 37, 91]  # Red, Green, Yellow, Blue, Magenta, Cyan, White, Bright Red
    
    print(f"🎨 Basic FlipperRNG Random Art")
    print(f"📡 Connecting to {port} at {baudrate} baud...")
    
    try:
        ser = serial.Serial(port, baudrate, timeout=2)
        print(f"✅ Connected! Press Ctrl+C to stop")
        print()
        
        frame_count = 0
        
        while True:
            # Read data
            data = ser.read(200)  # Smaller chunk
            if len(data) < 50:
                print("⏳ Waiting for more data...")
                time.sleep(0.5)
                continue
            
            # Simple clear screen
            print('\n' * 50)  # Scroll instead of clear
            
            # Simple header
            print(f"Frame {frame_count} | Bytes: {len(data)}")
            print("-" * 60)
            
            # Simple grid (no complex formatting)
            width = 60
            height = 10
            
            for y in range(height):
                line = ""
                for x in range(width):
                    idx = (y * width + x) % len(data)
                    byte_val = data[idx]
                    
                    # Simple mapping
                    char = chars[byte_val % len(chars)]
                    color = colors[byte_val % len(colors)]
                    
                    # Simple color format
                    line += f'\033[{color}m{char}\033[0m'
                
                print(line)
            
            print("-" * 60)
            
            # Simple stats
            if data:
                avg = sum(data) / len(data)
                print(f"Avg: {avg:.1f} | Min: {min(data)} | Max: {max(data)}")
                print(f"Sample: {data[0]:02X} {data[1]:02X} {data[2]:02X} {data[3]:02X}")
            
            frame_count += 1
            time.sleep(1)  # 1 second refresh
            
    except KeyboardInterrupt:
        print(f"\n✅ Art stopped after {frame_count} frames")
        return 0
    except Exception as e:
        print(f"❌ Error: {e}")
        return 1
    finally:
        if 'ser' in locals():
            ser.close()

if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description="Basic FlipperRNG Random Art")
    parser.add_argument("--port", default="/dev/ttyUSB0", help="UART port")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    
    args = parser.parse_args()
    
    print("🎨 FlipperRNG Basic Random Art")
    print("=" * 40)
    print("Simple ASCII art from random data")
    print("No complex formatting - just works!")
    print("=" * 40)
    
    sys.exit(basic_random_art(args.port, args.baud))
