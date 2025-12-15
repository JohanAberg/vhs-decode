import numpy as np
import sys

def compare_files(file1, file2):
    print(f"Comparing {file1} and {file2}...")
    
    # Read as uint16 (assuming 16-bit video data)
    try:
        d1 = np.fromfile(file1, dtype=np.uint16)
        d2 = np.fromfile(file2, dtype=np.uint16)
    except Exception as e:
        print(f"Error reading files: {e}")
        return

    if d1.shape != d2.shape:
        print(f"Shape mismatch: {d1.shape} vs {d2.shape}")
        return

    # Compare
    diff = np.abs(d1.astype(np.int32) - d2.astype(np.int32))
    max_diff = np.max(diff)
    mean_diff = np.mean(diff)
    
    print(f"Max difference: {max_diff}")
    print(f"Mean difference: {mean_diff:.4f}")
    
    if max_diff == 0:
        print("Files are IDENTICAL.")
    else:
        print("Files differ.")
        # Count significant differences
        significant = np.sum(diff > 1)
        print(f"Pixels with diff > 1: {significant} ({significant/d1.size*100:.2f}%)")

if __name__ == "__main__":
    if len(sys.argv) > 2:
        compare_files(sys.argv[1], sys.argv[2])
    else:
        compare_files("compare_cpu.tbc", "compare_gpu.tbc")
