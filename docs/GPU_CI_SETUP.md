# Setting Up GPU Tests in GitHub CI

This guide explains how to run GPU tests in GitHub Actions CI.

## Current Status

The GPU tests are designed to work in two modes:

1. **CPU Fallback Mode** (works on standard GitHub runners)
2. **GPU Mode** (requires self-hosted runner with NVIDIA GPU)

## Option 1: CPU Fallback Testing (Easiest)

Standard GitHub-hosted runners don't have GPU hardware, but the GPU tests can still run in CPU fallback mode to verify:
- Code imports correctly
- GPU detection works
- Automatic fallback to CPU functions properly
- Unit tests pass in CPU mode

### Already Configured

The workflow `.github/workflows/gpu-tests.yml` includes a `cpu-tests` job that:
- Runs on standard `ubuntu-22.04` runners
- Installs all dependencies except CuPy
- Runs quick validation script
- Runs GPU unit tests (which auto-skip GPU-specific tests)
- Runs benchmark tests in CPU-only mode

### To Enable

The workflow is configured to run on:
- Pushes to `copilot/update-gpu-proposal-testing` branch
- Pull requests that modify GPU-related files

**No additional setup needed** - these tests will run automatically on GitHub's standard runners.

## Option 2: Full GPU Testing (Recommended for Production)

For full GPU testing with actual CUDA acceleration, you need a self-hosted runner with GPU hardware.

### Requirements

**Hardware:**
- Machine with NVIDIA GPU (Compute Capability 6.0+)
- Minimum 4GB VRAM recommended
- Ubuntu 20.04 or 22.04

**Software:**
- CUDA Toolkit 11.0+ or 12.0+
- NVIDIA drivers (compatible with CUDA version)
- Docker (optional, for containerized runners)

### Setup Steps

#### 1. Set Up Self-Hosted Runner

**On your GPU machine:**

```bash
# Create a directory for the runner
mkdir actions-runner && cd actions-runner

# Download the latest runner package (replace with current version)
curl -o actions-runner-linux-x64-2.311.0.tar.gz -L \
  https://github.com/actions/runner/releases/download/v2.311.0/actions-runner-linux-x64-2.311.0.tar.gz

# Extract the installer
tar xzf ./actions-runner-linux-x64-2.311.0.tar.gz

# Configure the runner
# Go to: https://github.com/JohanAberg/vhs-decode/settings/actions/runners/new
# Follow the instructions to get your token
./config.sh --url https://github.com/JohanAberg/vhs-decode --token YOUR_TOKEN

# Add labels for GPU runner
./config.sh --labels linux,gpu,cuda

# Install as a service (optional)
sudo ./svc.sh install
sudo ./svc.sh start
```

#### 2. Install CUDA on Runner

**Ubuntu 22.04:**

```bash
# Install NVIDIA drivers
sudo apt-get update
sudo apt-get install -y nvidia-driver-535

# Add CUDA repository
wget https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2204/x86_64/cuda-keyring_1.1-1_all.deb
sudo dpkg -i cuda-keyring_1.1-1_all.deb
sudo apt-get update

# Install CUDA Toolkit
sudo apt-get install -y cuda-toolkit-12-3

# Verify installation
nvidia-smi
nvcc --version
```

#### 3. Enable GPU Tests Workflow

In `.github/workflows/gpu-tests.yml`, uncomment the `gpu-tests` job (lines 54-143).

Update the `runs-on` line to match your runner labels:
```yaml
runs-on: [self-hosted, linux, gpu, cuda]
```

#### 4. Install Python Dependencies on Runner

The runner needs base Python packages:

```bash
sudo apt-get install -y python3 python3-pip python3-venv
pip3 install --upgrade pip setuptools wheel
```

The workflow will install project-specific dependencies automatically.

### Verify Setup

Push a commit to test:

```bash
# Make a small change to trigger the workflow
git commit --allow-empty -m "Test GPU CI"
git push
```

Check the Actions tab in GitHub to see the workflow run.

## Option 3: Docker-Based GPU Runner (Advanced)

Use Docker with NVIDIA Container Toolkit for isolated testing.

### Prerequisites

```bash
# Install Docker
curl -fsSL https://get.docker.com -o get-docker.sh
sudo sh get-docker.sh

# Install NVIDIA Container Toolkit
distribution=$(. /etc/os-release;echo $ID$VERSION_ID)
curl -s -L https://nvidia.github.io/nvidia-docker/gpgkey | sudo apt-key add -
curl -s -L https://nvidia.github.io/nvidia-docker/$distribution/nvidia-docker.list | \
  sudo tee /etc/apt/sources.list.d/nvidia-docker.list

sudo apt-get update
sudo apt-get install -y nvidia-container-toolkit
sudo systemctl restart docker

# Test GPU in Docker
docker run --rm --gpus all nvidia/cuda:12.0.0-base-ubuntu22.04 nvidia-smi
```

### Configure Runner

Create a Dockerfile for the runner:

