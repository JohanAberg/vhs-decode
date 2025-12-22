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

sanitize_snap_environment() {
    # VS Code Snap exports GTK_PATH (and friends) that point inside /snap/*, which drags in
    # ancient glibc builds via module RPATHs. Swap those values back to system paths (or unset)
    # so the viewer links against the host toolchain libraries instead of the snap runtime.
    local system_gtk_path="/usr/lib/x86_64-linux-gnu/gtk-3.0"

    if [[ "${GTK_PATH:-}" == /snap/* ]]; then
        if [[ -d "$system_gtk_path" ]]; then
            export GTK_PATH="$system_gtk_path"
        elif [[ -n "${GTK_PATH_VSCODE_SNAP_ORIG:-}" ]]; then
            export GTK_PATH="${GTK_PATH_VSCODE_SNAP_ORIG}"
        else
            unset GTK_PATH
        fi
    fi

    for var in GTK_EXE_PREFIX GTK_DATA_PREFIX GDK_BACKEND GIO_MODULE_DIR GSETTINGS_SCHEMA_DIR GTK_IM_MODULE_FILE LOCPATH; do
        local value="${!var-}"
        if [[ -n "$value" && "$value" == /snap/* ]]; then
            local backup_var="${var}_VSCODE_SNAP_ORIG"
            local backup_value="${!backup_var-}"
            if [[ -n "$backup_value" ]]; then
                export "$var"="$backup_value"
            else
                unset "$var"
            fi
        fi
    done

    if [[ -n "${XDG_DATA_DIRS:-}" && "$XDG_DATA_DIRS" == *"/snap/"* && -n "${XDG_DATA_DIRS_VSCODE_SNAP_ORIG:-}" ]]; then
        export XDG_DATA_DIRS="${XDG_DATA_DIRS_VSCODE_SNAP_ORIG}"
    fi

    if [[ -n "${XDG_CONFIG_DIRS:-}" && "$XDG_CONFIG_DIRS" == *"/snap/"* && -n "${XDG_CONFIG_DIRS_VSCODE_SNAP_ORIG:-}" ]]; then
        export XDG_CONFIG_DIRS="${XDG_CONFIG_DIRS_VSCODE_SNAP_ORIG}"
    fi
}

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

sanitize_snap_environment

"$BINARY_PATH" "${PASS_ARGS[@]}"
