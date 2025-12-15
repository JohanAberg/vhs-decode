# Phase 1 Task 1: Convert Envelope Filter from IIR to FIR

**Priority:** CRITICAL (19.7% of total time)  
**Expected Gain:** 5-8x speedup on envelope filtering  
**Estimated Time:** 2 days  
**Difficulty:** Medium

## Problem Statement

The envelope filter (FEnvPost) is an IIR/SOS filter that **cannot** be implemented using FFT multiplication in the frequency domain. This forces a CPU fallback using `scipy.signal.sosfiltfilt()`, which:

1. Transfers data GPU→CPU (expensive)
2. Runs sequential IIR filter on CPU (slow, single-threaded)
3. Transfers result CPU→GPU (expensive)

**Current cost:** 1.306ms per block × 2710 blocks = **3.54 seconds (19.7% of total)**

## Solution Approach

Convert the IIR filter to a **FIR filter approximation** that can be applied in the frequency domain:

```python
# Current (IIR, must use CPU):
env = scipy.signal.sosfiltfilt(FEnvPost_sos, envelope_data)

# Target (FIR, can use GPU FFT):
env_fft = cp.fft.rfft(envelope_data_gpu)
env_filtered_gpu = env_fft * FEnvPost_FIR_freq  # Frequency-domain multiplication
env = cp.fft.irfft(env_filtered_gpu).real
```

## Implementation Steps

### Step 1: Analyze Current IIR Filter Characteristics

**File:** `vhsdecode/formats.py` or `compute_video_filters.py`  
**Task:** Extract FEnvPost filter specification

1. Find where FEnvPost is defined (likely butter/cheby/ellip filter)
2. Extract:
   - Filter order
   - Cutoff frequencies
   - Filter type (lowpass/highpass/bandpass)
   - Passband/stopband specs

**Expected format:**
```python
# Likely something like:
FEnvPost_sos = scipy.signal.butter(order=4, Wn=cutoff_freq, btype='lowpass', output='sos')
```

### Step 2: Design FIR Approximation

**Tool:** `scipy.signal.firwin2()` or `scipy.signal.firls()`

**Method:**
1. Get frequency response of IIR filter
2. Design FIR filter matching that response
3. Choose FIR length (start with 2x IIR order, tune for performance)

**Code:**
```python
import scipy.signal as sps
import numpy as np

def design_fir_envelope_filter(iir_sos, sample_rate, fir_length=101):
    """
    Convert IIR SOS filter to FIR approximation.
    
    Args:
        iir_sos: IIR filter in SOS format
        sample_rate: Sampling rate in Hz
        fir_length: Length of FIR filter (odd number preferred)
    
    Returns:
        fir_coeffs: FIR filter coefficients
        freq_response: Frequency-domain representation for GPU
    """
    # Get IIR frequency response
    w, h = sps.sosfreqz(iir_sos, worN=2048, fs=sample_rate)
    
    # Design FIR filter matching this response
    fir_coeffs = sps.firwin2(fir_length, w, abs(h), fs=sample_rate)
    
    # Compute frequency-domain representation for GPU
    fir_fft = np.fft.rfft(fir_coeffs, n=blocklen)  # Match block size
    
    return fir_coeffs, fir_fft


# Usage in VHSRFDecode.__init__():
if use_gpu:
    # Convert IIR envelope filter to FIR
    self.FEnvPost_FIR, self.FEnvPost_FIR_freq = design_fir_envelope_filter(
        self.Filters["FEnvPost"],  # Original IIR SOS
        self.freq_hz,
        fir_length=101
    )
    
    # Transfer to GPU
    self.FEnvPost_FIR_freq_gpu = cp.asarray(self.FEnvPost_FIR_freq)
```

### Step 3: Modify GPU Processing Path

**File:** `vhsdecode/process_gpu.py`  
**Function:** `_demodblock_gpu_optimized()`  
**Lines:** 515-520

**Before:**
```python
# Envelope post-filter (CPU - IIR/SOS filter)
if profiler:
    profiler.start("5_envelope_filter_cpu")

env = transfer_from_gpu(env_gpu)
env = scipy.signal.sosfiltfilt(
    self.Filters["FEnvPost"][0],
    self.Filters["FEnvPost"][1],
    env
)
env_gpu = transfer_to_gpu(env)

if profiler:
    profiler.stop()
```

**After:**
```python
# Envelope post-filter (GPU - FIR filter in frequency domain)
if profiler:
    profiler.start("5_envelope_filter_gpu")

# Apply FIR filter using FFT convolution
env_fft_gpu = cp.fft.rfft(env_gpu)
env_filtered_fft_gpu = env_fft_gpu * self.FEnvPost_FIR_freq_gpu
env_gpu = cp.fft.irfft(env_filtered_fft_gpu).real

if profiler:
    profiler.stop()
```

### Step 4: Validate Frequency Response

**Test:** Ensure FIR approximation matches IIR closely

