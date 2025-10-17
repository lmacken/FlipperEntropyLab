#!/usr/bin/env python3
"""
Entropy Lab Visualizer - Smooth, flickering-free visualization of random data streams
"""

import serial
import sys
import time
import os
import select
import termios
import tty
import threading
import unicodedata

def get_terminal_size():
    """Get terminal dimensions"""
    try:
        rows, cols = os.popen('stty size', 'r').read().split()
        return int(rows), int(cols)
    except:
        return 24, 80  # Default fallback

def clear_screen():
    """Clear screen without flickering"""
    print('\033[2J\033[H', end='', flush=True)

def move_cursor(row, col):
    """Move cursor to specific position"""
    print(f'\033[{row};{col}H', end='', flush=True)

def get_display_width(text):
    """Get the actual display width of text (handling emojis and ANSI codes properly)"""
    import re
    # Remove ANSI escape codes before measuring
    ansi_escape = re.compile(r'\x1b\[[0-9;]*m')
    clean_text = ansi_escape.sub('', text)
    
    width = 0
    for char in clean_text:
        # Most emojis are 2 columns, ASCII is 1
        if ord(char) > 0x1F000:  # Emoji range
            width += 2
        elif unicodedata.east_asian_width(char) in ('F', 'W'):
            width += 2
        else:
            width += 1
    return width

def pad_to_width(text, target_width):
    """Pad text to exact display width, accounting for emoji widths"""
    current_width = get_display_width(text)
    if current_width < target_width:
        return text + ' ' * (target_width - current_width)
    elif current_width > target_width:
        # Truncate to fit (remove characters from the end)
        while get_display_width(text) > target_width and len(text) > 0:
            text = text[:-1]
        # Pad if we removed too much
        return pad_to_width(text, target_width)
    return text

