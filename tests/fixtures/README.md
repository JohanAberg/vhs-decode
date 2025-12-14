# Test Fixtures for VHS-Decode

This directory contains test data for validating the VHS-Decode implementation.

## GPU Validation Test Data

The `gpu_validation/` directory should contain reference test captures for validating GPU-accelerated decoding.

### Required Test Captures

For comprehensive GPU validation, the following test captures are needed:

1. **VHS NTSC Color Bars** (10 seconds)
   - File: `vhs_ntsc_colorbars.lds`
   - Reference: `vhs_ntsc_colorbars.tbc` (CPU baseline)
   - Metadata: `vhs_ntsc_colorbars.tbc.json`
   - Purpose: Standard NTSC signal validation

2. **VHS PAL Test Card** (10 seconds)
   - File: `vhs_pal_testcard.lds`
   - Reference: `vhs_pal_testcard.tbc` (CPU baseline)
   - Metadata: `vhs_pal_testcard.tbc.json`
   - Purpose: Standard PAL signal validation

3. **SVHS NTSC Hi-Fi** (10 seconds)
   - File: `svhs_ntsc_hifi.lds`
   - Reference: `svhs_ntsc_hifi.tbc` (CPU baseline)
   - Metadata: `svhs_ntsc_hifi.tbc.json`
   - Purpose: High-quality SVHS validation

4. **Betamax NTSC** (10 seconds, if available)
   - File: `betamax_ntsc.lds`
   - Reference: `betamax_ntsc.tbc` (CPU baseline)
   - Metadata: `betamax_ntsc.tbc.json`
   - Purpose: Betamax format validation

5. **VHS with Dropouts** (10 seconds)
   - File: `vhs_dropouts.lds`
   - Reference: `vhs_dropouts.tbc` (CPU baseline)
   - Metadata: `vhs_dropouts.tbc.json`
   - Purpose: Dropout handling validation

6. **VHS with Weak Signal** (10 seconds)
   - File: `vhs_weak_signal.lds`
   - Reference: `vhs_weak_signal.tbc` (CPU baseline)
   - Metadata: `vhs_weak_signal.tbc.json`
   - Purpose: Low SNR scenario validation

### Test Data Sources

Due to the large size of test captures (5-10 GB total), they are not included in the repository. Test data can be obtained from:

1. **vhs-decode Discord Community**: https://discord.gg/pVVrrxd
2. **DomesDay86 Project**: https://www.domesday86.com
3. **ld-decode Test Suite**: Existing test captures from the ld-decode project

### Generating Reference Outputs

Reference TBC files should be generated using the current CPU-based decoder:

```bash
# Generate reference output for a test capture
vhs-decode --system NTSC input_capture.lds output_reference

# This creates:
# - output_reference.tbc       (video data)
# - output_reference.tbc.json  (metadata)
```

### Test Data Format

- **Input**: Raw RF captures in `.lds` or `.ldf` format
- **Reference Output**: TBC (Time Base Corrected) video in `.tbc` format
- **Metadata**: JSON files with `.tbc.json` extension

### Using Test Data in Tests

```python
import pytest
from pathlib import Path

TEST_DATA_DIR = Path(__file__).parent / "fixtures" / "gpu_validation"

@pytest.mark.skipif(not (TEST_DATA_DIR / "vhs_ntsc_colorbars.lds").exists(),
                    reason="Test data not available")
def test_with_real_capture():
    input_file = TEST_DATA_DIR / "vhs_ntsc_colorbars.lds"
    # Test implementation
```

## Small Test Data

For unit tests and quick validation, small synthetic test signals are included:

- `PAL_GOOD.txt.gz` (in root): Clean PAL signal sample
- `PAL_NOISY.txt.gz` (in root): Noisy PAL signal sample

These can be used for quick tests without requiring large captures.

## Adding New Test Data

When adding new test data:

1. Document the source and format in this README
2. Include generation instructions if applicable
3. Add reference outputs generated with the CPU decoder
4. Update test files to use the new data
5. Keep files as small as practical (10-30 seconds maximum)
