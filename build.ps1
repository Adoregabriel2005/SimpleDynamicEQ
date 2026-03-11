# ============================================================
#  ProEQ VST3 – Build Script
#  Compila o plugin e instala em C:\Program Files\Common Files\VST3\
#  Nao requer cmake nem git no PATH – usa as ferramentas do VS Build Tools
# ============================================================

param(
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"

# ---- Paths ------------------------------------------------
$Root     = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = "$Root\build"
$VS       = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
$CMake    = "$VS\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$Ninja    = "$VS\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
$VcVars   = "$VS\VC\Auxiliary\Build\vcvars64.bat"

Write-Host ""
Write-Host "=======================================" -ForegroundColor Cyan
Write-Host "  SimpleDynamicEQ VST3  Build Script" -ForegroundColor Cyan
Write-Host "  Config : $Config" -ForegroundColor Cyan
Write-Host "=======================================" -ForegroundColor Cyan
Write-Host ""

# ---- Verify tools -----------------------------------------
foreach ($tool in @($CMake, $Ninja, $VcVars)) {
    if (-not (Test-Path $tool)) {
        Write-Host "ERRO: Ferramenta nao encontrada: $tool" -ForegroundColor Red
        Write-Host "Instale o 'Desktop development with C++' no VS Build Tools." -ForegroundColor Yellow
        exit 1
    }
}
Write-Host "[OK] Ferramentas do VS Build Tools encontradas." -ForegroundColor Green

# ---- Download JUCE if not present -------------------------
$JuceCMake = "$Root\JUCE\CMakeLists.txt"
if (-not (Test-Path $JuceCMake)) {
    Write-Host ""
    Write-Host "[1/4] Baixando JUCE (GitHub ZIP)…" -ForegroundColor Yellow
    $ZipPath = "$env:TEMP\JUCE-master.zip"
    $ExtractTo = "$env:TEMP\JUCE-unzip"

    try {
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        Invoke-WebRequest -Uri "https://github.com/juce-framework/JUCE/archive/refs/heads/master.zip" `
                          -OutFile $ZipPath -UseBasicParsing
        Write-Host "   Download concluido. Extraindo…"
        if (Test-Path $ExtractTo) { Remove-Item $ExtractTo -Recurse -Force }
        Expand-Archive -Path $ZipPath -DestinationPath $ExtractTo -Force
        $JuceFolder = Get-ChildItem $ExtractTo -Directory | Select-Object -First 1
        Move-Item $JuceFolder.FullName "$Root\JUCE"
        Write-Host "[OK] JUCE extraido para $Root\JUCE" -ForegroundColor Green
    } catch {
        Write-Host "ERRO ao baixar JUCE: $_" -ForegroundColor Red
        Write-Host "Verifique sua conexao com a internet e tente novamente." -ForegroundColor Yellow
        exit 1
    }
} else {
    Write-Host "[OK] JUCE ja presente." -ForegroundColor Green
}

# ---- Remove JUCE's git-clone logic from CMakeLists --------
# (We already have JUCE; patch the CMakeLists so find_package(Git REQUIRED) is not triggered)
$CMLists = "$Root\CMakeLists.txt"
$CML = Get-Content $CMLists -Raw
if ($CML -match "find_package\(Git REQUIRED\)") {
    $CML = $CML -replace "(?s)if\(NOT EXISTS.*?endif\(\)", "# JUCE bundled locally"
    Set-Content $CMLists $CML -Encoding UTF8
    Write-Host "[OK] CMakeLists.txt: git-clone desativado (JUCE local)." -ForegroundColor Green
}

# ---- CMake configure (Ninja + MSVC via vcvars64) ----------
Write-Host ""
Write-Host "[2/4] Configurando CMake (Ninja + MSVC x64)..." -ForegroundColor Yellow

# Capture vcvars64 env vars into the current PowerShell session
$EnvDump = "$env:TEMP\vcvars_env.txt"
$BatInit = "$env:TEMP\proeq_init.bat"
"@echo off`r`ncall `"$VcVars`"`r`nset > `"$EnvDump`"" | Set-Content $BatInit -Encoding ASCII
cmd /c $BatInit | Out-Null
Get-Content $EnvDump | ForEach-Object {
    if ($_ -match "^([^=]+)=(.+)$") {
        [System.Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], "Process")
    }
}
Write-Host "   Ambiente MSVC x64 inicializado." -ForegroundColor DarkGray

# Run cmake configure directly (env is now set in this process)
& $CMake -B $BuildDir `
    -G Ninja `
    "-DCMAKE_BUILD_TYPE=$Config" `
    -DCMAKE_C_COMPILER=cl `
    -DCMAKE_CXX_COMPILER=cl `
    "-DCMAKE_MAKE_PROGRAM=$Ninja" `
    $Root

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "ERRO na configuracao CMake! Verifique os erros acima." -ForegroundColor Red
    exit 1
}
Write-Host "[OK] Configuracao concluida." -ForegroundColor Green

# ---- CMake build ------------------------------------------
Write-Host ""
Write-Host "[3/4] Compilando ($Config)..." -ForegroundColor Yellow

& $CMake --build $BuildDir --config $Config --parallel
if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "ERRO na compilacao! Verifique os erros acima." -ForegroundColor Red
    exit 1
}
Write-Host "[OK] Build concluido!" -ForegroundColor Green

# ---- Copy .vst3 to common VST3 folder ----------------------
Write-Host ""
Write-Host "[4/4] Instalando VST3…" -ForegroundColor Yellow

$Vst3Src = "$BuildDir\ProEQ_artefacts\$Config\VST3\SimpleDynamicEQ.vst3"
$Vst3Dst = "C:\Program Files\Common Files\VST3\SimpleDynamicEQ.vst3"

if (-not (Test-Path $Vst3Src)) {
    # JUCE sometimes nests differently
    $Vst3Src = (Get-ChildItem "$BuildDir" -Filter "SimpleDynamicEQ.vst3" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1 FullName).FullName
}

if ($Vst3Src -and (Test-Path $Vst3Src)) {
    if (Test-Path $Vst3Dst) { Remove-Item $Vst3Dst -Recurse -Force }
    Copy-Item $Vst3Src $Vst3Dst -Recurse -Force
    Write-Host "[OK] SimpleDynamicEQ.vst3 instalado em: $Vst3Dst" -ForegroundColor Green
} else {
    Write-Host "AVISO: SimpleDynamicEQ.vst3 nao encontrado para copiar." -ForegroundColor Yellow
    Write-Host "        Procure manualmente em: $BuildDir" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "=======================================" -ForegroundColor Cyan
Write-Host "  BUILD CONCLUIDO COM SUCESSO!" -ForegroundColor Green
Write-Host "  Abra o FL Studio, va em:" -ForegroundColor Cyan
Write-Host "  Options > Manage Plugins > Scan" -ForegroundColor Cyan
Write-Host "  e procure por 'SimpleDynamicEQ'" -ForegroundColor Cyan
Write-Host "=======================================" -ForegroundColor Cyan
Write-Host ""