def smooth_emoji_art(port="/dev/ttyUSB0", baudrate=115200):
    """Smooth emoji art with no flickering"""
    
    # Get terminal size
    term_rows, term_cols = get_terminal_size()
    
    # ANSI color codes for vibrant blocks
    RESET = '\033[0m'
    colors = [
        '\033[48;5;196m',  # Bright Red
        '\033[48;5;208m',  # Orange
        '\033[48;5;226m',  # Yellow
        '\033[48;5;46m',   # Bright Green
        '\033[48;5;51m',   # Cyan
        '\033[48;5;21m',   # Blue
        '\033[48;5;201m',  # Magenta
        '\033[48;5;213m',  # Pink
        '\033[48;5;165m',  # Purple
        '\033[48;5;93m',   # Dark Purple
        '\033[48;5;33m',   # Deep Blue
        '\033[48;5;39m',   # Sky Blue
        '\033[48;5;48m',   # Turquoise
        '\033[48;5;118m',  # Lime
        '\033[48;5;184m',  # Gold
        '\033[48;5;202m',  # Dark Orange
    ]
    
    # Create colorful blocks (each block is 2 chars wide to match emoji width)
    colorful_blocks = [f'{color}  {RESET}' for color in colors]
    
    # Grayscale gradient blocks (doubled for emoji width)
    grayscale = ['██', '▓▓', '▒▒', '░░', '  ']
    
    # Emoji collections
    emoji_sets = {
        'blocks': colorful_blocks,
        'gray': grayscale,
        'faces': ['😀', '😃', '😄', '😁', '😆', '😅', '🤣', '😂', '🙂', '🙃', '😉', '😊', '😇', '🥰', '😍', '🤩'],
        'animals': ['🐶', '🐱', '🐭', '🐹', '🐰', '🦊', '🐻', '🐼', '🐨', '🐯', '🦁', '🐮', '🐷', '🐸', '🐵', '🐔'],
        'nature': ['🌸', '🌺', '🌻', '🌷', '🌹', '🌿', '🍀', '🌱', '🌲', '🌳', '🌴', '🌵', '🍄', '🦋', '🐝', '🐞'],
        'space': ['🚀', '🛸', '⭐', '🌟', '💫', '✨', '🌠', '☄', '🪐', '🌌', '🌙', '☀', '🌞', '🌍', '🌎', '🌏'],
        'food': ['🍎', '🍏', '🍊', '🍋', '🍌', '🍉', '🍇', '🍓', '🍈', '🍒', '🥭', '🍍', '🥥', '🍅', '🥑', '🥦'],
    }
    
    current_set = 'blocks'
    frame_count = 0
    paused = False
    
    # Calculate display dimensions
    # Most emojis are 2-chars wide, so divide by 2
    canvas_width = min(35, (term_cols - 10) // 2)  # Account for emoji double-width
    canvas_height = min(15, term_rows - 8)  # Leave space for UI
    
    print(f"🎨 Entropy Lab Visualizer")
    print(f"📡 Connecting to {port} at {baudrate} baud...")
    print(f"🖼️  Canvas: {canvas_width}x{canvas_height} (Terminal: {term_cols}x{term_rows})")
    
    try:
        ser = serial.Serial(port, baudrate, timeout=2)
        print(f"✅ Connected! Starting smooth emoji display...")
        time.sleep(2)
        
        # Set up non-blocking keyboard input
        old_settings = termios.tcgetattr(sys.stdin)
        tty.setraw(sys.stdin.fileno())
        
        # Initial screen setup
        clear_screen()
        
        try:
            while True:
                # Check for keyboard input (non-blocking)
                if sys.stdin in select.select([sys.stdin], [], [], 0)[0]:
                    key = sys.stdin.read(1).lower()
                    
                    if key == 'q':
                        break
                    elif key == ' ':
                        paused = not paused
                        # Show pause status
                        move_cursor(term_rows - 2, 1)
                        if paused:
                            print("⏸️  PAUSED - Press Space to resume" + " " * 20, flush=True)
                        else:
                            print("▶️  RESUMED" + " " * 30, flush=True)
                        time.sleep(0.5)
                    elif key in '123456789':
                        # Change theme
                        theme_keys = list(emoji_sets.keys())
                        theme_index = int(key) - 1
                        if 0 <= theme_index < len(theme_keys):
                            current_set = theme_keys[theme_index]
                            # Show theme change
                            move_cursor(term_rows - 2, 1)
                            print(f"🎭 Changed to: {current_set.upper()}" + " " * 20, flush=True)
                            time.sleep(0.3)
                
                if paused:
                    time.sleep(0.1)
                    continue
                
                # Read data
                data = ser.read(canvas_width * canvas_height)
                if len(data) < canvas_width * canvas_height:
                    # Not enough data, wait
                    move_cursor(term_rows - 2, 1)
                    print("⏳ Waiting for more data...", end='', flush=True)
                    time.sleep(0.5)
                    continue
                
                frame_count += 1
                emojis = emoji_sets[current_set]
                
                # Calculate centering
                start_row = (term_rows - canvas_height - 4) // 2
                # Emoji display width: each emoji is 2 terminal columns
                emoji_display_width = canvas_width * 2
                start_col = max(1, (term_cols - emoji_display_width - 2) // 2)
                
                # Draw top border with exact width
                move_cursor(start_row, start_col)
                # Border width = 1 (left) + emoji_display_width + 1 (right)
                border_content_width = emoji_display_width
                print("╔" + "═" * border_content_width + "╗", flush=True)
                
                # Draw title (proper centering)
                move_cursor(start_row + 1, start_col)
                title_text = f"Entropy Lab - {current_set.upper()} - Frame {frame_count}"
                # Truncate if too long (leave room for borders)
                max_title_width = border_content_width - 2  # Account for border chars
                if len(title_text) > max_title_width:
                    title_text = title_text[:max_title_width-3] + "..."
                
                # Center the text within the border
                padding_left = (border_content_width - len(title_text)) // 2
                padding_right = border_content_width - len(title_text) - padding_left
                print(f"║{' ' * padding_left}{title_text}{' ' * padding_right}║", flush=True)
                
                # Draw separator
                move_cursor(start_row + 2, start_col)
                print("╠" + "═" * border_content_width + "╣", flush=True)
                
                # Draw emoji canvas (perfect border alignment)
                for y in range(canvas_height):
                    move_cursor(start_row + 3 + y, start_col)
                    print("║", end='', flush=True)
                    
                    line = ""
                    for x in range(canvas_width):
                        idx = (y * canvas_width + x) % len(data)
                        byte_val = data[idx]
                        emoji = emojis[byte_val % len(emojis)]
                        line += emoji
                    
                    # Ensure line is exactly the right width before adding border
                    line = pad_to_width(line, emoji_display_width)
                    print(line + "║", flush=True)
                
                # Draw bottom border (perfectly aligned)
                move_cursor(start_row + 3 + canvas_height, start_col)
                print("╚" + "═" * border_content_width + "╝", flush=True)
                
                # Draw stats below (no flickering)
                stats_row = start_row + 5 + canvas_height
                move_cursor(stats_row, start_col)
                
                if data:
                    avg_val = sum(data) / len(data)
                    min_val = min(data)
                    max_val = max(data)
                    unique = len(set(data))
                    
                    stats_text = f"📊 Avg: {avg_val:.1f} | Range: {max_val-min_val} | Unique: {unique}/256 | Frames: {frame_count}"
                    print(stats_text + " " * 20, flush=True)  # Clear any leftover text
                
                # Draw controls with theme list (bottom of screen)
                move_cursor(term_rows - 1, 1)
                theme_names = list(emoji_sets.keys())
                controls_text = f"🎮 Themes: "
                for i, theme in enumerate(theme_names, 1):
                    marker = f"[{i}]" if theme == current_set else f" {i} "
                    controls_text += f"{marker}{theme} "
                controls_text += "| Space=pause | Q=quit"
                print(controls_text[:term_cols-1] + " " * max(0, term_cols - len(controls_text) - 1), end='', flush=True)
                
                time.sleep(0.4)  # Smooth refresh rate
            
        except KeyboardInterrupt:
            pass  # Clean exit
        finally:
            # Restore terminal settings
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old_settings)
            
    except KeyboardInterrupt:
        pass
    except Exception as e:
        print(f"❌ Error: {e}")
        return 1
    finally:
        if 'ser' in locals():
            ser.close()
        
        # Clean exit
        clear_screen()
        print(f"🎨 Entropy Lab visualization ended after {frame_count} frames!")
        print("✨ Thanks for exploring the entropy!")
        return 0

if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description="Entropy Lab - Visualize random data streams")
    parser.add_argument("--port", default="/dev/ttyUSB0", help="UART port")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    
    args = parser.parse_args()
    
    print("🎨 Entropy Lab Visualizer")
    print("=" * 40)
    print("✨ Beautiful borders and centering")
    print("🚫 No flickering or overlap issues")
    print("🎯 Smooth cursor-based updates")
    print("=" * 40)
    
    sys.exit(smooth_emoji_art(args.port, args.baud))