```python
def validate_filter_approximation():
    """Compare IIR and FIR frequency responses."""
    import matplotlib.pyplot as plt
    
    # IIR response
    w_iir, h_iir = sps.sosfreqz(FEnvPost_sos, worN=2048, fs=sample_rate)
    
    # FIR response
    w_fir, h_fir = sps.freqz(FEnvPost_FIR, worN=2048, fs=sample_rate)
    
    # Plot comparison
    plt.figure(figsize=(12, 6))
    
    plt.subplot(1, 2, 1)
    plt.plot(w_iir, 20*np.log10(abs(h_iir)), label='IIR (original)', linewidth=2)
    plt.plot(w_fir, 20*np.log10(abs(h_fir)), label='FIR (approximation)', linestyle='--')
    plt.xlabel('Frequency (Hz)')
    plt.ylabel('Magnitude (dB)')
    plt.title('Magnitude Response')
    plt.legend()
    plt.grid(True)
    
    plt.subplot(1, 2, 2)
    plt.plot(w_iir, np.angle(h_iir), label='IIR (original)', linewidth=2)
    plt.plot(w_fir, np.angle(h_fir), label='FIR (approximation)', linestyle='--')
    plt.xlabel('Frequency (Hz)')
    plt.ylabel('Phase (radians)')
    plt.title('Phase Response')
    plt.legend()
    plt.grid(True)
    
    plt.tight_layout()
    plt.savefig('envelope_filter_comparison.png', dpi=150)
    
    # Calculate error metrics
    mag_error = np.mean(np.abs(abs(h_iir) - abs(h_fir)))
    phase_error = np.mean(np.abs(np.angle(h_iir) - np.angle(h_fir)))
    
    print(f"Magnitude Error (mean): {mag_error:.6f}")
    print(f"Phase Error (mean): {phase_error:.6f} radians")
    
    assert mag_error < 0.01, f"Magnitude error too high: {mag_error}"
    assert phase_error < 0.1, f"Phase error too high: {phase_error}"
```

### Step 5: Test Output Quality

**Comparison:** Decode same video with IIR (CPU) vs FIR (GPU)

```bash
# CPU with IIR (original)
python decode.py vhs --system PAL --threads 8 --length 200 sample_data/out2.u8 cpu_iir

# GPU with FIR (new)
python decode.py vhs --system PAL --gpu --threads 1 --length 200 sample_data/out2.u8 gpu_fir

# Compare using TBC tools
ld-analyse cpu_iir.tbc gpu_fir.tbc

# Compute PSNR
python -c "
import numpy as np
from skimage.metrics import peak_signal_noise_ratio

cpu_data = np.fromfile('cpu_iir.tbc', dtype=np.uint16)
gpu_data = np.fromfile('gpu_fir.tbc', dtype=np.uint16)

psnr = peak_signal_noise_ratio(cpu_data, gpu_data, data_range=65535)
print(f'PSNR: {psnr:.2f} dB')

# Should be >40 dB for visually identical
assert psnr > 40, f'PSNR too low: {psnr:.2f} dB'
"
```

### Step 6: Benchmark Performance

**Profile before and after:**

```bash
# Before (IIR on CPU)
python decode.py vhs --gpu --gpu-profile --length 50 sample_data/out2.u8 before
# Expected: envelope_filter_cpu = 19.7% @ 1.306ms/block

# After (FIR on GPU)  
python decode.py vhs --gpu --gpu-profile --length 50 sample_data/out2.u8 after
# Expected: envelope_filter_gpu = 2-3% @ 0.15-0.20ms/block

# Calculate speedup
python -c "
before_ms = 1.306
after_ms = 0.18  # Expected
speedup = before_ms / after_ms
print(f'Envelope filter speedup: {speedup:.1f}x')
print(f'Overall time saved: {(before_ms - after_ms) * 2710 / 1000:.2f} seconds')
"
```

## Expected Performance Gain

**Before:**
- Envelope filter: 3.54s (19.7% of 17.92s total)
- Total decode: 17.92s (2.11 FPS)

**After:**
- Envelope filter: 0.45s (2.5% of total) - **7.9x faster**
- Total decode: 14.83s (3.37 FPS) - **1.6x overall speedup**

**vs CPU (3.11 FPS):** Now **1.08x faster** (was 0.68x = slower!)

## Acceptance Criteria

✅ **Performance:**
- Envelope filter completes in <0.2ms per block
- Total FPS > 3.0 (faster than CPU baseline)

✅ **Quality:**
- PSNR > 40 dB vs IIR output
- Visual inspection shows no artifacts
- Frequency response error < 1%

✅ **Tests:**
- All existing GPU tests pass
- New test: compare IIR vs FIR output
- Benchmark: record improvement in benchmark_results.json

✅ **Documentation:**
- Update GPU_DEEP_PROFILING_RESULTS.md with new numbers
- Document filter conversion in code comments
- Add envelope_filter_comparison.png to docs/

## Risks & Mitigation

**Risk 1:** FIR approximation not accurate enough
- **Mitigation:** Increase FIR length (try 101, 201, 301 taps)
- **Fallback:** Use Parks-McClellan (firpm) for optimal approximation

**Risk 2:** FIR filter adds too much latency/artifacts
- **Mitigation:** Use zero-phase filtering (filtfilt equivalent in FFT domain)
- **Fallback:** Implement GPU IIR filter instead (Plan B)

**Risk 3:** Performance gain less than expected
- **Mitigation:** Profile to identify if other operations now dominate
- **Adjust:** Move to next bottleneck (chroma processing)

## Next Steps After Completion

Once envelope filter is on GPU:

1. Move to **Task 2: GPU Chroma Processing** (18.1% of time)
2. Continue with **Task 3: Optimize FM Demod** (35.0% of time)
3. Expected cumulative gain: **3x faster than current, 2x faster than CPU**

---

**Ready to implement?** Start with Step 1 (analyze current filter).
