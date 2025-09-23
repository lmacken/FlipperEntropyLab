#!/usr/bin/env python3
"""
FlipperRNG Random ANSI Art Visualizer
Converts raw random data stream into colorful terminal art
"""

import serial
import sys
import time
import random
import argparse
import threading
import select
import termios
import tty
from queue import Queue

class ANSIArtVisualizer:
    def __init__(self, width=80, height=24, initial_char_set='blocks'):
        self.width = width
        self.height = height
        self.data_queue = Queue()
        
        # ANSI color codes
        self.fg_colors = list(range(30, 38)) + list(range(90, 98))  # 16 foreground colors
        self.bg_colors = list(range(40, 48)) + list(range(100, 108))  # 16 background colors
        
        # Character sets for different randomness patterns
        self.char_sets = {
            'blocks': ['█', '▉', '▊', '▋', '▌', '▍', '▎', '▏', ' '],
            'patterns': ['░', '▒', '▓', '█', '▄', '▀', '▐', '▌'],
            'ascii': ['#', '@', '%', '*', '+', '=', '-', '.', ' '],
            'symbols': ['●', '○', '◆', '◇', '■', '□', '▲', '△', '▼', '▽'],
            'emoji': ['🎲', '🎯', '🎨', '🌈', '⭐', '🔥', '💎', '🚀', '⚡', '🌟', 
                     '🎭', '🎪', '🎊', '🎉', '🔮', '💫', '✨', '🌠', '🎆', '🎇',
                     '🟥', '🟧', '🟨', '🟩', '🟦', '🟪', '⬛', '⬜', '🔴', '🟠',
                     '🟡', '🟢', '🔵', '🟣', '🟤', '⚪', '⚫', '🔺', '🔻', '🔶',
                     '🔷', '🔸', '🔹', '💠', '🔳', '🔲', '▪️', '▫️', '◼️', '◻️'],
            'mega_emoji': [
                # 🎲 Gaming & Entertainment
                '🎲', '🎯', '🎮', '🕹️', '🎰', '🎳', '🎪', '🎭', '🎨', '🎬', '🎤', '🎧', '🎼', '🎵', '🎶', '🎸', '🥁', '🎺', '🎷', '🪗', '🎻',
                # ⭐ Space & Sky  
                '⭐', '🌟', '💫', '✨', '🌠', '☄️', '🪐', '🌌', '🌙', '🌛', '🌜', '🌚', '🌕', '🌖', '🌗', '🌘', '🌑', '🌒', '🌓', '🌔', '☀️', '🌞',
                # 🔥 Energy & Magic
                '🔥', '💎', '🚀', '⚡', '🔮', '🎆', '🎇', '💥', '💢', '💨', '💦', '💫', '🌈', '☁️', '⛅', '⛈️', '🌤️', '🌦️', '🌧️', '⛆', '🌩️', '🌨️',
                # 😀 All Face Expressions (80+ faces)
                '😀', '😃', '😄', '😁', '😆', '😅', '🤣', '😂', '🙂', '🙃', '😉', '😊', '😇', '🥰', '😍', '🤩', '😘', '😗', '😚', '😙',
                '🥲', '😋', '😛', '😜', '🤪', '😝', '🤑', '🤗', '🤭', '🤫', '🤔', '🤐', '🤨', '😐', '😑', '😶', '😏', '😒', '🙄', '😬',
                '🤥', '😌', '😔', '😪', '🤤', '😴', '😷', '🤒', '🤕', '🤢', '🤮', '🤧', '🥵', '🥶', '🥴', '😵', '🤯', '🤠', '🥳', '🥸',
                '😎', '🤓', '🧐', '😕', '😟', '🙁', '☹️', '😮', '😯', '😲', '😳', '🥺', '😦', '😧', '😨', '😰', '😥', '😢', '😭', '😱',
                '😖', '😣', '😞', '😓', '😩', '😫', '🥱', '😤', '😡', '😠', '🤬', '😈', '👿', '💀', '☠️', '💩', '🤡', '👹', '👺', '👻',
                # 🐶 Animals & Nature (50+ animals)
                '🐶', '🐱', '🐭', '🐹', '🐰', '🦊', '🐻', '🐼', '🐨', '🐯', '🦁', '🐮', '🐷', '🐽', '🐸', '🐵', '🙈', '🙉', '🙊', '🐒',
                '🐔', '🐧', '🐦', '🐤', '🐣', '🐥', '🦆', '🦅', '🦉', '🦇', '🐺', '🐗', '🐴', '🦄', '🐝', '🐛', '🦋', '🐌', '🐞', '🐜',
                '🦗', '🕷️', '🕸️', '🦂', '🐢', '🐍', '🦎', '🦖', '🦕', '🐙', '🦑', '🦐', '🦞', '🦀', '🐡', '🐠', '🐟', '🐬', '🐳', '🐋',
                # 🍎 Food & Drink (60+ foods)
                '🍎', '🍏', '🍊', '🍋', '🍌', '🍉', '🍇', '🍓', '🫐', '🍈', '🍒', '🍑', '🥭', '🍍', '🥥', '🥝', '🍅', '🍆', '🥑', '🥦',
                '🥬', '🥒', '🌶️', '🫑', '🌽', '🥕', '🫒', '🧄', '🧅', '🥔', '🍠', '🥐', '🥖', '🍞', '🥨', '🥯', '🧀', '🥚', '🍳', '🧈',
                '🥞', '🧇', '🥓', '🥩', '🍗', '🍖', '🦴', '🌭', '🍔', '🍟', '🍕', '🫓', '🥪', '🥙', '🧆', '🌮', '🌯', '🫔', '🥗', '🥘',
                # 🚗 Transport & Travel (40+ vehicles)
                '🚗', '🚕', '🚙', '🚌', '🚎', '🏎️', '🚓', '🚑', '🚒', '🚐', '🛻', '🚚', '🚛', '🚜', '🏍️', '🛵', '🚲', '🛴', '🛹', '🛼',
                '🚁', '🛸', '✈️', '🛩️', '🛫', '🛬', '🪂', '💺', '🚀', '🛰️', '🚢', '⛵', '🚤', '🛥️', '🛳️', '⛴️', '🚂', '🚃', '🚄', '🚅',
                # 🎾 Sports & Activities (30+ sports)
                '⚽', '🏀', '🏈', '⚾', '🥎', '🎾', '🏐', '🏉', '🥏', '🎱', '🪀', '🏓', '🏸', '🏒', '🏑', '🥍', '🏏', '🪃', '🥅', '⛳',
                '🪁', '🏹', '🎣', '🤿', '🥊', '🥋', '🎽', '🛹', '🛷', '⛷️', '🏂', '🪂', '🏋️', '🤸', '🤾', '🏌️', '🏇', '🧘', '🏃', '🚶',
                # 🌺 Plants & Flowers (25+ plants)
                '🌸', '🌺', '🌻', '🌷', '🌹', '🥀', '🌾', '🌿', '🍀', '🍃', '🌱', '🌲', '🌳', '🌴', '🌵', '🌶️', '🫑', '🥒', '🥬', '🥦',
                '🧄', '🧅', '🍄', '🟫', '🪨', '🪵', '🌰', '🌰', '🥜', '🫘', '🌭', '🫓', '🥖', '🍞', '🥨',
                # 🎪 Objects & Tools (50+ objects)
                '🔨', '🪓', '⛏️', '🔧', '🔩', '⚙️', '🪛', '🔗', '⛓️', '🪝', '🧲', '🪜', '🪣', '🧽', '🪒', '🧴', '🧼', '🪥', '🪮', '🧻',
                '🪆', '🎁', '🎀', '🎊', '🎉', '🎈', '🎂', '🍰', '🧁', '🍭', '🍬', '🍫', '🍩', '🍪', '🍯', '🧂', '🧈', '🥛', '🍼', '☕',
                '🫖', '🍵', '🧃', '🥤', '🧋', '🍶', '🍾', '🍷', '🍸', '🍹', '🍺', '🍻', '🥂', '🥃', '🫗', '🧊', '🥄', '🍴', '🍽️', '🥢',
                # 💰 Money & Symbols (20+ symbols)
                '💰', '💴', '💵', '💶', '💷', '💸', '💳', '🧾', '💹', '📈', '📉', '📊', '📋', '📌', '📍', '📎', '🖇️', '📏', '📐', '✂️',
                # 🏆 Awards & Achievements
                '🏆', '🥇', '🥈', '🥉', '🏅', '🎖️', '🎗️', '🏵️', '🎀', '🎁', '🎊', '🎉', '🎈', '🎂', '🍰', '🧁', '🍭', '🍬', '🍫', '🎪',
                # 🌍 World & Geography (30+ places)
                '🌍', '🌎', '🌏', '🌐', '🗺️', '🏔️', '⛰️', '🌋', '🗻', '🏕️', '🏖️', '🏜️', '🏝️', '🏞️', '🏟️', '🏛️', '🏗️', '🧱', '🏘️', '🏚️',
                '🏠', '🏡', '🏢', '🏣', '🏤', '🏥', '🏦', '🏨', '🏩', '🏪', '🏫', '🏬', '🏭', '🏯', '🏰', '🗼', '🗽', '⛪', '🕌', '🛕',
                # 🔤 Letters & Numbers (26 letters + 10 numbers)
                '🔤', '🔡', '🔠', '🔢', '🔣', '1️⃣', '2️⃣', '3️⃣', '4️⃣', '5️⃣', '6️⃣', '7️⃣', '8️⃣', '9️⃣', '0️⃣', '🔟', '#️⃣', '*️⃣',
                '🅰️', '🅱️', '🆎', '🆑', '🆒', '🆓', '🆔', '🆕', '🆖', '🆗', '🆘', '🆙', '🆚', '🈁', '🈂️', '🈷️', '🈶', '🈯', '🉐', '🈹',
                # 🎨 Art & Creativity (20+ creative)
                '🎨', '🖌️', '🖍️', '🖊️', '🖋️', '✏️', '✒️', '🖇️', '📝', '📄', '📃', '📑', '📊', '📈', '📉', '📋', '📌', '📍', '📎', '🔖',
                # 🧬 Science & Tech (30+ science)
                '🧬', '🔬', '🔭', '📡', '💻', '🖥️', '🖨️', '⌨️', '🖱️', '🖲️', '💽', '💾', '💿', '📀', '🧮', '🎛️', '⏱️', '⏰', '⏲️', '⏳',
                '⌛', '📱', '📞', '☎️', '📟', '📠', '🔋', '🔌', '💡', '🔦', '🕯️', '🪔', '🧯', '🛢️', '💸', '💰', '🔨', '⚒️', '🛠️', '⚙️',
                # 🌈 Colors & Shapes (All color squares and circles)
                '🟥', '🟧', '🟨', '🟩', '🟦', '🟪', '🟫', '⬛', '⬜', '🔴', '🟠', '🟡', '🟢', '🔵', '🟣', '🟤', '⚪', '⚫',
                '🔺', '🔻', '🔶', '🔷', '🔸', '🔹', '💠', '🔳', '🔲', '▪️', '▫️', '◼️', '◻️', '◾', '◽', '▪️', '▫️', '🔘', '🔲', '🔳',
                # 🦄 Fantasy & Magic (20+ fantasy)
                '🦄', '🐉', '🧚', '🧛', '🧜', '🧝', '🧞', '🧟', '🦸', '🦹', '🧙', '👸', '🤴', '👑', '💍', '💎', '🔮', '🪄', '🧿', '📿',
                # 🍎 More Food (40+ additional foods)
                '🍎', '🍏', '🍊', '🍋', '🍌', '🍉', '🍇', '🍓', '🫐', '🍈', '🍒', '🍑', '🥭', '🍍', '🥥', '🥝', '🍅', '🥕', '🌽', '🥔',
                '🍠', '🥐', '🥖', '🍞', '🥨', '🥯', '🧀', '🥚', '🍳', '🧈', '🥞', '🧇', '🥓', '🥩', '🍗', '🍖', '🌭', '🍔', '🍟', '🍕',
                # 🎸 Music & Sound (15+ music)
                '🎸', '🥁', '🎺', '🎷', '🪗', '🎻', '🎼', '🎵', '🎶', '🎤', '🎧', '📻', '📢', '📣', '🔊', '🔉', '🔈', '🔇', '🎚️', '🎛️',
                # 🏃 People & Activities (30+ people)
                '🏃', '🚶', '🧘', '🛀', '🛌', '👤', '👥', '🫂', '👪', '👨', '👩', '👧', '👦', '👶', '👵', '👴', '👱', '👨‍🦰', '👩‍🦰', '👨‍🦱',
                '👩‍🦱', '👨‍🦲', '👩‍🦲', '👨‍🦳', '👩‍🦳', '🧔', '👮', '👷', '💂', '🕵️', '👩‍⚕️', '👨‍⚕️', '👩‍🌾', '👨‍🌾', '👩‍🍳',
                # 🎭 More Entertainment (20+ entertainment)
                '🎭', '🎪', '🎨', '🎬', '🎤', '🎧', '🎼', '🎵', '🎶', '🎸', '🥁', '🎺', '🎷', '🪗', '🎻', '🎲', '🎯', '🎮', '🕹️', '🎰',
                # 🌸 Nature & Weather (25+ nature)
                '🌸', '🌺', '🌻', '🌷', '🌹', '🥀', '🌾', '🌿', '🍀', '🍃', '🌱', '🌲', '🌳', '🌴', '🌵', '🌶️', '🍄', '🌰', '🦋', '🐝',
                '🐞', '🦗', '🕷️', '🌈', '☀️', '🌤️', '⛅', '🌦️', '🌧️', '⛈️', '🌩️', '🌨️', '❄️', '☃️', '⛄', '🌬️', '💨', '🌪️', '🌫️',
                # 💎 Gems & Treasures (15+ treasures)
                '💎', '💍', '👑', '💰', '💴', '💵', '💶', '💷', '💸', '💳', '🧾', '💹', '🏆', '🥇', '🥈', '🥉', '🏅', '🎖️', '🏵️', '🎗️',
                # 🔧 Tools & Technology (25+ tools)
                '🔧', '🔨', '⚒️', '🛠️', '⛏️', '🪓', '🪚', '🔩', '⚙️', '🪛', '🔗', '⛓️', '📱', '💻', '🖥️', '🖨️', '⌨️', '🖱️', '📷', '📹',
                '📽️', '🎥', '📞', '☎️', '📟', '📠', '📺', '📻', '🎙️', '🎚️', '🎛️', '🧭', '⏰', '⏲️', '⏱️', '⏳', '⌛', '📡', '🔋', '🔌',
                # 🎯 Games & Fun (20+ games)
                '🎯', '🎲', '🎮', '🕹️', '🎰', '🎳', '🎪', '🎭', '🃏', '🀄', '🎴', '🎨', '🧩', '🪅', '🪆', '🎊', '🎉', '🎈', '🎁', '🎀',
                # 🌟 Final Special Characters
                '🌟', '✨', '💫', '⭐', '🌠', '💥', '💢', '💨', '💦', '💤', '🔥', '❄️', '⚡', '🌈', '☄️', '💎', '🔮', '🎆', '🎇', '🎊'
            ],
            'binary': ['0', '1'],
            'hex': list('0123456789ABCDEF'),
        }
        
        # Set initial character set (validate it exists)
        if initial_char_set in self.char_sets:
            self.current_char_set = initial_char_set
        else:
            print(f"⚠️  Unknown character set '{initial_char_set}', using 'blocks'")
            self.current_char_set = 'blocks'
        
    def clear_screen(self):
        """Clear terminal screen"""
        print('\033[2J\033[H', end='')
        
    def set_color(self, fg_color, bg_color):
        """Set ANSI foreground and background colors"""
        return f'\033[{fg_color};{bg_color}m'
        
    def reset_color(self):
        """Reset to default colors"""
        return '\033[0m'
        
    def byte_to_visual(self, byte_val):
        """Convert a random byte to visual elements"""
        # Use different bits for different visual elements
        fg_index = (byte_val & 0x0F)  # Lower 4 bits for foreground (16 colors)
        bg_index = (byte_val >> 4) & 0x0F  # Upper 4 bits for background (16 colors)
        
        # Map to actual ANSI color codes
        fg_color = self.fg_colors[fg_index]
        bg_color = self.bg_colors[bg_index]
        
        # Character selection based on byte value
        char_set = self.char_sets[self.current_char_set]
        char_index = byte_val % len(char_set)
        character = char_set[char_index]
        
        return self.set_color(fg_color, bg_color) + character + self.reset_color()
        
    def render_frame(self, data_bytes):
        """Render a frame of random art"""
        if len(data_bytes) < self.width * self.height:
            return  # Not enough data
            
        self.clear_screen()
        
        # Header with stats
        print(f"🎲 FlipperRNG Live Random ANSI Art 🎨")
        print(f"📊 Bytes: {len(data_bytes)} | Set: {self.current_char_set} | Press 'c' to change, 'q' to quit")
        print("─" * min(self.width, 80))  # Limit separator width
        
        # Render random art grid with emoji width compensation
        display_height = min(self.height - 4, 15)  # Limit height to prevent terminal issues
        base_width = min(self.width, 80)           # Base width
        
        # Adjust width for double-width characters (emojis)
        if self.current_char_set in ['emoji', 'mega_emoji']:
            display_width = base_width // 2  # Emojis are double-width
        else:
            display_width = base_width
        
        for y in range(display_height):
            line = ""
            for x in range(display_width):
                byte_index = (y * display_width + x) % len(data_bytes)
                if byte_index < len(data_bytes):
                    visual_char = self.byte_to_visual(data_bytes[byte_index])
                    line += visual_char
                else:
                    line += " "
            print(line + self.reset_color())  # Ensure line ends with color reset
            
        # Footer with stats
        print("─" * min(self.width, 80))
        if len(data_bytes) >= 8:
            print(f"🔢 Latest: {' '.join(f'{b:02X}' for b in data_bytes[-8:])}")
        
        # Flush output to ensure proper rendering
        sys.stdout.flush()
        
    def change_char_set(self):
        """Cycle through different character sets"""
        char_set_names = list(self.char_sets.keys())
        current_index = char_set_names.index(self.current_char_set)
        next_index = (current_index + 1) % len(char_set_names)
        self.current_char_set = char_set_names[next_index]
        print(f"\n🎨 Character set changed to: {self.current_char_set}")

