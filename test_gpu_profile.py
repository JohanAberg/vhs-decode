"""
Quick GPU profiling test script
Run directly with the working decode.py environment
"""

# Test by adding profiling to a simple decode run
import sys
import subprocess

# Run with profiling enabled
cmd = [
    sys.executable,
    "decode.py",
    "vhs",
    "--system", "PAL",
    "-f", "40",
    "--threads", "1",
    "--overwrite",
    "--length", "50",  # More frames for better profiling
    "--gpu",
    "--gpu-profile",
    "sample_data/out2.u8",
    "out2_profile"
]

print("Running GPU profiling test...")
print(" ".join(cmd))
print()

result = subprocess.run(cmd, capture_output=False)
sys.exit(result.returncode)
