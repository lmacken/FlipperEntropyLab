#!/usr/bin/env python3
"""
Simple FlipperRNG ANSI Art - Auto-cycling character sets
"""

import serial
import sys
import time

def simple_ansi_art(port="/dev/ttyUSB0", baudrate=115200):
    """Simple ANSI art with auto-cycling character sets"""
    
    # ANSI colors and character sets
    fg_colors = list(range(30, 38)) + list(range(90, 98))  # 16 colors
    bg_colors = list(range(40, 48)) + list(range(100, 108))  # 16 colors
    
    char_sets = {
        'blocks': ['█', '▉', '▊', '▋', '▌', '▍', '▎', '▏', ' '],
        'patterns': ['░', '▒', '▓', '█', '▄', '▀', '▐', '▌'],
        'ascii': ['#', '@', '%', '*', '+', '=', '-', '.', ' '],
        'symbols': ['●', '○', '◆', '◇', '■', '□', '▲', '△'],
        'emoji': ['🎲', '🎯', '🎨', '🌈', '⭐', '🔥', '💎', '🚀', '⚡', '🌟', 
                 '🎭', '🎪', '🎊', '🎉', '🔮', '💫', '✨', '🌠', '🎆', '🎇',
                 '🟥', '🟧', '🟨', '🟩', '🟦', '🟪', '⬛', '⬜', '🔴', '🟠',
                 '🟡', '🟢', '🔵', '🟣', '🟤', '⚪', '⚫', '🔺', '🔻', '🔶'],
        'binary': ['0', '1'],
        'hex': list('0123456789ABCDEF'),
    }
    
    char_set_names = list(char_sets.keys())
    current_set_index = 0
    
    width, height = 80, 20
    
    print(f"🎨 Simple FlipperRNG ANSI Art")
    print(f"📡 Connecting to {port}...")
    
    try:
        ser = serial.Serial(port, baudrate, timeout=1)
        print(f"✅ Connected! Generating random art...")
        print(f"🎮 Press Ctrl+C to stop")
        print("-" * width)
        
        frame_count = 0
        set_change_interval = 50  # Change character set every 50 frames
        
        while True:
            # Read random data
            data = ser.read(width * height)
            if len(data) < width * height:
                continue
                
            # Change character set periodically
            if frame_count % set_change_interval == 0:
                current_set_index = (current_set_index + 1) % len(char_set_names)
                current_set_name = char_set_names[current_set_index]
                current_chars = char_sets[current_set_name]
                print(f"\n🎭 Character set: {current_set_name}")
                time.sleep(0.5)
            
            # Clear screen and render
            print('\033[2J\033[H', end='')  # Clear screen, go to top
            
            print(f"🎲 FlipperRNG Random Art | Frame: {frame_count} | Set: {char_set_names[current_set_index]}")
            print("─" * width)
            
            # Render art
            current_chars = char_sets[char_set_names[current_set_index]]
            for y in range(height - 3):
                line = ""
                for x in range(width):
                    byte_idx = y * width + x
                    if byte_idx < len(data):
                        byte_val = data[byte_idx]
                        
                        # Map byte to colors and character
                        fg_color = fg_colors[byte_val & 0x0F]
                        bg_color = bg_colors[(byte_val >> 4) & 0x0F]
                        character = current_chars[byte_val % len(current_chars)]
                        
                        line += f'\033[{fg_color};{bg_color}m{character}\033[0m'
                    else:
                        line += ' '
                print(line)
            
            # Stats
            if data:
                avg_val = sum(data) / len(data)
                min_val = min(data)
                max_val = max(data)
                print(f"📊 Avg: {avg_val:.1f} | Min: {min_val} | Max: {max_val} | Range: {max_val-min_val}")
            
            frame_count += 1
            time.sleep(0.3)  # 300ms refresh rate
            
    except serial.SerialException as e:
        print(f"❌ Error: {e}")
        print(f"💡 Make sure FlipperRNG is set to UART mode and connected to {port}")
        return 1
    except KeyboardInterrupt:
        print(f"\n🎨 Random art stopped after {frame_count} frames")
        return 0
    finally:
        if 'ser' in locals():
            ser.close()

if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description="Simple FlipperRNG ANSI Art")
    parser.add_argument("--port", default="/dev/ttyUSB0", help="UART port")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    
    args = parser.parse_args()
    
    print("🎨 FlipperRNG Simple ANSI Art Visualizer")
    print("=" * 50)
    print("📋 Instructions:")
    print("   1. Set FlipperRNG to UART output mode")
    print("   2. Start FlipperRNG generator")
    print("   3. Watch random art with auto-cycling character sets!")
    print("=" * 50)
    
    sys.exit(simple_ansi_art(args.port, args.baud))
