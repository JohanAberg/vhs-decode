#!/usr/bin/env python3
"""
Compare two TBC outputs and visualize differences
"""

import sys
import struct
import numpy as np
import json
from pathlib import Path

def read_tbc_metadata(tbc_json_path):
    """Read TBC metadata from JSON file"""
    with open(tbc_json_path, 'r') as f:
        return json.load(f)

def read_tbc_field(tbc_path, field_index=0):
    """Read a single field from TBC file (16-bit unsigned)"""
    with open(tbc_path, 'rb') as f:
        # Read as 16-bit unsigned integers
        data = np.fromfile(f, dtype=np.uint16)
    
    # Assume 1135 samples per line, 312-313 lines per field (PAL)
    samples_per_line = 1135
    lines_per_field = 312
    samples_per_field = samples_per_line * lines_per_field
    
    # Extract first field
    if len(data) >= samples_per_field:
        field_data = data[:samples_per_field].reshape(lines_per_field, samples_per_line)
        return field_data
    else:
        print(f"Warning: TBC file too small. Expected {samples_per_field}, got {len(data)}")
        return None

def compute_statistics(field):
    """Compute basic statistics"""
    return {
        'min': np.min(field),
        'max': np.max(field),
        'mean': np.mean(field),
        'std': np.std(field),
    }

def compare_fields(field1, field2):
    """Compare two fields and compute difference metrics"""
    if field1.shape != field2.shape:
        print(f"Error: Field shapes don't match: {field1.shape} vs {field2.shape}")
        return None
    
    # Compute differences
    diff = field1.astype(np.float32) - field2.astype(np.float32)
    abs_diff = np.abs(diff)
    
    # Normalize to 8-bit range for MAD calculation
    field1_8bit = (field1 / 256).astype(np.uint8)
    field2_8bit = (field2 / 256).astype(np.uint8)
    mad_8bit = np.mean(np.abs(field1_8bit.astype(np.float32) - field2_8bit.astype(np.float32)))
    
    # Compute correlation
    correlation = np.corrcoef(field1.flatten(), field2.flatten())[0, 1]
    
    return {
        'MAD': np.mean(abs_diff),
        'MAD_8bit': mad_8bit,
        'Max_diff': np.max(abs_diff),
        'RMS_diff': np.sqrt(np.mean(diff**2)),
        'Correlation': correlation,
    }

def save_field_as_png(field, output_path):
    """Save field as 8-bit grayscale PNG"""
    try:
        from PIL import Image
        # Convert 16-bit to 8-bit
        field_8bit = (field / 256).astype(np.uint8)
        img = Image.fromarray(field_8bit, mode='L')
        img.save(output_path)
        print(f"Saved visualization to {output_path}")
    except ImportError:
        print("PIL not available, skipping PNG output")

def main():
    if len(sys.argv) < 3:
        print("Usage: compare_outputs.py <baseline.tbc> <new.tbc> [output_prefix]")
        print("Example: compare_outputs.py /tmp/baseline.tbc /tmp/new.tbc /tmp/comparison")
        sys.exit(1)
    
    baseline_path = sys.argv[1]
    new_path = sys.argv[2]
    output_prefix = sys.argv[3] if len(sys.argv) > 3 else None
    
    print("=" * 70)
    print("TBC Output Comparison Tool")
    print("=" * 70)
    
    # Read baseline field
    print(f"\nReading baseline: {baseline_path}")
    baseline_field = read_tbc_field(baseline_path, 0)
    if baseline_field is None:
        sys.exit(1)
    baseline_stats = compute_statistics(baseline_field)
    print(f"  Shape: {baseline_field.shape}")
    print(f"  Min: {baseline_stats['min']}, Max: {baseline_stats['max']}")
    print(f"  Mean: {baseline_stats['mean']:.1f}, Std: {baseline_stats['std']:.1f}")
    
    # Read new field
    print(f"\nReading new output: {new_path}")
    new_field = read_tbc_field(new_path, 0)
    if new_field is None:
        sys.exit(1)
    new_stats = compute_statistics(new_field)
    print(f"  Shape: {new_field.shape}")
    print(f"  Min: {new_stats['min']}, Max: {new_stats['max']}")
    print(f"  Mean: {new_stats['mean']:.1f}, Std: {new_stats['std']:.1f}")
    
    # Compare
    print("\n" + "=" * 70)
    print("Comparison Results")
    print("=" * 70)
    comparison = compare_fields(baseline_field, new_field)
    if comparison:
        print(f"  MAD (16-bit): {comparison['MAD']:.2f}")
        print(f"  MAD (8-bit):  {comparison['MAD_8bit']:.2f}")
        print(f"  Max Diff:     {comparison['Max_diff']:.2f}")
        print(f"  RMS Diff:     {comparison['RMS_diff']:.2f}")
        print(f"  Correlation:  {comparison['Correlation']:.6f}")
        
        # Interpretation
        print("\n" + "-" * 70)
        print("Interpretation:")
        if comparison['MAD_8bit'] < 5:
            print("  ✓ EXCELLENT match (MAD < 5)")
        elif comparison['MAD_8bit'] < 15:
            print("  ✓ GOOD match (MAD < 15)")
        elif comparison['MAD_8bit'] < 30:
            print("  ⚠ FAIR match (MAD < 30) - some differences")
        else:
            print("  ✗ POOR match (MAD >= 30) - significant differences")
        
        if comparison['Correlation'] > 0.95:
            print("  ✓ EXCELLENT correlation (> 0.95)")
        elif comparison['Correlation'] > 0.85:
            print("  ✓ GOOD correlation (> 0.85)")
        else:
            print("  ⚠ LOW correlation - check alignment")
        print("=" * 70)
    
    # Save visualizations if requested
    if output_prefix:
        save_field_as_png(baseline_field, f"{output_prefix}_baseline.png")
        save_field_as_png(new_field, f"{output_prefix}_new.png")
        
        # Save difference image
        diff = np.abs(baseline_field.astype(np.float32) - new_field.astype(np.float32))
        diff_8bit = np.clip(diff / 256, 0, 255).astype(np.uint8)
        save_field_as_png(diff_8bit, f"{output_prefix}_diff.png")

if __name__ == '__main__':
    main()
