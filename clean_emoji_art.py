#!/usr/bin/env python3
"""
Clean Emoji Art - No ANSI colors, just pure emoji randomness
"""

import serial
import sys
import time

def clean_emoji_art(port="/dev/ttyUSB0", baudrate=115200):
    """Simple emoji art without ANSI color complications"""
    
    # Mega emoji set - NO ANSI colors, just emojis
    emojis = [
        # Gaming & Fun
        '🎲', '🎯', '🎮', '🕹️', '🎰', '🎳', '🎪', '🎭', '🎨', '🎬', '🎤', '🎧', '🎸', '🥁', '🎺', '🎷', '🎻',
        # Faces (happy to crazy)
        '😀', '😃', '😄', '😁', '😆', '😅', '🤣', '😂', '🙂', '🙃', '😉', '😊', '😇', '🥰', '😍', '🤩', '😘', '😗', '😚', '😙',
        '🥲', '😋', '😛', '😜', '🤪', '😝', '🤑', '🤗', '🤭', '🤫', '🤔', '🤐', '🤨', '😐', '😑', '😶', '😏', '😒', '🙄', '😬',
        '🤥', '😌', '😔', '😪', '🤤', '😴', '😷', '🤒', '🤕', '🤢', '🤮', '🤧', '🥵', '🥶', '🥴', '😵', '🤯', '🤠', '🥳', '🥸',
        # Animals
        '🐶', '🐱', '🐭', '🐹', '🐰', '🦊', '🐻', '🐼', '🐨', '🐯', '🦁', '🐮', '🐷', '🐽', '🐸', '🐵', '🙈', '🙉', '🙊', '🐒',
        '🐔', '🐧', '🐦', '🐤', '🐣', '🐥', '🦆', '🦅', '🦉', '🦇', '🐺', '🐗', '🐴', '🦄', '🐝', '🐛', '🦋', '🐌', '🐞', '🐜',
        # Food
        '🍎', '🍏', '🍊', '🍋', '🍌', '🍉', '🍇', '🍓', '🍈', '🍒', '🍑', '🥭', '🍍', '🥥', '🥝', '🍅', '🍆', '🥑', '🥦', '🥒',
        '🌽', '🥕', '🧄', '🧅', '🥔', '🍠', '🥐', '🥖', '🍞', '🥨', '🥯', '🧀', '🥚', '🍳', '🥞', '🧇', '🥓', '🍗', '🍖', '🍔',
        # Nature & Weather
        '🌸', '🌺', '🌻', '🌷', '🌹', '🥀', '🌾', '🌿', '🍀', '🍃', '🌱', '🌲', '🌳', '🌴', '🌵', '🍄', '🌰', '☀️', '🌤️', '⛅',
        '🌦️', '🌧️', '⛈️', '🌩️', '🌨️', '❄️', '☃️', '⛄', '🌬️', '💨', '🌪️', '🌫️', '🌈', '⭐', '🌟', '💫', '✨', '🌠', '☄️', '🔥',
        # Objects & Symbols
        '💎', '💍', '👑', '🔮', '🎆', '🎇', '💥', '💢', '💦', '💤', '🔨', '🔧', '⚙️', '🔩', '🔗', '⛓️', '💰', '💴', '💵', '💶',
        '🏆', '🥇', '🥈', '🥉', '🏅', '🎖️', '🎗️', '🏵️', '🎀', '🎁', '🎊', '🎉', '🎈', '🎂', '🍰', '🧁', '🚀', '✈️', '🚗', '🏠'
    ]
    
    print(f"🎨 Clean Emoji Art Visualizer")
    print(f"📡 Connecting to {port} at {baudrate} baud...")
    print(f"🌈 Using {len(emojis)} different emojis!")
    
    try:
        ser = serial.Serial(port, baudrate, timeout=2)
        print(f"✅ Connected! Starting emoji chaos...")
        print(f"🎊 Press Ctrl+C to stop")
        print()
        
        frame_count = 0
        
        while True:
            # Read data
            data = ser.read(800)  # Read enough for a good display
            if len(data) < 100:
                print("⏳ Waiting for data...")
                time.sleep(0.5)
                continue
            
            # Simple clear (just newlines, no ANSI)
            print('\n' * 40)
            
            # Simple header (no fancy formatting)
            print(f"FlipperRNG Emoji Art - Frame {frame_count}")
            print(f"Emojis: {len(emojis)} | Bytes: {len(data)}")
            print("-" * 60)
            
            # Simple emoji grid (NO ANSI colors)
            width = 30  # Half width since emojis are double-wide
            height = 12
            
            for y in range(height):
                line = ""
                for x in range(width):
                    idx = (y * width + x) % len(data)
                    byte_val = data[idx]
                    
                    # Just emoji, no colors
                    emoji = emojis[byte_val % len(emojis)]
                    line += emoji
                
                print(line)  # Simple print, no ANSI codes
            
            print("-" * 60)
            
            # Simple stats
            if data:
                avg = sum(data) / len(data)
                print(f"Stats: Avg={avg:.1f} Min={min(data)} Max={max(data)} Range={max(data)-min(data)}")
                print(f"Sample bytes: {data[0]:02X} {data[1]:02X} {data[2]:02X} {data[3]:02X}")
            
            frame_count += 1
            time.sleep(0.8)  # Slower refresh for readability
            
    except KeyboardInterrupt:
        print(f"\n🎨 Emoji art stopped after {frame_count} frames")
        print("✨ Thanks for the emoji chaos!")
        return 0
    except Exception as e:
        print(f"❌ Error: {e}")
        return 1
    finally:
        if 'ser' in locals():
            ser.close()

if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description="Clean Emoji Art - No ANSI complexity")
    parser.add_argument("--port", default="/dev/ttyUSB0", help="UART port")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    
    args = parser.parse_args()
    
    print("🎨 FlipperRNG Clean Emoji Art")
    print("=" * 40)
    print("Pure emoji randomness - no ANSI colors")
    print("Simple and clean - guaranteed to work!")
    print("=" * 40)
    
    sys.exit(clean_emoji_art(args.port, args.baud))
