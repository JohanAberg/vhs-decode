import numpy as np
import sys

def check_burst_rms(filename, width=1135, burst_start=94, burst_end=150):
    data = np.fromfile(filename, dtype=np.uint16)
    num_lines = len(data) // width
    data = data[:num_lines*width].reshape((num_lines, width))
    
    print(f"Checking {filename}")
    print(f"Dimensions: {num_lines} lines x {width} samples")
    
    bursts = []
    peak_locs = []
    for i in range(min(100, num_lines)):
        line = data[i].astype(float) - 32768.0
        
        # Scan for burst location
        best_rms = 0
        best_loc = 0
        window = 40
        for j in range(0, 300): # Scan first 300 samples
            slice_ = line[j:j+window]
            ac = slice_ - np.mean(slice_)
            rms = np.sqrt(np.mean(ac**2))
            if rms > best_rms:
                best_rms = rms
                best_loc = j
        
        bursts.append(best_rms)
        peak_locs.append(best_loc)
        
    avg_rms = np.mean(bursts)
    avg_loc = np.mean(peak_locs)
    print(f"Average Peak RMS (first 100 lines): {avg_rms:.2f}")
    print(f"Average Burst Location: {avg_loc:.1f}")
    print(f"First 5 RMS: {[f'{x:.2f}' for x in bursts[:5]]}")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        check_burst_rms(sys.argv[1])
    else:
        print("Usage: python check_burst.py <tbc_file>")
