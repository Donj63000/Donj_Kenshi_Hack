param(
    [string]$KenshiDir = "C:\Program Files (x86)\Steam\steamapps\common\Kenshi",
    [switch]$ResetLogs
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# La je remonte proprement a la racine du depot depuis plugin\DonJ_Kenshi_Hack.
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$packageDir = Join-Path $repoRoot "package\DonJ_Kenshi_Hack"
$buildDir = Join-Path $PSScriptRoot "x64\Release_v100"
$modDir = Join-Path $KenshiDir "mods\DonJ_Kenshi_Hack"

$sourceDll = Join-Path $buildDir "DonJ_Kenshi_Hack.dll"
$sourcePdb = Join-Path $buildDir "DonJ_Kenshi_Hack.pdb"
$sourceMod = Join-Path $packageDir "DonJ_Kenshi_Hack.mod"
$sourceJson = Join-Path $packageDir "RE_Kenshi.json"

$packageDll = Join-Path $packageDir "DonJ_Kenshi_Hack.dll"
$packagePdb = Join-Path $packageDir "DonJ_Kenshi_Hack.pdb"

$targetDll = Join-Path $modDir "DonJ_Kenshi_Hack.dll"
$targetPdb = Join-Path $modDir "DonJ_Kenshi_Hack.pdb"
$targetMod = Join-Path $modDir "DonJ_Kenshi_Hack.mod"
$targetJson = Join-Path $modDir "RE_Kenshi.json"

foreach ($requiredPath in @($sourceDll, $sourcePdb, $sourceMod, $sourceJson)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Fichier source introuvable : $requiredPath"
    }
}

New-Item -ItemType Directory -Force -Path $packageDir | Out-Null
New-Item -ItemType Directory -Force -Path $modDir | Out-Null

# La je garde le package local aligne sur le dernier build avant de pousser dans Kenshi.
Copy-Item -LiteralPath $sourceDll -Destination $packageDll -Force
Copy-Item -LiteralPath $sourcePdb -Destination $packagePdb -Force

Copy-Item -LiteralPath $sourceDll -Destination $targetDll -Force
Copy-Item -LiteralPath $sourcePdb -Destination $targetPdb -Force
Copy-Item -LiteralPath $sourceMod -Destination $targetMod -Force
Copy-Item -LiteralPath $sourceJson -Destination $targetJson -Force

if ($ResetLogs) {
    foreach ($logPath in @(
        (Join-Path $KenshiDir "RE_Kenshi_log.txt"),
        (Join-Path $KenshiDir "DonJ_Kenshi_Hack_trace.log"),
        (Join-Path $KenshiDir "RE_Kenshi\DonJ_Kenshi_Hack_trace.log")
    )) {
        if (Test-Path -LiteralPath $logPath) {
            Remove-Item -LiteralPath $logPath -Force
        }
    }
}

$buildHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $sourceDll).Hash
$gameHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $targetDll).Hash

Write-Host "Build DLL : $sourceDll"
Write-Host "Mod DLL   : $targetDll"
Write-Host "SHA256 build : $buildHash"
Write-Host "SHA256 mod   : $gameHash"

if ($buildHash -ne $gameHash) {
    throw "Le hash de la DLL deployee ne correspond pas au build courant."
}

Write-Host "Deploiement Kenshi OK."
