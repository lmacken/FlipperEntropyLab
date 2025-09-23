#!/usr/bin/env python3
"""
Compare entropy quality between ESP32 External RNG and FlipperRNG
Tests both sources side-by-side for quality comparison
"""

import serial
import sys
import time
import subprocess
import argparse
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import threading

# Try to import tqdm for progress bars
try:
    from tqdm import tqdm
    HAVE_TQDM = True
except ImportError:
    HAVE_TQDM = False
    print("Note: Install tqdm for better progress bars: pip install tqdm")

# ANSI colors for output
class Colors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'

def collect_data_from_port(port, size=102400, timeout=30, name="Device", position=0):
    """Collect random data from a serial port
    
    Args:
        port: Serial port path
        size: Exact number of bytes to collect
        timeout: Maximum time to wait (increased to 30s for slower devices)
        name: Device name for display
        position: Progress bar position (for parallel display)
    """
    
    try:
        ser = serial.Serial(port, 115200, timeout=1)
        data = b''
        start = time.time()
        last_data_time = time.time()
        
        if HAVE_TQDM:
            # Use tqdm progress bar
            pbar = tqdm(total=size, 
                       desc=f"{name:20}", 
                       unit="B", 
                       unit_scale=True,
                       unit_divisor=1024,
                       position=position,
                       leave=True,
                       bar_format='{desc}: {percentage:3.0f}%|{bar}| {n_fmt}/{total_fmt} [{rate_fmt}{postfix}]',
                       colour='green' if 'ESP32' in name else 'blue')
            
            while len(data) < size:
                # Check for timeout
                if time.time() - start > timeout:
                    pbar.set_postfix_str("TIMEOUT", refresh=True)
                    break
                
                # Check for stall (no data for 5 seconds)
                if time.time() - last_data_time > 5:
                    pbar.set_postfix_str("STALLED", refresh=True)
                    break
                
                if ser.in_waiting:
                    # Read only what we need (don't over-read)
                    bytes_needed = size - len(data)
                    chunk = ser.read(min(ser.in_waiting, bytes_needed))
                    data += chunk
                    pbar.update(len(chunk))
                    last_data_time = time.time()
                time.sleep(0.001)
            
            pbar.close()
        else:
            # Fallback to simple progress
            print(f"{Colors.OKCYAN}Collecting exactly {size} bytes from {name} ({port})...{Colors.ENDC}")
            while len(data) < size:
                # Check for timeout
                if time.time() - start > timeout:
                    print(f"\n{Colors.WARNING}Timeout reached{Colors.ENDC}")
                    break
                
                # Check for stall
                if time.time() - last_data_time > 5:
                    print(f"\n{Colors.WARNING}Connection stalled{Colors.ENDC}")
                    break
                    
                if ser.in_waiting:
                    bytes_needed = size - len(data)
                    chunk = ser.read(min(ser.in_waiting, bytes_needed))
                    data += chunk
                    last_data_time = time.time()
                    # Show progress
                    progress = len(data) * 100 // size
                    sys.stdout.write(f'\r{name}: {progress}% ({len(data)}/{size} bytes)')
                    sys.stdout.flush()
                time.sleep(0.01)
            print()
        
        ser.close()
        
        # Calculate actual data rate
        elapsed = time.time() - start
        data_rate = len(data) / elapsed / 1024  # KB/s
        
        if HAVE_TQDM:
            # Use tqdm.write() to properly display completion message
            if len(data) == size:
                completion_msg = f"{Colors.OKGREEN}✓ {name:20}: Collected exactly {size} bytes @ {data_rate:.1f} KB/s{Colors.ENDC}"
            else:
                completion_msg = f"{Colors.WARNING}⚠ {name:20}: Collected only {len(data)}/{size} bytes @ {data_rate:.1f} KB/s{Colors.ENDC}"
            
            tqdm.write(completion_msg)
        else:
            if len(data) == size:
                print(f"{Colors.OKGREEN}✓ {name}: Collected exactly {size} bytes @ {data_rate:.1f} KB/s{Colors.ENDC}")
            else:
                print(f"{Colors.WARNING}⚠ {name}: Collected only {len(data)}/{size} bytes{Colors.ENDC}")
        
        # Truncate to exact size if we somehow got more (shouldn't happen with our logic)
        return data[:size]
    except Exception as e:
        if HAVE_TQDM:
            tqdm.write(f"{Colors.FAIL}✗ {name:20}: Error - {e}{Colors.ENDC}")
        else:
            print(f"\n{Colors.FAIL}✗ {name}: Error - {e}{Colors.ENDC}")
        return None

