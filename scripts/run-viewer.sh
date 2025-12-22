#!/usr/bin/env bash
set -euo pipefail

# Launches the Qt RF viewer after configuring/building it in the requested configuration.
# Usage:
#   scripts/run-viewer.sh [--debug|--release] [--build-dir <path>] [--run-only] [--build-only] [-- <viewer args>]

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_TYPE="Debug"
BUILD_ONLY=0
RUN_ONLY=0
CUSTOM_BUILD_DIR=""
PASS_ARGS=()

print_help() {
    cat <<'EOF'
Usage: run-viewer.sh [options] [-- <viewer args>]

Options:
  --debug, -d        Build/run the viewer in Debug mode (default)
  --release, -r      Build/run the viewer in Release mode
  --build-dir PATH   Use a custom build directory (defaults to cpp-prototype/build-$CONFIG)
  --build-only       Configure & build but skip launching the viewer
  --run-only         Skip configuring/building and just launch the viewer (expects existing build)
  --help, -h         Show this help message
  --                 Pass the remaining arguments to vhs-rf-viewer
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --debug|-d)
            BUILD_TYPE="Debug"
            shift
            ;;
        --release|-r)
            BUILD_TYPE="Release"
            shift
            ;;
        --build-dir)
            [[ $# -gt 1 ]] || { echo "Error: --build-dir requires a path" >&2; exit 1; }
            CUSTOM_BUILD_DIR="$2"
            shift 2
            ;;
        --build-only)
            BUILD_ONLY=1
            shift
            ;;
        --run-only)
            RUN_ONLY=1
            shift
            ;;
        --help|-h)
            print_help
            exit 0
            ;;
        --)
            shift
            PASS_ARGS=("$@")
            break
            ;;
        *)
            PASS_ARGS+=("$1")
            shift
            ;;
    esac
done

if [[ $BUILD_ONLY -eq 1 && $RUN_ONLY -eq 1 ]]; then
    echo "Error: --build-only and --run-only are mutually exclusive" >&2
    exit 1
fi

if [[ -z "$CUSTOM_BUILD_DIR" ]]; then
    BUILD_DIR="${ROOT_DIR}/cpp-prototype/build-${BUILD_TYPE,,}"
else
    BUILD_DIR="$CUSTOM_BUILD_DIR"
fi

VIEWER_TARGET="vhs-rf-viewer"
BINARY_PATH="${BUILD_DIR}/viewer/${VIEWER_TARGET}"

configure_and_build() {
    cmake -S "${ROOT_DIR}/cpp-prototype" -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DBUILD_VIEWER=ON
    cmake --build "${BUILD_DIR}" --target "${VIEWER_TARGET}"
}

if [[ $RUN_ONLY -eq 0 ]]; then
    configure_and_build
fi

if [[ $BUILD_ONLY -eq 1 ]]; then
    exit 0
fi

if [[ ! -x "$BINARY_PATH" ]]; then
    echo "Error: ${BINARY_PATH} not found. Did the build succeed?" >&2
    exit 1
fi

"$BINARY_PATH" "${PASS_ARGS[@]}"
