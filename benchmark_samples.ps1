# VHS-Decode GPU Benchmark Script
# Compares CPU vs GPU performance on sample files

$samples = @(
    @{Name="sony_short_40M_8b"; File="sample_data\sony_short_40M_8b.u8"; Frames=50},
    @{Name="sony_nichols_new_cable_v01"; File="sample_data\sony_nichols_new_cable_v01.u8"; Frames=50}
)

$results = @()

Write-Host "`n=== VHS-Decode GPU Benchmark ===" -ForegroundColor Cyan
Write-Host "Testing CPU vs GPU performance on sample files`n" -ForegroundColor Gray

foreach ($sample in $samples) {
    Write-Host "Sample: $($sample.Name)" -ForegroundColor Yellow
    Write-Host "  Size: $([math]::Round((Get-Item $sample.File).Length/1GB, 2)) GB" -ForegroundColor Gray
    Write-Host "  Testing $($sample.Frames) frames...`n" -ForegroundColor Gray
    
    # CPU Benchmark
    Write-Host "  [CPU] Processing..." -ForegroundColor Green -NoNewline
    $cpuStart = Get-Date
    $cpuOutput = "benchmark_${sample.Name}_cpu"
    
    python decode.py vhs --system PAL -f 40 --overwrite --length $sample.Frames `
        $sample.File $cpuOutput 2>&1 | Out-Null
    
    $cpuEnd = Get-Date
    $cpuTime = ($cpuEnd - $cpuStart).TotalSeconds
    Write-Host " Done! $([math]::Round($cpuTime, 2))s" -ForegroundColor Green
    
    # GPU Benchmark
    Write-Host "  [GPU] Processing..." -ForegroundColor Blue -NoNewline
    $gpuStart = Get-Date
    $gpuOutput = "benchmark_${sample.Name}_gpu"
    
    python decode.py vhs --system PAL -f 40 --gpu --overwrite --length $sample.Frames `
        $sample.File $gpuOutput 2>&1 | Out-Null
    
    $gpuEnd = Get-Date
    $gpuTime = ($gpuEnd - $gpuStart).TotalSeconds
    Write-Host " Done! $([math]::Round($gpuTime, 2))s" -ForegroundColor Blue
    
    # Calculate speedup
    $speedup = $cpuTime / $gpuTime
    
    $results += [PSCustomObject]@{
        Sample = $sample.Name
        Size_GB = [math]::Round((Get-Item $sample.File).Length/1GB, 2)
        Frames = $sample.Frames
        CPU_Time_s = [math]::Round($cpuTime, 2)
        GPU_Time_s = [math]::Round($gpuTime, 2)
        Speedup = [math]::Round($speedup, 2)
        CPU_FPS = [math]::Round($sample.Frames / $cpuTime, 2)
        GPU_FPS = [math]::Round($sample.Frames / $gpuTime, 2)
    }
    
    Write-Host "  Speedup: $([math]::Round($speedup, 2))x" -ForegroundColor Magenta
    Write-Host ""
    
    # Cleanup output files
    Remove-Item "benchmark_${sample.Name}_cpu*" -ErrorAction SilentlyContinue
    Remove-Item "benchmark_${sample.Name}_gpu*" -ErrorAction SilentlyContinue
}

# Display results table
Write-Host "`n=== Benchmark Results ===" -ForegroundColor Cyan
$results | Format-Table -AutoSize

# Calculate average speedup
$avgSpeedup = ($results | Measure-Object -Property Speedup -Average).Average
Write-Host "Average Speedup: $([math]::Round($avgSpeedup, 2))x" -ForegroundColor Magenta

# Save results to JSON
$results | ConvertTo-Json | Out-File "benchmark_results.json"
Write-Host "`nResults saved to benchmark_results.json" -ForegroundColor Gray
