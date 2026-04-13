param(
    [switch]$Clean
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# La je force un build plugin strict en VC++ 2010 x64 depuis les artefacts
# extraits localement, pour ne plus dependre du fallback moderne.
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$outputDir = Join-Path $PSScriptRoot "x64\Release_v100"
$depsRoot = Join-Path $repoRoot "KenshiLib_Examples_deps"
$kenshiLibRoot = Join-Path $depsRoot "KenshiLib"
$boostRoot = Join-Path $depsRoot "boost_1_60_0"
$extractRoot = Join-Path $repoRoot "backups\vc100_extract"

$compilerPath = Join-Path $extractRoot "vc_stdx86\Program Files\Microsoft Visual Studio 10.0\VC\bin\x86_amd64\cl.exe"
$linkerPath = Join-Path $extractRoot "vc_stdx86\Program Files\Microsoft Visual Studio 10.0\VC\bin\x86_amd64\link.exe"
$vcIncludeDir = Join-Path $extractRoot "vc_stdx86\Program Files\Microsoft Visual Studio 10.0\VC\include"
$vcLibraryDir = Join-Path $extractRoot "vc_stdamd64\Program Files(64)\Microsoft Visual Studio 10.0\VC\lib\amd64"
$mspdbDir = Join-Path $extractRoot "vc_stdx86\Program Files\Microsoft Visual Studio 10.0\Common7\IDE"
$vcRuntimeDir = Join-Path $extractRoot "vc_stdamd64\Win\System64"
$sdkRoot = Join-Path $extractRoot "winsdkbuild_amd64\Program Files\Microsoft SDKs\Windows\v7.1"
$sdkIncludeDir = Join-Path $sdkRoot "Include"
$sdkLibraryDir = Join-Path $sdkRoot "Lib\x64"
$kenshiLibraryDir = Join-Path $kenshiLibRoot "Libraries"
$vsDumpbinPath = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\dumpbin.exe"

foreach ($requiredPath in @(
    $compilerPath,
    $linkerPath,
    $vcIncludeDir,
    $vcLibraryDir,
    $mspdbDir,
    $vcRuntimeDir,
    $sdkIncludeDir,
    $sdkLibraryDir,
    $kenshiLibraryDir
)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Element VC100/KenshiLib introuvable : $requiredPath"
    }
}

if (-not (Test-Path -LiteralPath $vsDumpbinPath)) {
    throw "Dumpbin introuvable pour verifier l'export startPlugin : $vsDumpbinPath"
}

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

if ($Clean) {
    Get-ChildItem -Path $outputDir -Filter "DonJ_Kenshi_Hack*" -ErrorAction SilentlyContinue | Remove-Item -Force
    Get-ChildItem -Path $outputDir -Filter "*.obj" -ErrorAction SilentlyContinue | Remove-Item -Force
}

$sourceFiles = @(
    (Join-Path $PSScriptRoot "CommandRegistry.cpp"),
    (Join-Path $PSScriptRoot "SpawnManager.cpp"),
    (Join-Path $PSScriptRoot "ArmyRuntimeManager.cpp"),
    (Join-Path $PSScriptRoot "TerminalBackend.cpp"),
    (Join-Path $PSScriptRoot "DonJ_Kenshi_Hack.cpp")
)

$originalPath = $env:PATH
$env:PATH = ($mspdbDir + ";" + (Split-Path -Parent $compilerPath) + ";" + $vcRuntimeDir + ";" + $env:PATH)

try {
    $compileArgs = @(
        "/c",
        "/EHsc",
        "/GR",
        "/Zi",
        "/MD",
        "/DNDEBUG",
        "/DUNICODE",
        "/D_UNICODE",
        "/DWIN32",
        "/D_WINDOWS",
        "/D_CONSOLE",
        "/I", $depsRoot,
        "/I", (Join-Path $kenshiLibRoot "Include"),
        "/I", $boostRoot,
        "/I", $vcIncludeDir,
        "/I", $sdkIncludeDir,
        ("/Fo" + (Join-Path $outputDir "")),
        $sourceFiles
    )

    & $compilerPath @compileArgs
    if ($LASTEXITCODE -ne 0) {
        throw "La compilation VC100 a echoue avec le code $LASTEXITCODE."
    }

    $objectFiles = @(
        (Join-Path $outputDir "CommandRegistry.obj"),
        (Join-Path $outputDir "SpawnManager.obj"),
        (Join-Path $outputDir "ArmyRuntimeManager.obj"),
        (Join-Path $outputDir "TerminalBackend.obj"),
        (Join-Path $outputDir "DonJ_Kenshi_Hack.obj")
    )

    $linkArgs = @(
        "/DLL",
        "/NOLOGO",
        "/DEBUG",
        "/MACHINE:X64",
        "/SUBSYSTEM:CONSOLE",
        ("/OUT:" + (Join-Path $outputDir "DonJ_Kenshi_Hack.dll")),
        ("/PDB:" + (Join-Path $outputDir "DonJ_Kenshi_Hack.pdb")),
        ("/LIBPATH:" + $kenshiLibraryDir),
        ("/LIBPATH:" + $vcLibraryDir),
        ("/LIBPATH:" + $sdkLibraryDir),
        $objectFiles,
        "KenshiLib.lib",
        "MyGUIEngine_x64.lib",
        "user32.lib"
    )

    & $linkerPath @linkArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Le link VC100 a echoue avec le code $LASTEXITCODE."
    }

    $builtDllPath = Join-Path $outputDir "DonJ_Kenshi_Hack.dll"
    $dumpbinOutput = & $vsDumpbinPath /exports $builtDllPath
    if ($LASTEXITCODE -ne 0) {
        throw "La verification des exports a echoue avec dumpbin."
    }

    if (-not ($dumpbinOutput | Select-String -Pattern '\?startPlugin@@YAXXZ' -Quiet)) {
        throw "La DLL buildée n'exporte pas startPlugin avec la convention KenshiLib attendue."
    }
}
finally {
    $env:PATH = $originalPath
}

Write-Host "Build VC100 OK : $(Join-Path $outputDir 'DonJ_Kenshi_Hack.dll')"
