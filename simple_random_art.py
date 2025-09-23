#!/usr/bin/env python3
"""
Simple Random Art Visualizer - More compatible version
"""

import serial
import sys
import time

def simple_random_art(port="/dev/ttyUSB0", baudrate=115200):
    """Simple, robust random art visualizer"""
    
    # Simple character sets that work well in most terminals
    char_sets = [
        # Set 0: Blocks (density)
        [' ', '░', '▒', '▓', '█'],
        
        # Set 1: ASCII (classic)
        [' ', '.', '-', '+', '*', '#', '@'],
        
        # Set 2: Symbols
        ['○', '●', '◇', '◆', '□', '■'],
        
        # Set 3: Emojis (fun!)
        ['🎲', '🎯', '🎨', '⭐', '🔥', '💎', '🚀', '⚡', '🌟', '✨',
         '🟥', '🟧', '🟨', '🟩', '🟦', '🟪', '⬛', '⬜'],
        
        # Set 4: Binary
        ['0', '1'],
        
        # Set 5: Hex
        ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'],
    ]
    
    set_names = ['blocks', 'ascii', 'symbols', 'emoji', 'binary', 'hex']
    
    # Simple 8-color palette (more compatible)
    colors = [31, 32, 33, 34, 35, 36, 37, 91, 92, 93, 94, 95, 96, 97]  # Red, Green, Yellow, Blue, Magenta, Cyan, White + Bright versions
    
    print(f"🎨 Simple FlipperRNG Random Art")
    print(f"📡 Connecting to {port} at {baudrate} baud...")
    
    try:
        ser = serial.Serial(port, baudrate, timeout=2)
        print(f"✅ Connected! Starting art generation...")
        print(f"🎮 Auto-cycling through character sets every 30 frames")
        print(f"🔄 Press Ctrl+C to stop")
        print()
        
        frame_count = 0
        current_set = 0
        
        while True:
            # Read a chunk of random data
            chunk_size = 400  # Smaller chunks for better responsiveness
            data = ser.read(chunk_size)
            
            if len(data) < 10:  # Need some data to work with
                print("⏳ Waiting for data...")
                time.sleep(0.5)
                continue
            
            # Change character set every 30 frames
            if frame_count % 30 == 0:
                current_set = (current_set + 1) % len(char_sets)
                print(f"\n🎭 Switching to: {set_names[current_set]}")
                time.sleep(1)
            
            # Clear screen
            print('\033[2J\033[H', end='')
            
            # Header
            print(f"🎲 FlipperRNG Random Art | Frame: {frame_count} | Set: {set_names[current_set]}")
            print("─" * 70)
            
            # Generate art with emoji width compensation
            chars = char_sets[current_set]
            base_width = 70
            height = 12
            
            # Adjust width for emoji double-width
            if set_names[current_set] == 'emoji':
                width = base_width // 2  # Emojis are double-width
            else:
                width = base_width
            
            for y in range(height):
                line = ""
                for x in range(width):
                    byte_idx = (y * width + x) % len(data)
                    if byte_idx < len(data):
                        byte_val = data[byte_idx]
                        
                        # Simple color mapping
                        color = colors[byte_val % len(colors)]
                        char = chars[byte_val % len(chars)]
                        
                        line += f'\033[{color}m{char}\033[0m'
                    else:
                        line += ' '
                print(line)
            
            # Stats
            print("─" * 70)
            if data:
                avg_val = sum(data) / len(data)
                min_val = min(data)
                max_val = max(data)
                print(f"📊 Bytes: {len(data)} | Avg: {avg_val:.1f} | Min: {min_val} | Max: {max_val} | Range: {max_val-min_val}")
                print(f"🔢 Sample: {' '.join(f'{b:02X}' for b in data[:8])}...")
            
            frame_count += 1
            time.sleep(0.5)  # 500ms refresh for stability
            
    except serial.SerialException as e:
        print(f"❌ Connection error: {e}")
        print(f"💡 Make sure:")
        print(f"   - FlipperRNG is running and set to UART mode")
        print(f"   - UART is connected to {port}")
        print(f"   - Baud rate matches (try --baud 230400)")
        return 1
    except KeyboardInterrupt:
        print(f"\n🎨 Art stopped after {frame_count} frames")
        print(f"✨ Thanks for watching the entropy art!")
        return 0
    except Exception as e:
        print(f"❌ Error: {e}")
        return 1
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()

if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description="Simple FlipperRNG Random Art")
    parser.add_argument("--port", default="/dev/ttyUSB0", help="UART port")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    
    args = parser.parse_args()
    
    print("🎨 FlipperRNG Simple Random Art Visualizer")
    print("=" * 50)
    print("📋 Setup:")
    print("   1. Set FlipperRNG to UART output mode")
    print("   2. Start FlipperRNG generator")
    print("   3. Enjoy the random art show!")
    print("=" * 50)
    
    sys.exit(simple_random_art(args.port, args.baud))
