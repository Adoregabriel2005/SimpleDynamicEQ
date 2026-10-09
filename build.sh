#!/usr/bin/env bash
# ==============================================================================
# SimpleDynamicEQ – macOS & Linux Build and Install Script
# Builds VST3, AU (AudioUnit for Logic Pro / GarageBand), and Standalone
# Compatible with Intel (x86_64) and Apple Silicon (arm64 / M1 / M2 / M3 / M4)
# ==============================================================================

set -e

CONFIG="${1:-Release}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build"

echo ""
echo "======================================================"
echo "  SimpleDynamicEQ – Cross-Platform Build Script"
echo "  Target OS: $(uname -s) ($(uname -m))"
echo "  Config:    ${CONFIG}"
echo "======================================================"
echo ""

# 1. Check tools
command -v cmake >/dev/null 2>&1 || {
    echo "ERROR: CMake is required but not installed."
    echo "Install via Homebrew: brew install cmake ninja"
    exit 1
}

# Choose generator
GENERATOR="Unix Makefiles"
if command -v ninja >/dev/null 2>&1; then
    GENERATOR="Ninja"
fi

echo "[1/4] Configuring CMake (${GENERATOR})..."
mkdir -p "${BUILD_DIR}"

if [[ "$(uname -s)" == "Darwin" ]]; then
    cmake -B "${BUILD_DIR}" \
        -G "${GENERATOR}" \
        -DCMAKE_BUILD_TYPE="${CONFIG}" \
        -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
        -DCMAKE_OSX_DEPLOYMENT_TARGET="10.15" \
        -S "${ROOT_DIR}"
else
    cmake -B "${BUILD_DIR}" \
        -G "${GENERATOR}" \
        -DCMAKE_BUILD_TYPE="${CONFIG}" \
        -S "${ROOT_DIR}"
fi

echo ""
echo "[2/4] Compiling plugin (${CONFIG})..."
cmake --build "${BUILD_DIR}" --config "${CONFIG}" --parallel

echo ""
echo "[3/4] Installing plugins..."

if [[ "$(uname -s)" == "Darwin" ]]; then
    # macOS plugin directories
    AU_DIR="${HOME}/Library/Audio/Plug-Ins/Components"
    VST3_DIR="${HOME}/Library/Audio/Plug-Ins/VST3"

    mkdir -p "${AU_DIR}"
    mkdir -p "${VST3_DIR}"

    # Find compiled AU (.component)
    AU_SRC=$(find "${BUILD_DIR}" -type d -name "SimpleDynamicEQ.component" | head -n 1)
    if [[ -n "${AU_SRC}" && -d "${AU_SRC}" ]]; then
        echo "   Copying AU -> ${AU_DIR}/SimpleDynamicEQ.component"
        rm -rf "${AU_DIR}/SimpleDynamicEQ.component"
        cp -R "${AU_SRC}" "${AU_DIR}/"
        echo "   [OK] AU (AudioUnit) installed successfully for Logic Pro / GarageBand!"
    fi

    # Find compiled VST3
    VST3_SRC=$(find "${BUILD_DIR}" -type d -name "SimpleDynamicEQ.vst3" | head -n 1)
    if [[ -n "${VST3_SRC}" && -d "${VST3_SRC}" ]]; then
        echo "   Copying VST3 -> ${VST3_DIR}/SimpleDynamicEQ.vst3"
        rm -rf "${VST3_DIR}/SimpleDynamicEQ.vst3"
        cp -R "${VST3_SRC}" "${VST3_DIR}/"
        echo "   [OK] VST3 installed successfully for Ableton, Reaper, FL Studio, Cubase, Studio One!"
    fi

    # Rescan AU cache for Logic Pro / macOS
    echo ""
    echo "[4/4] Refreshing macOS Audio Component cache..."
    killall -9 AudioComponentRegistrar 2>/dev/null || true

elif [[ "$(uname -s)" == "Linux" ]]; then
    # Linux plugin directory
    VST3_DIR="${HOME}/.vst3"
    mkdir -p "${VST3_DIR}"
    VST3_SRC=$(find "${BUILD_DIR}" -type d -name "SimpleDynamicEQ.vst3" | head -n 1)
    if [[ -n "${VST3_SRC}" && -d "${VST3_SRC}" ]]; then
        rm -rf "${VST3_DIR}/SimpleDynamicEQ.vst3"
        cp -R "${VST3_SRC}" "${VST3_DIR}/"
        echo "   [OK] VST3 installed to ${VST3_DIR}/SimpleDynamicEQ.vst3"
    fi
fi

echo ""
echo "======================================================"
echo "  BUILD COMPLETED SUCCESSFULLY!"
echo "  Universal binaries generated for all DAWs:"
echo "  - Logic Pro / GarageBand : AU (AudioUnit)"
echo "  - Ableton / FL Studio / Reaper / Cubase / Studio One : VST3"
echo "  - Standalone App in build directory"
echo "======================================================"
echo ""
