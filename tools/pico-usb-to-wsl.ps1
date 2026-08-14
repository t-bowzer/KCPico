#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Forward the Raspberry Pi Pico's USB device from Windows into WSL2 (usbipd-win).

.DESCRIPTION
    Detects the Pico (BOOTSEL RP2 mass-storage or running-firmware USB-CDC serial),
    binds it with usbipd, and attaches it to WSL so it appears as /dev/ttyACM0 inside WSL.
    Can also detach or just list USB devices.

.PARAMETER BusId
    Use a specific usbipd BUSID instead of auto-detecting the Pico.

.PARAMETER Detach
    Detach the device from WSL back to Windows instead of attaching it.

.PARAMETER List
    Print `usbipd list` and exit.

.EXAMPLE
    .\pico-usb-to-wsl.ps1
    Auto-detect the Pico and attach it to WSL.

.EXAMPLE
    .\pico-usb-to-wsl.ps1 -Detach
    Detach the Pico from WSL.

.EXAMPLE
    .\pico-usb-to-wsl.ps1 -BusId 1-3
    Attach a specific BUSID to WSL.
#>

param(
    [string]$BusId,
    [switch]$Detach,
    [switch]$List
)

$ErrorActionPreference = 'Stop'

function Get-UsbipdDevices {
    $out = & usbipd list 2>$null
    if ($LASTEXITCODE -ne 0 -or -not $out) {
        throw "Failed to run 'usbipd list'. Is usbipd-win installed? (https://github.com/dorssel/usbipd-win)"
    }

    # usbipd output format: "BUSID  VID:PID  DEVICE  STATE"
    # Parse into objects.
    $devices = @()
    foreach ($line in $out) {
        if ($line -match '^\s*(?<busid>\d+-\d+)\s+(?<vidpid>[0-9a-fA-F]{4}:[0-9a-fA-F]{4})\s+(?<desc>.+?)\s+(?<state>Not shared|Shared|Attached|Not attached|Detached)\s*$') {
            $devices += [pscustomobject]@{
                BusId   = $matches['busid']
                VidPid  = $matches['vidpid']
                Desc    = $matches['desc'].Trim()
                State   = $matches['state']
            }
        }
    }
    return $devices
}

function Find-PicoBusId {
    param([object[]]$Devices)

    $keywords = @('Raspberry Pi', 'RP2', 'RP1', 'Pico', 'USB Serial', 'CDC', 'TinyUSB')
    $matches = @($Devices | Where-Object {
        $d = $_.Desc
        foreach ($k in $keywords) { if ($d -like "*$k*") { return $true } }
        return $false
    })

    if ($matches.Count -eq 0) {
        Write-Host "No Pico-like device auto-detected. Current USB devices:" -ForegroundColor Yellow
        $Devices | Format-Table -AutoSize | Out-String | Write-Host
        $input = Read-Host "Enter the BUSID of the Pico (e.g. 1-3)"
        if ([string]::IsNullOrWhiteSpace($input)) {
            throw "No BUSID provided."
        }
        return $input.Trim()
    }

    if ($matches.Count -eq 1) {
        return $matches[0].BusId
    }

    Write-Host "Multiple Pico-like devices found:" -ForegroundColor Yellow
    for ($i = 0; $i -lt $matches.Count; $i++) {
        Write-Host ("  [{0}] {1}  {2}  ({3})" -f $i, $matches[$i].BusId, $matches[$i].Desc, $matches[$i].State)
    }
    $choice = Read-Host "Select a device number"
    $idx = [int]$choice
    if ($idx -lt 0 -or $idx -ge $matches.Count) {
        throw "Invalid selection."
    }
    return $matches[$idx].BusId
}

# ---- Main ----

if (-not (Get-Command usbipd -ErrorAction SilentlyContinue)) {
    throw "usbipd not found on PATH. Install usbipd-win: https://github.com/dorssel/usbipd-win"
}

if ($List) {
    & usbipd list
    exit 0
}

$devices = Get-UsbipdDevices

if (-not $BusId) {
    $BusId = Find-PicoBusId -Devices $devices
}

$target = $devices | Where-Object { $_.BusId -eq $BusId } | Select-Object -First 1

if ($Detach) {
    Write-Host "Detaching BUSID $BusId from WSL..." -ForegroundColor Cyan
    & usbipd detach --busid $BusId
    Write-Host "Done. Device is now back on Windows." -ForegroundColor Green
    exit 0
}

if ($target -and $target.State -eq 'Attached') {
    Write-Host "BUSID $BusId is already attached to WSL." -ForegroundColor Green
    Write-Host "In WSL run:  ls /dev/ttyACM*" -ForegroundColor Yellow
    exit 0
}

Write-Host "Binding BUSID $BusId..." -ForegroundColor Cyan
& usbipd bind --busid $BusId

Write-Host "Attaching BUSID $BusId to WSL..." -ForegroundColor Cyan
& usbipd attach --wsl --busid $BusId

Write-Host ""
Write-Host "Success. In WSL, the Pico should now appear as /dev/ttyACM0." -ForegroundColor Green
Write-Host "Next steps in WSL:" -ForegroundColor Yellow
Write-Host "  ls /dev/ttyACM*"
Write-Host "  pio device monitor -b 115200"
Write-Host ""
Write-Host "To release back to Windows later:" -ForegroundColor Yellow
Write-Host "  .\pico-usb-to-wsl.ps1 -Detach -BusId $BusId"
