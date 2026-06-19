param(
    [switch]$Clean,
    [switch]$Flash,
    [string]$Port = "COM10",
    [string]$BuildRoot = "D:\OttoBuild\xz_com10_build"
)

$ErrorActionPreference = "Stop"

$SourceRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$BuildRoot = [System.IO.Path]::GetFullPath($BuildRoot)

if ($BuildRoot -eq $SourceRoot) {
    throw "BuildRoot must be a separate short path."
}

if (-not $BuildRoot.StartsWith("D:\OttoBuild\", [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to use unexpected BuildRoot: $BuildRoot"
}

New-Item -ItemType Directory -Force -Path $BuildRoot | Out-Null

$excludeDirs = @(".git", "build", ".tmp", ".tools", ".ccache", ".ccache-tmp", "releases")
$robocopyArgs = @($SourceRoot, $BuildRoot, "/E", "/XD") + $excludeDirs
& robocopy @robocopyArgs
if ($LASTEXITCODE -gt 7) {
    throw "robocopy failed with exit code $LASTEXITCODE"
}

if ($Clean -and (Test-Path -LiteralPath (Join-Path $BuildRoot "build"))) {
    $target = (Resolve-Path -LiteralPath (Join-Path $BuildRoot "build")).Path
    if (-not $target.StartsWith($BuildRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to delete outside BuildRoot: $target"
    }
    Remove-Item -LiteralPath $target -Recurse -Force
}

$env:IDF_PATH = "D:\Espressif\v5.5.2\esp-idf"
$env:IDF_TOOLS_PATH = "C:\Espressif\tools"
$env:IDF_PYTHON_ENV_PATH = "C:\Espressif\tools\python\v5.5.2\venv"
$env:IDF_CCACHE_ENABLE = "1"
$env:CCACHE_DIR = "D:\Espressif\ccache"
$env:TEMP = "D:\Espressif\tmp"
$env:TMP = "D:\Espressif\tmp"

$toolPath = @(
    "C:\Espressif\tools\ccache\4.11.2\ccache-4.11.2-windows-x86_64",
    "C:\Espressif\tools\cmake\3.30.2\bin",
    "C:\Espressif\tools\dfu-util\0.11\dfu-util-0.11-win64",
    "C:\Espressif\tools\esp-clang\esp-19.1.2_20250312\esp-clang\bin",
    "C:\Espressif\tools\esp-rom-elfs\20241011\",
    "C:\Espressif\tools\esp32ulp-elf\2.38_20240113\esp32ulp-elf\bin",
    "C:\Espressif\tools\esp32ulp-elf\2.38_20240113\esp32ulp-elf\esp32ulp-elf\bin",
    "C:\Espressif\tools\idf-exe\1.0.3\",
    "C:\Espressif\tools\ninja\1.12.1\",
    "C:\Espressif\tools\openocd-esp32\v0.12.0-esp32-20250707\openocd-esp32\bin",
    "C:\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20251107\riscv32-esp-elf\bin",
    "C:\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20251107\riscv32-esp-elf\riscv32-esp-elf\bin",
    "C:\Espressif\tools\xtensa-esp-elf-gdb\16.3_20250913\xtensa-esp-elf-gdb\bin",
    "C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20251107\xtensa-esp-elf\bin",
    "C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20251107\xtensa-esp-elf\xtensa-esp-elf\bin",
    "C:\Espressif\tools\python\v5.5.2\venv\Scripts"
) -join ";"
$env:PATH = $toolPath + ";" + $env:PATH

$python = "C:\Espressif\tools\python\v5.5.2\venv\Scripts\python.exe"
$idf = "D:\Espressif\v5.5.2\esp-idf\tools\idf.py"

Push-Location $BuildRoot
try {
    & $python $idf set-target esp32s3
    if ($LASTEXITCODE -ne 0) { throw "idf.py set-target failed: $LASTEXITCODE" }

    Add-Content -Path sdkconfig -Value "`n# Append by Codex otto build`nCONFIG_BOARD_TYPE_OTTO_ROBOT=y`nCONFIG_HTTPD_WS_SUPPORT=y`nCONFIG_CAMERA_OV2640=y`nCONFIG_CAMERA_OV3660=y"

    & $python $idf -DBOARD_NAME=otto-robot -DBOARD_TYPE=otto-robot build
    if ($LASTEXITCODE -ne 0) { throw "idf.py build failed: $LASTEXITCODE" }

    if ($Flash) {
        & $python $idf -p $Port flash
        if ($LASTEXITCODE -ne 0) { throw "idf.py flash failed: $LASTEXITCODE" }
    }
}
finally {
    Pop-Location
}
