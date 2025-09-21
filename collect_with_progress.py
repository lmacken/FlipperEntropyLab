#!/usr/bin/env python3
"""
Progress-tracked data collection for FlipperRNG testing
"""

import sys
import time
from tqdm import tqdm

def collect_data(input_file, output_file, target_size, description):
    """Collect data with progress bar"""
    print(f"Collecting {description}...")
    
    with open(input_file, 'rb') as infile, open(output_file, 'wb') as outfile:
        with tqdm(total=target_size, unit='B', unit_scale=True, desc=description) as pbar:
            collected = 0
            start_time = time.time()
            
            while collected < target_size:
                # Read in chunks
                chunk_size = min(1024, target_size - collected)
                chunk = infile.read(chunk_size)
                
                if not chunk:
                    break
                    
                outfile.write(chunk)
                collected += len(chunk)
                pbar.update(len(chunk))
                
                # Update rate in description
                elapsed = time.time() - start_time
                if elapsed > 0:
                    rate = collected / elapsed
                    pbar.set_postfix(rate=f"{rate:.1f} B/s")
            
            elapsed = time.time() - start_time
            final_rate = collected / elapsed if elapsed > 0 else 0
            
    return collected, elapsed, final_rate

if __name__ == "__main__":
    if len(sys.argv) != 5:
        print("Usage: collect_with_progress.py <input> <output> <size> <description>")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    target_size = int(sys.argv[3])
    description = sys.argv[4]
    
    collected, elapsed, rate = collect_data(input_file, output_file, target_size, description)
    
    print(f"Collected: {collected} bytes in {elapsed:.1f}s = {rate:.1f} bytes/sec")
