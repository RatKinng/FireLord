param(
    [switch]$SkipCompose
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
Set-Location $scriptDir

if (-not (Test-Path .env)) {
    Write-Error ".env not found. Run setup.bat or setup.sh first."
}

if (-not (Test-Path .venv)) {
    Write-Error ".venv not found. Run setup.bat or setup.sh first."
}

# Load .env into process env vars
Get-Content .env | Where-Object { $_ -and ($_ -notmatch '^#') } | ForEach-Object {
    $parts = $_ -split '=',2
    if ($parts.Length -eq 2) {
        [System.Environment]::SetEnvironmentVariable($parts[0], $parts[1], 'Process')
    }
}

if (-not $env:DB_HOST) { $env:DB_HOST = 'localhost' }

$localDb = $false
if ($env:DB_HOST -eq 'localhost' -or $env:DB_HOST -eq '127.0.0.1') {
    $localDb = $true
}

if ($localDb -and -not $SkipCompose) {
    Write-Host "Starting Postgres via docker compose..."
    docker compose up -d
} else {
    Write-Host "DB_HOST is $($env:DB_HOST); skipping local docker compose."
}

Write-Host "Launching logger..."
. .\.venv\Scripts\Activate.ps1
python -m firelord_logger