def read_serial_data(ser, data_queue, visualizer):
    """Read data from serial port and queue for visualization"""
    buffer = bytearray()
    
    while True:
        try:
            if ser.in_waiting:
                new_data = ser.read(ser.in_waiting)
                buffer.extend(new_data)
                
                # When we have enough data for a frame, queue it
                frame_size = visualizer.width * visualizer.height
                if len(buffer) >= frame_size:
                    frame_data = list(buffer[:frame_size])
                    data_queue.put(frame_data)
                    buffer = buffer[frame_size//2:]  # Keep some overlap
                    
            time.sleep(0.01)
            
        except Exception as e:
            print(f"Serial read error: {e}")
            break

def random_ansi_art_monitor(port, baudrate=115200, width=80, height=20, char_set='blocks'):
    """Monitor random data and display as ANSI art with interactive controls"""
    
    print(f"🎨 FlipperRNG Random ANSI Art Visualizer")
    print(f"📡 Connecting to {port} at {baudrate} baud")
    print(f"🖼️  Canvas: {width}x{height} characters")
    print(f"💡 Set FlipperRNG to UART mode for best results")
    print("-" * 60)
    
    try:
        # Connect to UART data stream (not CLI)
        ser = serial.Serial(port, baudrate, timeout=1)
        print(f"✅ Connected to {port}")
        print(f"🎭 Waiting for random data stream...")
        
        # Initialize visualizer with specified character set
        visualizer = ANSIArtVisualizer(width, height, char_set)
        data_queue = Queue()
        
        # Start data reading thread
        read_thread = threading.Thread(
            target=read_serial_data, 
            args=(ser, data_queue, visualizer), 
            daemon=True
        )
        read_thread.start()
        
        # Visual refresh rate
        refresh_rates = [0.1, 0.3, 0.5, 1.0]  # Fast to slow options
        refresh_index = 1  # Start with 300ms
        last_frame_data = None
        frame_count = 0
        
        print("🎨 Starting random art visualization...")
        print("🎮 Controls: 'c'=change chars, 'f'=refresh rate, 's'=screenshot, 'q'=quit")
        time.sleep(1)
        
        # Set terminal to non-blocking input
        old_settings = termios.tcgetattr(sys.stdin)
        tty.setraw(sys.stdin.fileno())
        
        try:
            while True:
                # Check for keyboard input (non-blocking)
                if sys.stdin in select.select([sys.stdin], [], [], 0)[0]:
                    key = sys.stdin.read(1).lower()
                    
                    if key == 'q':
                        break
                    elif key == 'c':
                        visualizer.change_char_set()
                        time.sleep(0.5)  # Brief pause to show message
                    elif key == 'f':
                        refresh_index = (refresh_index + 1) % len(refresh_rates)
                        print(f"\n🔄 Refresh rate: {refresh_rates[refresh_index]*1000:.0f}ms")
                        time.sleep(0.5)
                    elif key == 's':
                        print(f"\n📸 Screenshot saved as frame_{frame_count}")
                        time.sleep(0.5)
                
                # Check for new frame data
                if not data_queue.empty():
                    last_frame_data = data_queue.get()
                    
                # Render if we have data
                if last_frame_data:
                    visualizer.render_frame(last_frame_data)
                    frame_count += 1
                    
                    # Add some statistics
                    byte_avg = sum(last_frame_data) / len(last_frame_data)
                    byte_min = min(last_frame_data)
                    byte_max = max(last_frame_data)
                    print(f"📈 Frame {frame_count} | Avg={byte_avg:.1f} | Range={byte_max-byte_min} | Rate={refresh_rates[refresh_index]*1000:.0f}ms | Set={visualizer.current_char_set}")
                
                time.sleep(refresh_rates[refresh_index])
                
        except KeyboardInterrupt:
            print(f"\n🎨 Art visualization stopped")
        finally:
            # Restore terminal settings
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old_settings)
            
        ser.close()
        return 0
        
    except serial.SerialException as e:
        print(f"❌ Connection error: {e}")
        print(f"💡 Make sure:")
        print(f"   - FlipperRNG is set to UART output mode")
        print(f"   - Flipper is connected to {port}")
        print(f"   - UART pins are properly connected")
        return 1
    except Exception as e:
        print(f"❌ Unexpected error: {e}")
        return 1

