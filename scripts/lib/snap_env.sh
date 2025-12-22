#!/usr/bin/env bash
# Shared helper that scrubs Snap-provided GTK/GIO paths so Qt binaries
# load the host system's glibc/libpthread instead of the Snap runtime.

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    echo "snap_env.sh is meant to be sourced, not executed." >&2
    exit 1
fi

if [[ -n "${VHS_DECODE_SNAP_ENV_HELPER:-}" ]]; then
    return
fi
readonly VHS_DECODE_SNAP_ENV_HELPER=1

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

    local var
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
