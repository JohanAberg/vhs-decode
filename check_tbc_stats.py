import numpy as np
import sys

filename = "out1_cpp_chroma_test_chroma.tbc"
try:
    data = np.fromfile(filename, dtype=np.uint16)
    print(f"File: {filename}")
    print(f"Size: {len(data)} samples")
    print(f"Min: {np.min(data)}")
    print(f"Max: {np.max(data)}")
    print(f"Mean: {np.mean(data)}")
    print(f"Std Dev: {np.std(data)}")
    
    # Check distribution around 32768
    centered = data.astype(np.float32) - 32768
    print(f"Centered Min: {np.min(centered)}")
    print(f"Centered Max: {np.max(centered)}")
    print(f"Centered Mean: {np.mean(centered)}")
    
except Exception as e:
    print(f"Error reading file: {e}")
