#!/usr/bin/env bash
set -euo pipefail

# Runs a command (or an interactive shell) after stripping the Snap-provided
# GTK/GIO environment variables that cause Qt to pick up the wrong glibc.
# Usage:
#   ./scripts/with-clean-env.sh <command> [args...]
#   ./scripts/with-clean-env.sh                  # spawns a sanitized shell
#   ./scripts/with-clean-env.sh gdb --args cpp-prototype/build-release/viewer/vhs-rf-viewer

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "${ROOT_DIR}/scripts/lib/snap_env.sh"

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    cat <<'EOF'
Usage: with-clean-env.sh [command [args...]]

Launches the requested command (or an interactive shell if no command is
provided) after removing Snap-provided GTK/GIO paths that otherwise force
Qt to load /snap/.../libpthread.so.0 and trigger GLIBC_PRIVATE failures.
EOF
    exit 0
fi

sanitize_snap_environment

if [[ $# -eq 0 ]]; then
    exec "${SHELL:-/bin/bash}"
else
    exec "$@"
fi
