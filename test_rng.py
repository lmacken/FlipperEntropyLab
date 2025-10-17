#!/usr/bin/env python3
"""
FlipperRNG Test Script
Tests random data from FlipperRNG via USB CDC
"""

import serial
import time
import sys
import os
from collections import Counter
import statistics


def find_flipper_device():
    """Find the Flipper device"""
    # Check all possible ttyACM devices
    import glob
    devices = glob.glob('/dev/ttyUSB*')

    for device in devices:
        try:
            print(f"Trying {device}...")
            ser = serial.Serial(device, 115200, timeout=1)
            print(f"Found Flipper at {device}")
            return ser
        except Exception as e:
            print(f"Failed to open {device}: {e}")
            continue
    return None


def basic_randomness_tests(data):
    """Basic statistical tests for randomness"""
    if len(data) == 0:
        return

    print(f"\n=== Basic Randomness Tests ===")
    print(f"Data length: {len(data)} bytes")

    # Byte frequency test
    byte_counts = Counter(data)
    expected_freq = len(data) / 256

    print(f"Expected frequency per byte value: {expected_freq:.2f}")
    print(f"Actual frequency range: {
          min(byte_counts.values())} - {max(byte_counts.values())}")

    # Chi-square test (simplified)
    chi_square = sum((count - expected_freq) ** 2 /
                     expected_freq for count in byte_counts.values())
    print(f"Chi-square statistic: {chi_square:.2f}")
    print(f"Chi-square critical value (α=0.05): 293.25")
    print(f"Chi-square test: {'PASS' if chi_square < 293.25 else 'FAIL'}")

    # Bit frequency test
    bit_count = sum(bin(byte).count('1') for byte in data)
    total_bits = len(data) * 8
    bit_ratio = bit_count / total_bits
    print(f"\nBit frequency: {bit_count}/{total_bits} = {bit_ratio:.4f}")
    print(f"Expected: ~0.5000")
    print(f"Bit frequency test: {
          'PASS' if 0.49 < bit_ratio < 0.51 else 'FAIL'}")

    # Entropy estimation (Shannon entropy)
    entropy = -sum((count/len(data)) * (count/len(data)).bit_length()
                   for count in byte_counts.values() if count > 0)
    max_entropy = 8.0  # log2(256)
    print(f"\nShannon entropy: {entropy:.4f} bits/byte")
    print(f"Maximum entropy: {max_entropy:.4f} bits/byte")
    print(f"Entropy ratio: {entropy/max_entropy:.4f}")
    print(f"Entropy test: {'PASS' if entropy/max_entropy > 0.95 else 'FAIL'}")


def hex_dump(data, max_lines=10):
    """Create a hex dump of the data"""
    print(f"\n=== Hex Dump (first {max_lines} lines) ===")
    for i in range(0, min(len(data), max_lines * 16), 16):
        hex_part = ' '.join(f'{b:02x}' for b in data[i:i+16])
        ascii_part = ''.join(chr(b) if 32 <= b <=
                             126 else '.' for b in data[i:i+16])
        print(f"{i:08x}: {hex_part:<48} |{ascii_part}|")


def main():
    print("FlipperRNG Test Script")
    print("=====================")

    # Find Flipper device
    ser = find_flipper_device()
    if not ser:
        print("Error: Could not find Flipper device")
        print("Make sure:")
        print("1. Flipper is connected via USB")
        print("2. FlipperRNG app is running")
        print("3. Output mode is set to USB")
        print("4. Generator is started")
        return 1

    try:
        print("Collecting random data... (10 seconds)")
        data = bytearray()
        start_time = time.time()

        while time.time() - start_time < 10:
            chunk = ser.read(1024)
            if chunk:
                data.extend(chunk)
                print(f"\rCollected {len(data)} bytes...", end='', flush=True)
            time.sleep(0.1)

        print(f"\nCollection complete: {len(data)} bytes")

        if len(data) == 0:
            print("No data received! Check that:")
            print("1. FlipperRNG generator is started")
            print("2. Output mode is set to USB")
            return 1

        # Show hex dump
        hex_dump(data)

        # Run basic tests
        basic_randomness_tests(data)

        # Save data to file for further analysis
        filename = f"flipper_rng_data_{int(time.time())}.bin"
        with open(filename, 'wb') as f:
            f.write(data)
        print(f"\nData saved to: {filename}")
        print(f"You can analyze this file with: rngtest < {filename}")

    except KeyboardInterrupt:
        print("\nInterrupted by user")
    except Exception as e:
        print(f"Error: {e}")
    finally:
        ser.close()

    return 0


if __name__ == "__main__":
    sys.exit(main())
