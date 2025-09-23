#!/usr/bin/env python3
"""
Premium FlipperRNG Emoji Art - Ultimate terminal experience
"""

import serial
import sys
import time
import threading
from queue import Queue
import argparse

try:
    from rich.console import Console
    from rich.panel import Panel
    from rich.layout import Layout
    from rich.live import Live
    from rich.table import Table
    from rich.align import Align
    from rich.text import Text
    from rich.box import ROUNDED, DOUBLE, HEAVY, ASCII
    from rich.columns import Columns
    from rich.progress import Progress, BarColumn, TextColumn, SpinnerColumn
    from rich.rule import Rule
    from rich.padding import Padding
except ImportError:
    print("❌ Rich library required!")
    print("📦 Install: pip install rich")
    sys.exit(1)

class PremiumEmojiArt:
    def __init__(self):
        self.console = Console()
        self.data_queue = Queue()
        self.current_theme = 'mega'
        self.frame_count = 0
        self.total_bytes = 0
        self.paused = False
        self.refresh_rate = 0.25
        
        # Themed emoji collections
        self.themes = {
            'party': {
                'name': '🎉 Party Time',
                'emojis': ['🎉', '🎊', '🎈', '🎁', '🎀', '🎂', '🍰', '🧁', '🎪', '🎭', '🎨', '🎬', '🎤', '🎧', '🎸', '🥳', '🍾', '🥂', '🍻', '🎆'],
                'style': 'magenta'
            },
            'space': {
                'name': '🚀 Cosmic Chaos',
                'emojis': ['🚀', '🛸', '⭐', '🌟', '💫', '✨', '🌠', '☄️', '🪐', '🌌', '🌙', '🌛', '🌜', '☀️', '🌞', '🌍', '🌎', '🌏', '🛰️', '👽'],
                'style': 'blue'
            },
            'nature': {
                'name': '🌿 Nature Vibes',
                'emojis': ['🌸', '🌺', '🌻', '🌷', '🌹', '🌿', '🍀', '🌱', '🌲', '🌳', '🌴', '🌵', '🍄', '🦋', '🐝', '🐞', '🌈', '☀️', '🌤️', '⛅'],
                'style': 'green'
            },
            'animals': {
                'name': '🐾 Animal Kingdom',
                'emojis': ['🐶', '🐱', '🐭', '🐹', '🐰', '🦊', '🐻', '🐼', '🐨', '🐯', '🦁', '🐮', '🐷', '🐸', '🐵', '🐔', '🐧', '🦆', '🦅', '🦉'],
                'style': 'yellow'
            },
            'food': {
                'name': '🍕 Food Fest',
                'emojis': ['🍎', '🍏', '🍊', '🍋', '🍌', '🍉', '🍇', '🍓', '🍈', '🍒', '🥭', '🍍', '🥥', '🍅', '🥑', '🥦', '🍔', '🍟', '🍕', '🌭'],
                'style': 'red'
            },
            'faces': {
                'name': '😄 Emoji Faces',
                'emojis': ['😀', '😃', '😄', '😁', '😆', '😅', '🤣', '😂', '🙂', '🙃', '😉', '😊', '😇', '🥰', '😍', '🤩', '😘', '😗', '😚', '😙'],
                'style': 'cyan'
            },
            'mega': {
                'name': '🌟 MEGA MIX',
                'emojis': [
                    '🎲', '🎯', '🎮', '🎪', '🎭', '🎨', '🎬', '🎤', '🎧', '🎸', '🥁', '🎺', '🎷', '🎻', '🎼', '🎵',
                    '😀', '😃', '😄', '😁', '😆', '😅', '🤣', '😂', '🙂', '🙃', '😉', '😊', '😇', '🥰', '😍', '🤩',
                    '🐶', '🐱', '🐭', '🐹', '🐰', '🦊', '🐻', '🐼', '🐨', '🐯', '🦁', '🐮', '🐷', '🐸', '🐵', '🐔',
                    '🍎', '🍏', '🍊', '🍋', '🍌', '🍉', '🍇', '🍓', '🍈', '🍒', '🥭', '🍍', '🥥', '🍅', '🥑', '🥦',
                    '🚀', '🛸', '⭐', '🌟', '💫', '✨', '🌠', '☄️', '🪐', '🌌', '🌙', '☀️', '🌞', '🌍', '🌎', '🌏',
                    '🔮', '💎', '🌈', '🦄', '🧚', '🧙', '👸', '🤴', '👑', '💍', '🎆', '🎇', '🔥', '⚡', '💥', '💫',
                    '🏆', '🥇', '🥈', '🥉', '🏅', '🎖️', '🎗️', '🏵️', '🎀', '🎁', '🎊', '🎉', '🎈', '🎂', '🍰', '🧁'
                ],
                'style': 'bright_magenta'
            }
        }
        
    def generate_emoji_canvas(self, data):
        """Generate the main emoji canvas"""
        if not data:
            return Panel(
                Align.center("⏳ Waiting for FlipperRNG data...\n\n🔌 Check UART connection"),
                title="🎭 Emoji Canvas",
                box=HEAVY,
                style="yellow"
            )
            
        theme = self.themes[self.current_theme]
        emojis = theme['emojis']
        
        # Generate emoji grid
        lines = []
        for y in range(20):  # Fixed height for consistency
            line = ""
            for x in range(35):  # Fixed width accounting for emoji double-width
                idx = (y * 35 + x) % len(data)
                byte_val = data[idx]
                emoji = emojis[byte_val % len(emojis)]
                line += emoji
            lines.append(line)
        
        emoji_display = "\n".join(lines)
        
        return Panel(
            Align.center(emoji_display),
            title=f"🎨 {theme['name']} Canvas (Frame {self.frame_count})",
            box=HEAVY,
            style=theme['style']
        )
        
    def run(self, port, baudrate):
        """Run the premium emoji art experience"""
        
        # Startup screen
        self.console.print()
        startup_panel = Panel(
            Align.center(
                "[bold bright_magenta]🎨 PREMIUM FLIPPERRNG EMOJI ART 🎨[/]\n\n" +
                "[bright_cyan]✨ Beautiful • Professional • Stunning ✨[/]\n\n" +
                "[yellow]🎭 Multiple emoji themes with rich terminal UI[/]\n" +
                "[green]📊 Real-time entropy statistics and monitoring[/]\n" +
                "[blue]🎮 Interactive controls and theme switching[/]"
            ),
            title="🌟 Premium Experience",
            box=DOUBLE,
            style="bright_blue"
        )
        self.console.print(startup_panel)
        
        input("\n🚀 Press Enter to launch the premium emoji art experience... ")
        
        # Connect to FlipperRNG
        with self.console.status("[bold green]🔌 Connecting to FlipperRNG...") as status:
            try:
                ser = serial.Serial(port, baudrate, timeout=2)
                status.update("[bold green]✅ Connected! Initializing emoji chaos...")
                time.sleep(1)
            except Exception as e:
                self.console.print(f"[bold red]❌ Connection failed: {e}")
                return 1
        
        # Start data thread
        read_thread = threading.Thread(target=read_serial_thread, args=(ser, self), daemon=True)
        read_thread.start()
        
        # Main UI loop
        current_data = None
        
        try:
            with Live(console=self.console, refresh_per_second=4) as live:
                while True:
                    # Get new data
                    if not self.data_queue.empty():
                        current_data = self.data_queue.get()
                        self.frame_count += 1
                        self.total_bytes += len(current_data)
                    
                    # Create main display
                    main_display = self.generate_emoji_canvas(current_data)
                    
                    # Create side panel with theme selector
                    theme_table = Table(title="🎭 Emoji Themes", box=ROUNDED)
                    theme_table.add_column("Key", style="yellow")
                    theme_table.add_column("Theme", style="cyan")
                    
                    for i, (key, theme) in enumerate(self.themes.items(), 1):
                        marker = "👉" if key == self.current_theme else "  "
                        theme_table.add_row(str(i), f"{marker} {theme['name']}")
                    
                    # Create layout
                    layout = Layout()
                    layout.split_row(
                        Layout(theme_table, ratio=1),
                        Layout(main_display, ratio=3)
                    )
                    
                    live.update(layout)
                    time.sleep(self.refresh_rate)
                    
        except KeyboardInterrupt:
            self.console.print(f"\n[bold green]🎨 Premium emoji art experience ended!")
            self.console.print(f"[cyan]📊 Total frames: {self.frame_count} | Total bytes: {self.total_bytes}")
            self.console.print("[yellow]✨ Thanks for experiencing the emoji chaos!")
        finally:
            ser.close()
        
        return 0

def read_serial_thread(ser, app):
    """Background serial reading"""
    buffer = bytearray()
    
    while True:
        try:
            if ser.in_waiting:
                buffer.extend(ser.read(ser.in_waiting))
                if len(buffer) >= 1000:
                    app.data_queue.put(list(buffer[:1000]))
                    buffer = buffer[500:]
            time.sleep(0.01)
        except:
            break

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Premium FlipperRNG Emoji Art")
    parser.add_argument("--port", default="/dev/ttyUSB0", help="UART port")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    
    args = parser.parse_args()
    
    app = PremiumEmojiArt()
    sys.exit(app.run(args.port, args.baud))
