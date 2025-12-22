<#
.SYNOPSIS
Generates a multi-size .ico for the viewer from the PNG, using ImageMagick if available, otherwise Python Pillow.

.PARAMETER SrcPng
Source PNG path. Defaults to repo viewer icon PNG.

.PARAMETER OutIco
Output ICO path. Defaults to matching .ico next to PNG.

.EXAMPLE
pwsh -File scripts/generate_viewer_icon.ps1

.EXAMPLE
pwsh -File scripts/generate_viewer_icon.ps1 -SrcPng assets/images/viewer/signal_color_v01.png -OutIco assets/images/viewer/signal_color_v01.ico
#>

[CmdletBinding()]
param(
    [string]$SrcPng = "$(Join-Path $PSScriptRoot '..' 'assets' 'images' 'viewer' 'signal_color_v01.png')",
    [string]$OutIco = "$(Join-Path $PSScriptRoot '..' 'assets' 'images' 'viewer' 'signal_color_v01.ico')"
)

function Test-Command {
    param([string]$Name)
    try { $null = Get-Command $Name -ErrorAction Stop; return $true } catch { return $false }
}

Write-Host "Source PNG: $SrcPng" -ForegroundColor Cyan
Write-Host "Output ICO: $OutIco" -ForegroundColor Cyan

if (!(Test-Path $SrcPng)) {
    Write-Error "PNG not found: $SrcPng"
    exit 1
}

New-Item -ItemType Directory -Force -Path (Split-Path $OutIco) | Out-Null

# Try ImageMagick first
if (Test-Command 'magick') {
    Write-Host "Using ImageMagick (magick) to create .ico..." -ForegroundColor Green
    $sizes = '256,128,64,48,32,16'
    $args = @('convert', $SrcPng, '-resize', '256x256', '-define', "icon:auto-resize=$sizes", $OutIco)
    $proc = Start-Process -FilePath 'magick' -ArgumentList $args -PassThru -Wait
    if ($proc.ExitCode -eq 0 -and (Test-Path $OutIco)) {
        Write-Host "Created icon via ImageMagick: $OutIco" -ForegroundColor Green
        exit 0
    } else {
        Write-Warning "ImageMagick failed (exit $($proc.ExitCode)). Falling back to Python Pillow."
    }
}

# Python Pillow fallback
if (Test-Command 'python') {
    Write-Host "Attempting Python Pillow fallback..." -ForegroundColor Yellow
    $py = @"
import sys
from pathlib import Path
try:
    from PIL import Image
except Exception as e:
    print(f"Pillow not available: {e}. Install with 'pip install pillow'.", file=sys.stderr)
    sys.exit(2)

src = Path(sys.argv[1])
out = Path(sys.argv[2])
img = Image.open(src).convert('RGBA')
sizes = [(256,256),(128,128),(64,64),(48,48),(32,32),(16,16)]
img.save(out, format='ICO', sizes=sizes)
print(f"Created icon via Pillow: {out}")
"@
    $tmpPath = [System.IO.Path]::GetTempFileName()
    Set-Content -LiteralPath $tmpPath -Value $py -Encoding UTF8
    $proc = Start-Process -FilePath 'python' -ArgumentList @($tmpPath, $SrcPng, $OutIco) -PassThru -Wait
    Remove-Item -LiteralPath $tmpPath -Force
    if ($proc.ExitCode -eq 0 -and (Test-Path $OutIco)) {
        Write-Host "Created icon via Python Pillow: $OutIco" -ForegroundColor Green
        exit 0
    } elseif ($proc.ExitCode -eq 2) {
        Write-Warning "Python found, but Pillow is missing. Run 'pip install pillow' and re-run."
    } else {
        Write-Warning "Python conversion failed (exit $($proc.ExitCode))."
    }
} else {
    Write-Warning "Python not found in PATH."
}

Write-Error "Failed to create .ico. Install ImageMagick or Python Pillow and retry."
exit 1