```dockerfile
FROM nvidia/cuda:12.0.0-devel-ubuntu22.04

# Install GitHub Actions runner dependencies
RUN apt-get update && apt-get install -y \
    curl git build-essential python3 python3-pip \
    python3-numpy python3-scipy python3-numba \
    libfftw3-dev ffmpeg \
    && rm -rf /var/lib/apt/lists/*

# Install CuPy
RUN pip3 install cupy-cuda12x

# Install pytest
RUN pip3 install pytest pytest-benchmark pytest-timeout

# Set up runner (you'll need to configure this)
WORKDIR /actions-runner
```

## Testing Locally

Before setting up CI, test GPU code locally:

### Quick Validation
```bash
# Verify GPU detection
python tests/test_gpu_quick.py

# Should show:
# ✓ GPU utilities module imported successfully
# ✓ GPU detection test passed
# GPU Available: True (or False if no GPU)
```

### Run Tests
```bash
# Install test dependencies
pip install pytest pytest-benchmark

# Run unit tests
pytest tests/test_gpu_unit.py -v

# Run benchmarks
pytest tests/test_gpu_benchmark.py -v --benchmark-only
```

## What Tests Run in CI

### CPU Fallback Mode (Standard Runners)
- ✅ Import validation
- ✅ GPU detection (reports not available)
- ✅ Array module selection
- ⊘ GPU-specific tests (auto-skipped)
- ⊘ Performance benchmarks (limited value without GPU)

### GPU Mode (Self-Hosted Runner)
- ✅ Import validation
- ✅ GPU detection and info
- ✅ FFT equivalence tests
- ✅ Filtering tests
- ✅ Memory management tests
- ✅ Precision validation
- ✅ Performance benchmarks
- ✅ Speedup tracking
- ⊘ Integration tests (require test data)
- ⊘ Regression tests (require test data)

## Test Data for Integration Tests

Integration and regression tests require RF capture test data (~5-10 GB).

### Setup Test Data

```bash
# Create test data directory
mkdir -p tests/fixtures/gpu_validation

# Option 1: Use community samples
# Download from vhs-decode Discord or contact maintainers

# Option 2: Generate reference outputs
# Use your own captures:
vhs-decode input.lds tests/fixtures/gpu_validation/test_sample

# This creates:
# - test_sample.tbc (reference output)
# - test_sample.tbc.json (metadata)
```

See `tests/fixtures/README.md` for detailed test data requirements.

## Troubleshooting CI

### Runner Can't Find GPU

**Problem**: Tests skip with "GPU not available"

**Solutions:**
1. Verify NVIDIA drivers: `nvidia-smi`
2. Check CUDA installation: `nvcc --version`
3. Verify runner labels match workflow
4. Check runner logs: `~/actions-runner/_diag/`

### CuPy Installation Fails

**Problem**: `pip install cupy-cuda12x` fails

**Solutions:**
```bash
# Check CUDA version
nvcc --version

# Install matching CuPy version:
# CUDA 11.x: pip install cupy-cuda11x
# CUDA 12.x: pip install cupy-cuda12x

# Or build from source (takes longer):
pip install cupy
```

### Tests Timeout

**Problem**: Tests exceed timeout limits

**Solutions:**
- Increase timeout in workflow (e.g., `--timeout=600`)
- Check GPU is not thermal throttling: `nvidia-smi`
- Reduce test data size for CI
- Split tests into smaller jobs

### Out of Memory

**Problem**: `OutOfMemoryError` during tests

**Solutions:**
```bash
# Check available VRAM
nvidia-smi

# Reduce batch sizes in tests
# Close other GPU processes
# Use smaller test data
```

## Cost Considerations

### Standard GitHub Runners (Free)
- ✅ Free for public repositories
- ✅ 2,000 minutes/month for private repos
- ⊘ No GPU support
- ⊘ Limited testing capabilities

### Self-Hosted GPU Runner
- 💰 Hardware costs (GPU machine)
- 💰 Electricity costs
- 💰 Maintenance time
- ✅ Full GPU testing
- ✅ Unlimited minutes
- ✅ Better performance

### Cloud GPU Runner (Alternative)
- 💰 Per-minute charges (AWS, Azure, GCP)
- ✅ On-demand GPU access
- ✅ Full GPU testing
- ⚠️ Can be expensive for frequent runs

## Recommended Approach

For this project, we recommend:

1. **Phase 1** (Current): Use CPU fallback tests on standard runners
   - Validates code correctness
   - Catches import/syntax errors
   - Tests CPU fallback functionality
   - Free and easy to maintain

2. **Phase 2** (Future): Add self-hosted GPU runner for weekly/release testing
   - Full GPU validation
   - Performance benchmarking
   - Run on PRs to main branch only
   - Cost-effective for occasional testing

3. **Phase 3** (Production): Cloud GPU runners for critical paths
   - On-demand GPU testing
   - Triggered manually or on releases
   - Comprehensive validation before release

## Summary

**To run tests in GitHub CI right now:**
- ✅ Already configured! Push to `copilot/update-gpu-proposal-testing` branch
- ✅ CPU fallback tests run automatically on standard runners
- ✅ Tests validate code structure and CPU fallback

**For full GPU testing:**
- ⏳ Requires self-hosted runner with NVIDIA GPU
- ⏳ Follow setup steps in Option 2 above
- ⏳ Uncomment GPU tests in workflow file

The current setup allows development and validation to continue while GPU hardware is being set up.
