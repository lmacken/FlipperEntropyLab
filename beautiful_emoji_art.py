#!/usr/bin/env python3
"""
Beautiful FlipperRNG Emoji Art - Professional terminal UI
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
    from rich.progress import Progress, SpinnerColumn, TextColumn
    from rich.align import Align
    from rich.text import Text
    from rich.columns import Columns
    from rich.box import ROUNDED, DOUBLE, HEAVY
except ImportError:
    print("❌ Rich library not found!")
    print("📦 Install with: pip install rich")
    print("🔧 Or run: pip install -r requirements.txt")
    sys.exit(1)

class BeautifulEmojiArt:
    def __init__(self, width=40, height=15):
        self.console = Console()
        self.width = width
        self.height = height
        self.data_queue = Queue()
        self.stats = {'frames': 0, 'bytes': 0, 'avg': 0, 'min': 0, 'max': 0}
        
        # Curated emoji sets for maximum visual impact
        self.emoji_sets = {
            'party': ['🎉', '🎊', '🎈', '🎁', '🎀', '🎂', '🍰', '🧁', '🎪', '🎭', '🎨', '🎬', '🎤', '🎧', '🎸', '🥳'],
            'space': ['🚀', '🛸', '⭐', '🌟', '💫', '✨', '🌠', '☄️', '🪐', '🌌', '🌙', '☀️', '🌞', '🌍', '🌎', '🌏'],
            'animals': ['🐶', '🐱', '🐭', '🐹', '🐰', '🦊', '🐻', '🐼', '🐨', '🐯', '🦁', '🐮', '🐷', '🐸', '🐵', '🐔'],
            'food': ['🍎', '🍏', '🍊', '🍋', '🍌', '🍉', '🍇', '🍓', '🍈', '🍒', '🥭', '🍍', '🥥', '🍅', '🥑', '🥦'],
            'faces': ['😀', '😃', '😄', '😁', '😆', '😅', '🤣', '😂', '🙂', '🙃', '😉', '😊', '😇', '🥰', '😍', '🤩'],
            'magic': ['🔮', '✨', '💎', '🌟', '⭐', '💫', '🎆', '🎇', '🔥', '⚡', '💥', '🌈', '🦄', '🧚', '🧙', '👸'],
            'mega': [
                '🎲', '🎯', '🎮', '🎪', '🎭', '🎨', '🎬', '🎤', '🎧', '🎸', '🥁', '🎺', '🎷', '🎻', '🎼', '🎵',
                '😀', '😃', '😄', '😁', '😆', '😅', '🤣', '😂', '🙂', '🙃', '😉', '😊', '😇', '🥰', '😍', '🤩',
                '🐶', '🐱', '🐭', '🐹', '🐰', '🦊', '🐻', '🐼', '🐨', '🐯', '🦁', '🐮', '🐷', '🐸', '🐵', '🐔',
                '🍎', '🍏', '🍊', '🍋', '🍌', '🍉', '🍇', '🍓', '🍈', '🍒', '🥭', '🍍', '🥥', '🍅', '🥑', '🥦',
                '🚀', '🛸', '⭐', '🌟', '💫', '✨', '🌠', '☄️', '🪐', '🌌', '🌙', '☀️', '🌞', '🌍', '🌎', '🌏',
                '🔮', '💎', '🌈', '🦄', '🧚', '🧙', '👸', '🤴', '👑', '💍', '🎆', '🎇', '🔥', '⚡', '💥', '💫'
            ]
        }
        
        self.current_set = 'mega'
        
    def create_layout(self):
        """Create the main application layout"""
        layout = Layout()
        
        layout.split_column(
            Layout(name="header", size=3),
            Layout(name="main"),
            Layout(name="footer", size=4)
        )
        
        layout["main"].split_row(
            Layout(name="left", ratio=1),
            Layout(name="center", ratio=3),
            Layout(name="right", ratio=1)
        )
        
        return layout
        
    def generate_emoji_grid(self, data):
        """Generate emoji grid from random data"""
        if not data:
            return "⏳ Waiting for random data..."
            
        emojis = self.emoji_sets[self.current_set]
        lines = []
        
        for y in range(self.height):
            line = ""
            for x in range(self.width):
                idx = (y * self.width + x) % len(data)
                byte_val = data[idx]
                emoji = emojis[byte_val % len(emojis)]
                line += emoji
            lines.append(line)
            
        return "\n".join(lines)
        
    def create_stats_table(self, data):
        """Create statistics table"""
        table = Table(title="📊 Entropy Statistics", box=ROUNDED)
        table.add_column("Metric", style="cyan")
        table.add_column("Value", style="magenta")
        
        if data:
            avg_val = sum(data) / len(data)
            min_val = min(data)
            max_val = max(data)
            unique_bytes = len(set(data))
            
            table.add_row("Bytes Processed", f"{len(data)}")
            table.add_row("Average Value", f"{avg_val:.2f}")
            table.add_row("Min / Max", f"{min_val} / {max_val}")
            table.add_row("Range", f"{max_val - min_val}")
            table.add_row("Unique Bytes", f"{unique_bytes}/256")
            table.add_row("Entropy %", f"{(max_val-min_val)/255*100:.1f}%")
            
        return table
        
    def create_header(self):
        """Create application header"""
        title = Text("🎲 FlipperRNG Beautiful Emoji Art 🎨", style="bold magenta")
        subtitle = Text(f"Emoji Set: {self.current_set} ({len(self.emoji_sets[self.current_set])} emojis)", style="cyan")
        
        header_panel = Panel(
            Align.center(f"{title}\n{subtitle}"),
            box=DOUBLE,
            style="bright_blue"
        )
        return header_panel
        
    def create_controls(self):
        """Create controls panel"""
        controls = Table(box=ROUNDED)
        controls.add_column("Key", style="yellow")
        controls.add_column("Action", style="green")
        
        controls.add_row("1-7", "Change emoji set")
        controls.add_row("Space", "Pause/Resume")
        controls.add_row("R", "Reset stats")
        controls.add_row("Q", "Quit")
        
        return Panel(controls, title="🎮 Controls", box=ROUNDED)

def read_serial_thread(ser, art_app):
    """Background thread to read serial data"""
    buffer = bytearray()
    
    while True:
        try:
            if ser.in_waiting:
                new_data = ser.read(ser.in_waiting)
                buffer.extend(new_data)
                
                # Queue data when we have enough
                if len(buffer) >= 800:
                    art_app.data_queue.put(list(buffer[:800]))
                    buffer = buffer[400:]  # Keep some overlap
                    
            time.sleep(0.01)
        except Exception:
            break

def beautiful_emoji_art(port="/dev/ttyUSB0", baudrate=115200):
    """Beautiful emoji art with rich terminal UI"""
    
    console = Console()
    
    with console.status("[bold green]Connecting to FlipperRNG...") as status:
        try:
            ser = serial.Serial(port, baudrate, timeout=2)
            status.update("[bold green]Connected! Starting emoji art...")
            time.sleep(1)
        except Exception as e:
            console.print(f"[bold red]❌ Connection failed: {e}")
            console.print("[yellow]💡 Make sure FlipperRNG is set to UART mode")
            return 1
    
    # Initialize art generator
    art_app = BeautifulEmojiArt(40, 15)
    
    # Start data reading thread
    read_thread = threading.Thread(target=read_serial_thread, args=(ser, art_app), daemon=True)
    read_thread.start()
    
    # Main display loop
    layout = art_app.create_layout()
    current_data = None
    frame_count = 0
    paused = False
    
    try:
        with Live(layout, console=console, refresh_per_second=4) as live:
            while True:
                # Get new data
                if not art_app.data_queue.empty():
                    current_data = art_app.data_queue.get()
                    frame_count += 1
                
                # Update layout
                layout["header"].update(art_app.create_header())
                
                if current_data and not paused:
                    # Main emoji display
                    emoji_grid = art_app.generate_emoji_grid(current_data)
                    emoji_panel = Panel(
                        Align.center(emoji_grid),
                        title=f"🎨 Random Emoji Canvas (Frame {frame_count})",
                        box=HEAVY,
                        style="bright_green"
                    )
                    layout["center"].update(emoji_panel)
                    
                    # Statistics
                    stats_table = art_app.create_stats_table(current_data)
                    layout["right"].update(stats_table)
                    
                else:
                    # Waiting for data
                    waiting_panel = Panel(
                        Align.center("⏳ Waiting for random data...\n\n🔌 Check FlipperRNG UART connection"),
                        title="🎭 Status",
                        box=ROUNDED,
                        style="yellow"
                    )
                    layout["center"].update(waiting_panel)
                
                # Controls
                layout["left"].update(art_app.create_controls())
                
                # Footer
                footer_text = f"📡 Port: {port} | 📊 Frames: {frame_count} | 🎨 Emoji Chaos Powered by FlipperRNG"
                if paused:
                    footer_text += " | ⏸️ PAUSED"
                
                footer_panel = Panel(
                    Align.center(footer_text),
                    style="blue"
                )
                layout["footer"].update(footer_panel)
                
                time.sleep(0.25)  # 4 FPS refresh rate
                
    except KeyboardInterrupt:
        console.print(f"\n[bold green]🎨 Beautiful emoji art ended after {frame_count} frames!")
        console.print("[cyan]✨ Thanks for the emoji chaos!")
    finally:
        ser.close()
    
    return 0

def main():
    parser = argparse.ArgumentParser(description="Beautiful FlipperRNG Emoji Art")
    parser.add_argument("--port", default="/dev/ttyUSB0", help="UART port")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    
    args = parser.parse_args()
    
    console = Console()
    
    # Startup banner
    console.print()
    console.print(Panel(
        Align.center(
            "[bold magenta]🎨 FlipperRNG Beautiful Emoji Art 🎨[/]\n\n" +
            "[cyan]Professional terminal visualization of entropy[/]\n" +
            "[yellow]Powered by Rich library for stunning UI[/]"
        ),
        title="🎲 Welcome",
        box=DOUBLE,
        style="bright_blue"
    ))
    
    console.print("\n[bold green]📋 Setup Instructions:[/]")
    console.print("   [cyan]1.[/] Set FlipperRNG to UART output mode")
    console.print("   [cyan]2.[/] Start FlipperRNG generator") 
    console.print("   [cyan]3.[/] Enjoy the beautiful emoji chaos!")
    console.print()
    
    input("Press Enter to start the beautiful emoji art experience... ")
    
    return beautiful_emoji_art(args.port, args.baud)

if __name__ == "__main__":
    sys.exit(main())
