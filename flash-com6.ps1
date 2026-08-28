$ErrorActionPreference = "Stop"
$dst = "$env:USERPROFILE\root-flash"
New-Item -ItemType Directory -Force -Path $dst | Out-Null
$src = "\\wsl.localhost\kali-linux\home\Hatari\root\.pio\build\esp32-s3-n16r8"
Copy-Item "$src\bootloader.bin" $dst -Force
Copy-Item "$src\partitions.bin" $dst -Force
Copy-Item "$src\firmware.bin" $dst -Force
Set-Location $dst
Write-Host "Flashing root to COM6 (flash-mode dio for N16R8 board)..."
python -m esptool --chip esp32s3 --port COM6 --baud 460800 --before default-reset --after hard-reset write-flash -z --flash-mode dio --flash-freq 80m --flash-size 16MB 0x0 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin
exit $LASTEXITCODE
