# ============================================================
#  SimpleDynamicEQ - Dev Script
#
#  USO:
#    .\dev.ps1           -> compila Debug e abre o Standalone
#    .\dev.ps1 -fl       -> compila Debug, instala VST3 e abre o FL Studio
#    .\dev.ps1 -release  -> compila Release e instala o VST3
#
# ============================================================
param(
    [switch]$fl,
    [switch]$release
)

$ErrorActionPreference = "Stop"

# --- Configuracoes ---
$Root      = $PSScriptRoot
$VS        = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
$CMake     = "$VS\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$VcVars    = "$VS\VC\Auxiliary\Build\vcvars64.bat"
$FLExe     = "C:\Program Files\Image-Line\FL Studio 2025\FL64.exe"
$Vst3Dst   = "C:\Program Files\Common Files\VST3\SimpleDynamicEQ.vst3"

if ($release) {
    $Config   = "Release"
    $BuildDir = "$Root\build"
} else {
    $Config   = "Debug"
    $BuildDir = "$Root\build_debug"
}

$StandaloneExe = "$BuildDir\ProEQ_artefacts\$Config\Standalone\SimpleDynamicEQ.exe"
$Vst3Src       = "$BuildDir\ProEQ_artefacts\$Config\VST3\SimpleDynamicEQ.vst3"

# --- Cabecalho ---
if ($release)      { $mode = "RELEASE -> instala VST3" }
elseif ($fl)       { $mode = "DEBUG -> instala VST3 + abre FL" }
else               { $mode = "DEBUG -> abre Standalone" }

Write-Host ""
Write-Host "=====================================" -ForegroundColor Cyan
Write-Host "  SimpleDynamicEQ  -  $mode" -ForegroundColor Cyan
Write-Host "=====================================" -ForegroundColor Cyan
Write-Host ""

# --- Fechar o FL antes de compilar (evita arquivo bloqueado) ---
if ($fl -or $release) {
    $flProcs = Get-Process -Name "FL64" -ErrorAction SilentlyContinue
    if ($flProcs) {
        Write-Host "[->] Fechando FL Studio para liberar o VST3..." -ForegroundColor Yellow
        $flProcs | Stop-Process -Force
        Start-Sleep -Milliseconds 800
    }
}

# --- Inicializar ambiente MSVC x64 ---
Write-Host "[1/3] Inicializando MSVC x64..." -ForegroundColor Yellow
$EnvBat = "$env:TEMP\proeq_vcvars.bat"
$EnvTxt = "$env:TEMP\proeq_vcvars.txt"
Set-Content $EnvBat "@echo off`r`ncall `"$VcVars`" >nul 2>&1`r`nset > `"$EnvTxt`"" -Encoding ASCII
cmd.exe /c "`"$EnvBat`"" 2>$null | Out-Null
if (Test-Path $EnvTxt) {
    Get-Content $EnvTxt | ForEach-Object {
        if ($_ -match "^([^=]+)=(.*)$") {
            [System.Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], "Process")
        }
    }
} else {
    Write-Host "ERRO: Falha ao inicializar MSVC." -ForegroundColor Red
    exit 1
}

# --- CMake configure (so se necessario) ---
if (-not (Test-Path "$BuildDir\build.ninja")) {
    Write-Host "   Configurando CMake pela primeira vez..." -ForegroundColor DarkGray
    & $CMake -B $BuildDir -G Ninja "-DCMAKE_BUILD_TYPE=$Config" `
             -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl $Root
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERRO na configuracao CMake." -ForegroundColor Red
        exit 1
    }
}

# --- Build incremental (so recompila o que mudou) ---
Write-Host "[2/3] Compilando ($Config)..." -ForegroundColor Yellow
$buildOut = & $CMake --build $BuildDir --config $Config 2>&1
$errors   = $buildOut | Where-Object { $_ -match ": error " }

if ($errors) {
    Write-Host ""
    Write-Host "====================================" -ForegroundColor Red
    Write-Host "  ERRO DE COMPILACAO" -ForegroundColor Red
    Write-Host "====================================" -ForegroundColor Red
    $errors | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    Write-Host ""
    exit 1
}

# Warnings relevantes (filtra os conhecidos de juce::Font)
$warnings = $buildOut | Where-Object {
    $_ -match ": warning C" -and $_ -notmatch "Font::Font|getStringWidth"
}
if ($warnings) {
    Write-Host "   Warnings:" -ForegroundColor DarkYellow
    $warnings | Select-Object -Unique | ForEach-Object {
        Write-Host "   $_" -ForegroundColor DarkYellow
    }
}
Write-Host "[OK] Build concluido!" -ForegroundColor Green

# ============================================================
#  MODO STANDALONE
# ============================================================
if (-not $fl -and -not $release) {
    Write-Host ""
    Write-Host "[3/3] Abrindo Standalone..." -ForegroundColor Yellow

    if (-not (Test-Path $StandaloneExe)) {
        Write-Host "ERRO: Standalone nao encontrado:" -ForegroundColor Red
        Write-Host "  $StandaloneExe" -ForegroundColor Red
        exit 1
    }

    # Matar instancia anterior do standalone se estiver aberta
    Get-Process -Name "SimpleDynamicEQ" -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Milliseconds 300

    Start-Process $StandaloneExe
    Write-Host "[OK] SimpleDynamicEQ Standalone aberto!" -ForegroundColor Green
    Write-Host ""
    Write-Host "  Para testar no FL Studio:  .\dev.ps1 -fl" -ForegroundColor DarkGray
    Write-Host ""
    exit 0
}

# ============================================================
#  INSTALAR VST3
# ============================================================
Write-Host "[3/3] Instalando VST3..." -ForegroundColor Yellow

try {
    if (Test-Path $Vst3Dst) { Remove-Item $Vst3Dst -Recurse -Force -ErrorAction Stop }
    Copy-Item $Vst3Src $Vst3Dst -Recurse -Force -ErrorAction Stop
    Write-Host "[OK] VST3 instalado!" -ForegroundColor Green
}
catch {
    Write-Host "   Permissao negada. Pedindo admin..." -ForegroundColor Yellow
    $cmd = "if (Test-Path '$Vst3Dst') { Remove-Item '$Vst3Dst' -Recurse -Force }; Copy-Item '$Vst3Src' '$Vst3Dst' -Recurse -Force"
    Start-Process powershell -Verb RunAs -ArgumentList "-NoProfile -Command `"$cmd`"" -Wait
    Write-Host "[OK] VST3 instalado via admin!" -ForegroundColor Green
}

# ============================================================
#  ABRIR FL STUDIO
# ============================================================
if ($fl) {
    if (Test-Path $FLExe) {
        Write-Host ""
        Write-Host "[->] Abrindo FL Studio..." -ForegroundColor Yellow
        Start-Process $FLExe
        Write-Host ""
        Write-Host "==========================================" -ForegroundColor Green
        Write-Host "  PRONTO!" -ForegroundColor Green
        Write-Host "  No FL Studio:" -ForegroundColor Green
        Write-Host "    Mixer > slot vazio > selecione SimpleDynamicEQ" -ForegroundColor Green
        Write-Host "    (sem scan necessario - VST3 ja instalado)" -ForegroundColor Green
        Write-Host "==========================================" -ForegroundColor Green
    } else {
        Write-Host "AVISO: FL Studio nao encontrado em: $FLExe" -ForegroundColor Yellow
        Write-Host "       VST3 instalado, abra o FL manualmente." -ForegroundColor Yellow
    }
}

Write-Host ""