def analyze_with_rngtest(data, name="Device"):
    """Analyze data with rngtest"""
    if not data:
        return None
    
    # Write to temp file
    temp_file = f'/tmp/{name.lower().replace(" ", "_")}_random.bin'
    with open(temp_file, 'wb') as f:
        f.write(data)
    
    try:
        result = subprocess.run(['rngtest'], 
                              stdin=open(temp_file, 'rb'),
                              capture_output=True, 
                              text=True,
                              timeout=5)
        
        # Parse results
        stats = {}
        lines = result.stderr.split('\n')
        for line in lines:
            if 'successes:' in line:
                parts = line.split()
                for i, part in enumerate(parts):
                    if part == 'successes:':
                        stats['successes'] = int(parts[i+1]) if i+1 < len(parts) else 0
            elif 'failures:' in line:
                parts = line.split()
                for i, part in enumerate(parts):
                    if part == 'failures:':
                        stats['failures'] = int(parts[i+1]) if i+1 < len(parts) else 0
        
        if 'successes' in stats and 'failures' in stats:
            total = stats['successes'] + stats['failures']
            stats['pass_rate'] = (stats['successes'] / total * 100) if total > 0 else 0
        
        return stats
    except Exception as e:
        print(f"{Colors.WARNING}Warning: rngtest failed for {name}: {e}{Colors.ENDC}")
        return None

def analyze_with_ent(data, name="Device"):
    """Analyze data with ent"""
    if not data:
        return None
    
    # Write to temp file
    temp_file = f'/tmp/{name.lower().replace(" ", "_")}_random.bin'
    with open(temp_file, 'wb') as f:
        f.write(data)
    
    try:
        result = subprocess.run(['ent', temp_file], 
                              capture_output=True, 
                              text=True,
                              timeout=5)
        
        # Parse results
        stats = {}
        lines = result.stdout.split('\n')
        for line in lines:
            if 'Entropy =' in line:
                parts = line.split()
                stats['entropy'] = float(parts[2]) if len(parts) > 2 else 0
            elif 'Chi square distribution' in line:
                if 'would exceed this value' in line:
                    percent_idx = line.find('value') + 6
                    percent_end = line.find('percent', percent_idx)
                    if percent_idx > 5 and percent_end > percent_idx:
                        try:
                            # Handle both "50.00" and "less than 0.01" formats
                            percent_str = line[percent_idx:percent_end].strip()
                            if 'less than' in percent_str:
                                stats['chi_square_percent'] = 0.01
                            else:
                                stats['chi_square_percent'] = float(percent_str)
                        except:
                            pass
            elif 'Arithmetic mean' in line:
                parts = line.split()
                for i, part in enumerate(parts):
                    if part == 'is' and i+1 < len(parts):
                        stats['mean'] = float(parts[i+1])
                        break
            elif 'Serial correlation coefficient' in line:
                parts = line.split()
                for i, part in enumerate(parts):
                    if part == 'is' and i+1 < len(parts):
                        stats['correlation'] = float(parts[i+1])
                        break
        
        return stats
    except Exception as e:
        print(f"{Colors.WARNING}Warning: ent failed for {name}: {e}{Colors.ENDC}")
        return None

