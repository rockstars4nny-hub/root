$p = New-Object System.IO.Ports.SerialPort COM6,115200
$p.ReadTimeout = 10000
$p.Open()
Start-Sleep -Milliseconds 300
# Toggle reset via DTR if available
$p.DtrEnable = $false
Start-Sleep -Milliseconds 100
$p.DtrEnable = $true
Start-Sleep -Milliseconds 300
$buf = ""
for ($i = 0; $i -lt 80; $i++) {
  Start-Sleep -Milliseconds 150
  if ($p.BytesToRead -gt 0) { $buf += $p.ReadExisting() }
}
$p.Close()
Write-Output $buf
