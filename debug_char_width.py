#!/usr/bin/env python3
"""
Debug Character Width - Test different character sets
"""

def test_char_widths():
    """Test how different characters render in terminal"""
    
    print("🔍 Character Width Test")
    print("=" * 50)
    
    char_sets = {
        'ascii': ['#', '@', '%', '*', '+', '=', '-', '.'],
        'blocks': ['█', '▉', '▊', '▋', '▌', '▍', '▎', '▏'],
        'symbols': ['●', '○', '◆', '◇', '■', '□', '▲', '△'],
        'emoji': ['🎲', '🎯', '🎨', '⭐', '🔥', '💎', '🚀', '⚡'],
    }
    
    for set_name, chars in char_sets.items():
        print(f"\n{set_name.upper()} characters:")
        
        # Test single width
        print("Single: ", end="")
        for char in chars[:8]:
            print(f"{char}", end="")
        print(" <-- Should align")
        
        # Test with colors
        print("Colored:", end="")
        colors = [31, 32, 33, 34, 35, 36, 37, 91]
        for i, char in enumerate(chars[:8]):
            color = colors[i % len(colors)]
            print(f"\033[{color}m{char}\033[0m", end="")
        print(" <-- Should align")
        
        # Test expected width
        if set_name == 'emoji':
            print("NOTE: Emojis are double-width - each takes 2 character positions")
        else:
            print("NOTE: Single-width characters")
    
    print("\n" + "=" * 50)
    print("💡 If emoji line looks twice as wide, that's the issue!")
    print("💡 The fix: Use width//2 for emoji character sets")

if __name__ == "__main__":
    test_char_widths()
