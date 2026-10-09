# ============================================================
#  SimpleDynamicEQ – Windows Build Script
#  Compila o plugin VST3 e Standalone e instala em:
#  C:\Program Files\Common Files\VST3\
# ============================================================

param(
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"

# ---- Paths ------------------------------------------------
$Root     = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = "$Root\build"

Write-Host ""
Write-Host "=======================================" -ForegroundColor Cyan
Write-Host "  SimpleDynamicEQ Build Script (Windows)" -ForegroundColor Cyan
Write-Host "  Config : $Config" -ForegroundColor Cyan
Write-Host "=======================================" -ForegroundColor Cyan
Write-Host ""

# ---- Locate Visual Studio & CMake -------------------------
$VsInstallations = @(
    "C:\Program Files\Microsoft Visual Studio\2022\Community",
    "C:\Program Files\Microsoft Visual Studio\2022\Professional",
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise",
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools",
    "C:\Program Files\Microsoft Visual Studio\2019\Community",
    "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools"
)

$VcVars = $null
$VSPath = $null

foreach ($path in $VsInstallations) {
    $testBat = "$path\VC\Auxiliary\Build\vcvars64.bat"
    if (Test-Path $testBat) {
        $VcVars = $testBat
        $VSPath = $path
        break
    }
}

$CMake = "cmake"
$Ninja = "ninja"

if ($VSPath) {
    $vsCmake = "$VSPath\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    $vsNinja = "$VSPath\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
    if (Test-Path $vsCmake) { $CMake = $vsCmake }
    if (Test-Path $vsNinja) { $Ninja = $vsNinja }
}

# If cmake not found in VS, test in PATH
if (-not (Get-Command $CMake -ErrorAction SilentlyContinue) -and -not (Test-Path $CMake)) {
    Write-Host "ERRO: CMake nao encontrado." -ForegroundColor Red
    Write-Host "Instale o CMake ou o componente 'Desktop development with C++' do Visual Studio." -ForegroundColor Yellow
    exit 1
}

Write-Host "[OK] Ferramentas de compilacao localizadas." -ForegroundColor Green

# ---- Initialize MSVC environment if vcvars64 available ----
if ($VcVars -and (Test-Path $VcVars)) {
    Write-Host "   Carregando ambiente MSVC x64..." -ForegroundColor DarkGray
    $EnvDump = "$env:TEMP\vcvars_env.txt"
    $BatInit = "$env:TEMP\proeq_init.bat"
    "@echo off`r`ncall `"$VcVars`"`r`nset > `"$EnvDump`"" | Set-Content $BatInit -Encoding ASCII
    cmd /c $BatInit | Out-Null
    Get-Content $EnvDump | ForEach-Object {
        if ($_ -match "^([^=]+)=(.+)$") {
            [System.Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], "Process")
        }
    }
}

# ---- CMake configure --------------------------------------
Write-Host ""
Write-Host "[1/3] Configurando CMake..." -ForegroundColor Yellow

$GeneratorArgs = @()
if ((Get-Command "ninja" -ErrorAction SilentlyContinue) -or (Test-Path $Ninja)) {
    $GeneratorArgs = @("-G", "Ninja", "-DCMAKE_MAKE_PROGRAM=$Ninja", "-DCMAKE_C_COMPILER=cl", "-DCMAKE_CXX_COMPILER=cl")
}

& $CMake -B $BuildDir "-DCMAKE_BUILD_TYPE=$Config" @GeneratorArgs $Root

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "ERRO na configuracao CMake!" -ForegroundColor Red
    exit 1
}
Write-Host "[OK] Configuracao concluida." -ForegroundColor Green

# ---- CMake build ------------------------------------------
Write-Host ""
Write-Host "[2/3] Compilando ($Config)..." -ForegroundColor Yellow

& $CMake --build $BuildDir --config $Config --parallel
if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "ERRO na compilacao!" -ForegroundColor Red
    exit 1
}
Write-Host "[OK] Build concluido com sucesso!" -ForegroundColor Green

# ---- Copy .vst3 to common VST3 folder ----------------------
Write-Host ""
Write-Host "[3/3] Instalando VST3..." -ForegroundColor Yellow

$Vst3DstDir = "C:\Program Files\Common Files\VST3"
$Vst3Dst = "$Vst3DstDir\SimpleDynamicEQ.vst3"

if (-not (Test-Path $Vst3DstDir)) {
    New-Item -ItemType Directory -Path $Vst3DstDir -Force | Out-Null
}

$Vst3Src = (Get-ChildItem "$BuildDir" -Filter "SimpleDynamicEQ.vst3" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1 FullName).FullName

if ($Vst3Src -and (Test-Path $Vst3Src)) {
    if (Test-Path $Vst3Dst) { Remove-Item $Vst3Dst -Recurse -Force }
    Copy-Item $Vst3Src $Vst3Dst -Recurse -Force
    Write-Host "[OK] SimpleDynamicEQ.vst3 instalado em: $Vst3Dst" -ForegroundColor Green
} else {
    Write-Host "AVISO: Arquivo .vst3 compilado em $BuildDir" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "=======================================" -ForegroundColor Cyan
Write-Host "  BUILD CONCLUIDO COM SUCESSO!" -ForegroundColor Green
Write-Host "  Compativel com qualquer DAW no Windows:" -ForegroundColor Cyan
Write-Host "  - FL Studio, Ableton Live, Reaper, Cubase, Studio One, Bitwig, etc." -ForegroundColor Cyan
Write-Host "=======================================" -ForegroundColor Cyan
Write-Host ""