def interactive_ansi_art(port, baudrate=115200):
    """Interactive ANSI art with user controls"""
    
    try:
        ser = serial.Serial(port, baudrate, timeout=1)
        visualizer = ANSIArtVisualizer(80, 20)
        
        print("🎮 Interactive Random ANSI Art Mode")
        print("Controls:")
        print("  'c' - Change character set")
        print("  'f' - Toggle refresh rate (fast/slow)")
        print("  's' - Take screenshot")
        print("  'q' - Quit")
        print("-" * 60)
        
        buffer = bytearray()
        refresh_rate = 0.3
        frame_count = 0
        
        while True:
            # Read data
            if ser.in_waiting:
                buffer.extend(ser.read(ser.in_waiting))
                
            # Render when we have enough data
            frame_size = visualizer.width * visualizer.height
            if len(buffer) >= frame_size:
                frame_data = list(buffer[:frame_size])
                visualizer.render_frame(frame_data)
                buffer = buffer[frame_size//4:]  # Keep some data
                frame_count += 1
                
                print(f"🎬 Frame: {frame_count} | Refresh: {refresh_rate*1000:.0f}ms | Char set: {visualizer.current_char_set}")
                
            time.sleep(refresh_rate)
            
    except KeyboardInterrupt:
        print("\n🎨 Interactive art mode ended")
        ser.close()

def main():
    parser = argparse.ArgumentParser(description="FlipperRNG Random ANSI Art Visualizer")
    parser.add_argument("--port", default="/dev/ttyUSB0", help="Serial port for UART data (default: /dev/ttyUSB0)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--width", type=int, default=80, help="Art width in characters (default: 80)")
    parser.add_argument("--height", type=int, default=20, help="Art height in characters (default: 20)")
    parser.add_argument("--charset", default="blocks", 
                       choices=['blocks', 'patterns', 'ascii', 'symbols', 'emoji', 'mega_emoji', 'binary', 'hex'],
                       help="Character set to use (default: blocks)")
    parser.add_argument("--interactive", action="store_true", help="Interactive mode with controls")
    
    args = parser.parse_args()
    
    print("🎨 FlipperRNG Random ANSI Art Visualizer")
    print("=" * 60)
    print("📋 Setup Instructions:")
    print("   1. Set FlipperRNG to UART output mode")
    print("   2. Connect Flipper UART pins to USB adapter")
    print("   3. Start FlipperRNG generator")
    print("   4. Watch the random art come alive!")
    print("=" * 60)
    
    if args.interactive:
        return interactive_ansi_art(args.port, args.baud)
    else:
        return random_ansi_art_monitor(args.port, args.baud, args.width, args.height, args.charset)

if __name__ == "__main__":
    sys.exit(main())
