#!/bin/sh
# Finder entrypoint: run the native client without an external interpreter.
set -eu
entry_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
workspace_root=$(CDPATH= cd -- "$entry_dir"/@WORKSPACE_ROOT@ && pwd)
run_dir="$workspace_root/runtime/client/runs/$(date +%Y%m%d-%H%M%S)-$$"
mkdir -p "$run_dir/cache"
exec >> "$run_dir/log.txt" 2>&1
printf '%s\n' '# Use the pinned backend defaults.' > "$run_dir/dxvk.conf"
export VK_DRIVER_FILES="$workspace_root/deps/moltenvk/local-icd.json"
export DYLD_LIBRARY_PATH="$workspace_root/deps/moltenvk/MoltenVK/MoltenVK/dynamic/dylib/macOS:$workspace_root/deps/vulkan-sdk/macOS/lib:$workspace_root/deps/sdl3/lib:$workspace_root/deps/dxvk/lib"
export DXVK_WSI_DRIVER=SDL3
export DXVK_STATE_CACHE_PATH="$run_dir/cache"
export DXVK_CONFIG_FILE="$run_dir/dxvk.conf"
unset DXVK_CONFIG
export FOURSTORY_SETTINGS_DIR="${FOURSTORY_SETTINGS_DIR:-$workspace_root/runtime/client/settings}"
export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0:abort_on_error=0}"
account_record="$workspace_root/runtime/accounts/service.json"
if [ -f "$account_record" ] && [ "${FOURSTORY_ACCOUNTS_URL+x}" != x ]; then
    account_port=$(/usr/bin/plutil -extract port raw -o - "$account_record")
    case "$account_port" in ''|*[!0-9]*) echo 'Invalid account service port'; exit 1;; esac
    [ "$account_port" -ge 1 ] && [ "$account_port" -le 65535 ] || { echo 'Invalid account service port'; exit 1; }
    export FOURSTORY_ACCOUNTS_URL="https://127.0.0.1:$account_port/v1/accounts"
    export FOURSTORY_ACCOUNTS_CA="$workspace_root/runtime/accounts/tls/ca.pem"
fi
if [ "$#" = 0 ]; then
    interface=$(/sbin/route -n get 1.1.1.1 2>/dev/null | /usr/bin/awk '/interface:/ {print $2; exit}')
    address=$(/usr/sbin/ipconfig getifaddr "$interface" 2>/dev/null || true)
    [ -n "$address" ] || address=$(/usr/sbin/ipconfig getifaddr en0 2>/dev/null || true)
    [ -n "$address" ] || address=$(/usr/sbin/ipconfig getifaddr en1 2>/dev/null || true)
    [ -n "$address" ] || { echo 'Cannot choose a server address. Use Play.command with --server SERVER_IP.'; exit 1; }
    set -- "$address" 4815 1
fi
exec "$entry_dir/4story-client" --data "$workspace_root/game" "$@"