def print_comparison_results(esp32_stats, flipper_stats, data_size=None):
    """Print side-by-side comparison of results"""
    print(f"\n{Colors.HEADER}{'='*80}")
    print(f"                    ENTROPY COMPARISON RESULTS")
    if data_size:
        print(f"                  Analyzing {data_size} bytes from each source")
    print(f"{'='*80}{Colors.ENDC}\n")
    
    # Create comparison table
    print(f"{Colors.BOLD}{'Metric':<30} {'ESP32 External':<25} {'FlipperRNG':<25}{Colors.ENDC}")
    print("-" * 80)
    
    # FIPS 140-2 Results
    if esp32_stats.get('rngtest') or flipper_stats.get('rngtest'):
        print(f"{Colors.OKCYAN}FIPS 140-2 Tests:{Colors.ENDC}")
        
        esp32_rng = esp32_stats.get('rngtest', {})
        flipper_rng = flipper_stats.get('rngtest', {})
        
        # Successes/Failures
        esp32_success = esp32_rng.get('successes', 'N/A')
        esp32_fail = esp32_rng.get('failures', 'N/A')
        flipper_success = flipper_rng.get('successes', 'N/A')
        flipper_fail = flipper_rng.get('failures', 'N/A')
        
        print(f"  {'Successes':<28} {str(esp32_success):<25} {str(flipper_success):<25}")
        print(f"  {'Failures':<28} {str(esp32_fail):<25} {str(flipper_fail):<25}")
        
        # Pass rate with color coding
        esp32_pass = esp32_rng.get('pass_rate', 0)
        flipper_pass = flipper_rng.get('pass_rate', 0)
        
        esp32_color = Colors.OKGREEN if esp32_pass >= 95 else Colors.WARNING if esp32_pass >= 90 else Colors.FAIL
        flipper_color = Colors.OKGREEN if flipper_pass >= 95 else Colors.WARNING if flipper_pass >= 90 else Colors.FAIL
        
        esp32_pass_str = f"{esp32_color}{esp32_pass:.1f}%{Colors.ENDC}" if esp32_pass else "N/A"
        flipper_pass_str = f"{flipper_color}{flipper_pass:.1f}%{Colors.ENDC}" if flipper_pass else "N/A"
        
        print(f"  {'Pass Rate':<28} {esp32_pass_str:<35} {flipper_pass_str:<35}")
    
    # ENT Analysis Results
    if esp32_stats.get('ent') or flipper_stats.get('ent'):
        print(f"\n{Colors.OKCYAN}Entropy Analysis (ENT):{Colors.ENDC}")
        
        esp32_ent = esp32_stats.get('ent', {})
        flipper_ent = flipper_stats.get('ent', {})
        
        # Entropy (bits per byte)
        esp32_entropy = esp32_ent.get('entropy', 0)
        flipper_entropy = flipper_ent.get('entropy', 0)
        
        esp32_ent_color = Colors.OKGREEN if esp32_entropy > 7.9 else Colors.WARNING if esp32_entropy > 7.5 else Colors.FAIL
        flipper_ent_color = Colors.OKGREEN if flipper_entropy > 7.9 else Colors.WARNING if flipper_entropy > 7.5 else Colors.FAIL
        
        esp32_ent_str = f"{esp32_ent_color}{esp32_entropy:.6f}{Colors.ENDC}" if esp32_entropy else "N/A"
        flipper_ent_str = f"{flipper_ent_color}{flipper_entropy:.6f}{Colors.ENDC}" if flipper_entropy else "N/A"
        
        print(f"  {'Entropy (bits/byte)':<28} {esp32_ent_str:<35} {flipper_ent_str:<35}")
        
        # Chi-square
        esp32_chi = esp32_ent.get('chi_square_percent', -1)
        flipper_chi = flipper_ent.get('chi_square_percent', -1)
        
        esp32_chi_str = f"{esp32_chi:.2f}%" if esp32_chi >= 0 else "N/A"
        flipper_chi_str = f"{flipper_chi:.2f}%" if flipper_chi >= 0 else "N/A"
        
        print(f"  {'Chi-square (% exceeded)':<28} {esp32_chi_str:<25} {flipper_chi_str:<25}")
        
        # Mean
        esp32_mean = esp32_ent.get('mean', 0)
        flipper_mean = flipper_ent.get('mean', 0)
        
        esp32_mean_str = f"{esp32_mean:.4f}" if esp32_mean else "N/A"
        flipper_mean_str = f"{flipper_mean:.4f}" if flipper_mean else "N/A"
        
        print(f"  {'Mean (ideal: 127.5)':<28} {esp32_mean_str:<25} {flipper_mean_str:<25}")
        
        # Serial correlation
        esp32_corr = esp32_ent.get('correlation', 999)
        flipper_corr = flipper_ent.get('correlation', 999)
        
        esp32_corr_str = f"{esp32_corr:.6f}" if esp32_corr != 999 else "N/A"
        flipper_corr_str = f"{flipper_corr:.6f}" if flipper_corr != 999 else "N/A"
        
        print(f"  {'Serial Correlation':<28} {esp32_corr_str:<25} {flipper_corr_str:<25}")
    
    # Summary
    print(f"\n{Colors.HEADER}Summary:{Colors.ENDC}")
    
    # Determine winner for each category
    winners = []
    
    if esp32_stats.get('rngtest') and flipper_stats.get('rngtest'):
        esp32_pass = esp32_stats['rngtest'].get('pass_rate', 0)
        flipper_pass = flipper_stats['rngtest'].get('pass_rate', 0)
        if esp32_pass > flipper_pass:
            winners.append(f"  • FIPS 140-2: {Colors.OKGREEN}ESP32 wins{Colors.ENDC} ({esp32_pass:.1f}% vs {flipper_pass:.1f}%)")
        elif flipper_pass > esp32_pass:
            winners.append(f"  • FIPS 140-2: {Colors.OKGREEN}FlipperRNG wins{Colors.ENDC} ({flipper_pass:.1f}% vs {esp32_pass:.1f}%)")
        else:
            winners.append(f"  • FIPS 140-2: {Colors.OKCYAN}Tie{Colors.ENDC} ({esp32_pass:.1f}%)")
    
    if esp32_stats.get('ent') and flipper_stats.get('ent'):
        esp32_entropy = esp32_stats['ent'].get('entropy', 0)
        flipper_entropy = flipper_stats['ent'].get('entropy', 0)
        if esp32_entropy > flipper_entropy:
            winners.append(f"  • Entropy: {Colors.OKGREEN}ESP32 wins{Colors.ENDC} ({esp32_entropy:.4f} vs {flipper_entropy:.4f} bits/byte)")
        elif flipper_entropy > esp32_entropy:
            winners.append(f"  • Entropy: {Colors.OKGREEN}FlipperRNG wins{Colors.ENDC} ({flipper_entropy:.4f} vs {esp32_entropy:.4f} bits/byte)")
        else:
            winners.append(f"  • Entropy: {Colors.OKCYAN}Tie{Colors.ENDC} ({esp32_entropy:.4f} bits/byte)")
    
    for winner in winners:
        print(winner)
    
    print(f"\n{Colors.BOLD}Combined Performance:{Colors.ENDC}")
    print(f"  When ESP32 is connected to FlipperRNG as external entropy source,")
    print(f"  the combined system will have even better randomness quality!")
    print(f"\n{'='*80}\n")

