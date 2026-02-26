#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
#  SpaceEcho – build script
#  Usage:  ./build.sh [Debug|Release]
# ─────────────────────────────────────────────────────────────────────────────
set -e

CONFIG=${1:-Release}
BUILD_DIR="build_${CONFIG}"

echo ""
echo "  ██████  ██████   █████   ██████ ███████     ███████  ██████ ██   ██  ██████"
echo "  ██      ██   ██ ██   ██ ██      ██          ██      ██      ██   ██ ██    ██"
echo "  ███████ ██████  ███████ ██      █████       █████   ██      ███████ ██    ██"
echo "       ██ ██      ██   ██ ██      ██          ██      ██      ██   ██ ██    ██"
echo "  ██████  ██      ██   ██  ██████ ███████     ███████  ██████ ██   ██  ██████"
echo ""
echo "  Building: ${CONFIG}"
echo ""

# ── Dependencies check ────────────────────────────────────────────────────────
if ! command -v cmake &>/dev/null; then
    echo "ERROR: cmake not found. Install via: brew install cmake"
    exit 1
fi

# ── Configure ─────────────────────────────────────────────────────────────────
cmake -B "${BUILD_DIR}" \
      -DCMAKE_BUILD_TYPE="${CONFIG}" \
      -G "Unix Makefiles"

# ── Build ─────────────────────────────────────────────────────────────────────
cmake --build "${BUILD_DIR}" --config "${CONFIG}" --parallel "$(sysctl -n hw.logicalcpu)"

echo ""
echo "✓ Build complete!"
echo ""
echo "  VST3: ${BUILD_DIR}/SpaceEcho_artefacts/${CONFIG}/VST3/Space Echo.vst3"
echo "  AU  : ${BUILD_DIR}/SpaceEcho_artefacts/${CONFIG}/AU/Space Echo.component"
echo ""
echo "  To install VST3: cp -r '${BUILD_DIR}/SpaceEcho_artefacts/${CONFIG}/VST3/Space Echo.vst3' ~/Library/Audio/Plug-Ins/VST3/"
echo "  To install AU  : cp -r '${BUILD_DIR}/SpaceEcho_artefacts/${CONFIG}/AU/Space Echo.component' ~/Library/Audio/Plug-Ins/Components/"
echo ""