def main():
    parser = argparse.ArgumentParser(description='Compare ESP32 External RNG with FlipperRNG')
    parser.add_argument('--esp32', default='/dev/ttyACM0', help='ESP32 serial port')
    parser.add_argument('--flipper', default='/dev/ttyUSB0', help='FlipperRNG serial port')
    parser.add_argument('-s', '--size', type=int, default=102400, help='Bytes to collect (default: 100KB)')
    parser.add_argument('--no-flipper', action='store_true', help='Test ESP32 only')
    parser.add_argument('--no-esp32', action='store_true', help='Test FlipperRNG only')
    
    args = parser.parse_args()
    
    print(f"{Colors.HEADER}{'='*80}")
    print(f"     ESP32 External RNG vs FlipperRNG - Entropy Comparison")
    print(f"{'='*80}{Colors.ENDC}\n")
    
    # Check which devices to test
    test_esp32 = not args.no_esp32 and Path(args.esp32).exists()
    test_flipper = not args.no_flipper and Path(args.flipper).exists()
    
    if not test_esp32 and not test_flipper:
        print(f"{Colors.FAIL}No devices found to test!{Colors.ENDC}")
        print(f"ESP32 ({args.esp32}): {'Found' if Path(args.esp32).exists() else 'Not found'}")
        print(f"FlipperRNG ({args.flipper}): {'Found' if Path(args.flipper).exists() else 'Not found'}")
        return 1
    
    # Collect data from both sources
    esp32_data = None
    flipper_data = None
    
    if test_esp32 and test_flipper:
        print(f"{Colors.BOLD}Collecting data from both sources in parallel...{Colors.ENDC}\n")
        if HAVE_TQDM:
            # Reserve space for progress bars
            print()  # ESP32 progress bar line
            print()  # FlipperRNG progress bar line
        
        with ThreadPoolExecutor(max_workers=2) as executor:
            esp32_future = executor.submit(collect_data_from_port, args.esp32, args.size, 30, "ESP32 External", 2 if HAVE_TQDM else 0)
            flipper_future = executor.submit(collect_data_from_port, args.flipper, args.size, 30, "FlipperRNG", 1 if HAVE_TQDM else 0)
            
            esp32_data = esp32_future.result()
            flipper_data = flipper_future.result()
            
        if HAVE_TQDM:
            print()  # Add spacing after progress bars
        
        # Ensure we compare the same amount of data
        if esp32_data and flipper_data:
            min_size = min(len(esp32_data), len(flipper_data))
            if min_size < args.size:
                print(f"{Colors.WARNING}Note: Comparing {min_size} bytes (minimum collected from both devices){Colors.ENDC}")
                esp32_data = esp32_data[:min_size]
                flipper_data = flipper_data[:min_size]
    elif test_esp32:
        print(f"{Colors.BOLD}Testing ESP32 External RNG only...{Colors.ENDC}\n")
        if HAVE_TQDM:
            print()  # Reserve space for progress bar
        esp32_data = collect_data_from_port(args.esp32, args.size, 30, "ESP32 External", 1 if HAVE_TQDM else 0)
        if HAVE_TQDM:
            print()
    else:
        print(f"{Colors.BOLD}Testing FlipperRNG only...{Colors.ENDC}\n")
        if HAVE_TQDM:
            print()  # Reserve space for progress bar
        flipper_data = collect_data_from_port(args.flipper, args.size, 30, "FlipperRNG", 1 if HAVE_TQDM else 0)
        if HAVE_TQDM:
            print()
    
    # Analyze the data
    esp32_stats = {}
    flipper_stats = {}
    
    print(f"\n{Colors.BOLD}Analyzing randomness quality...{Colors.ENDC}\n")
    
    if esp32_data:
        print(f"{Colors.OKCYAN}Analyzing ESP32 External RNG...{Colors.ENDC}")
        esp32_stats['rngtest'] = analyze_with_rngtest(esp32_data, "ESP32")
        esp32_stats['ent'] = analyze_with_ent(esp32_data, "ESP32")
    
    if flipper_data:
        print(f"{Colors.OKCYAN}Analyzing FlipperRNG...{Colors.ENDC}")
        flipper_stats['rngtest'] = analyze_with_rngtest(flipper_data, "Flipper")
        flipper_stats['ent'] = analyze_with_ent(flipper_data, "Flipper")
    
    # Print comparison results
    if test_esp32 and test_flipper:
        # Determine actual data size analyzed
        analyzed_size = None
        if esp32_data and flipper_data:
            analyzed_size = min(len(esp32_data), len(flipper_data))
        print_comparison_results(esp32_stats, flipper_stats, analyzed_size)
    elif test_esp32:
        print(f"\n{Colors.HEADER}ESP32 External RNG Results:{Colors.ENDC}")
        if esp32_stats.get('rngtest'):
            print(f"FIPS 140-2: {esp32_stats['rngtest'].get('successes', 0)} successes, "
                  f"{esp32_stats['rngtest'].get('failures', 0)} failures "
                  f"({esp32_stats['rngtest'].get('pass_rate', 0):.1f}% pass rate)")
        if esp32_stats.get('ent'):
            print(f"Entropy: {esp32_stats['ent'].get('entropy', 0):.6f} bits/byte")
            print(f"Mean: {esp32_stats['ent'].get('mean', 0):.4f}")
            print(f"Serial Correlation: {esp32_stats['ent'].get('correlation', 0):.6f}")
    else:
        print(f"\n{Colors.HEADER}FlipperRNG Results:{Colors.ENDC}")
        if flipper_stats.get('rngtest'):
            print(f"FIPS 140-2: {flipper_stats['rngtest'].get('successes', 0)} successes, "
                  f"{flipper_stats['rngtest'].get('failures', 0)} failures "
                  f"({flipper_stats['rngtest'].get('pass_rate', 0):.1f}% pass rate)")
        if flipper_stats.get('ent'):
            print(f"Entropy: {flipper_stats['ent'].get('entropy', 0):.6f} bits/byte")
            print(f"Mean: {flipper_stats['ent'].get('mean', 0):.4f}")
            print(f"Serial Correlation: {flipper_stats['ent'].get('correlation', 0):.6f}")
    
    return 0

if __name__ == '__main__':
    sys.exit(main())
