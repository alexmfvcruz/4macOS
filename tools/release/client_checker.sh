#!/bin/bash
# 4Story (Beta) client checker. Lives in
# 4Story (Beta).app/Contents/Resources/tools/Client Checker.command
# Checks that everything the game needs is present. Changes nothing.

here=$(cd "$(dirname "$0")" && pwd)
resources=$(dirname "$here")
contents=$(dirname "$resources")
app=$(dirname "$contents")
settings="$HOME/Library/Application Support/4Story/Settings.plist"
logs="$HOME/Library/Logs/4Story"

bold=$'\e[1m'; dim=$'\e[2m'; green=$'\e[32m'; red=$'\e[31m'; blue=$'\e[34m'; reset=$'\e[0m'
problems=0

ok()   { printf "  ${green}✔${reset} %-14s %s\n" "$1" "$2"; }
bad()  { printf "  ${red}✘${reset} %-14s %s\n" "$1" "$2"; problems=$((problems+1)); }

bar() { # bar DONE TOTAL
    local width=30 filled=$(( $1 * 30 / ($2 > 0 ? $2 : 1) ))
    printf "\r  ${blue}%s%s${reset} %3d%%  %s/%s files" \
        "$(printf '█%.0s' $(seq 1 $filled) 2>/dev/null)" \
        "$(printf '░%.0s' $(seq 1 $((width - filled))) 2>/dev/null)" \
        $(( $1 * 100 / ($2 > 0 ? $2 : 1) )) "$1" "$2"
}

clear
printf "\n${bold}4Story (Beta) · client check${reset}\n${dim}%s${reset}\n\n" "$app"

# Game files, from the list made when the app was built.
list="$here/game-files.txt"
if [ -f "$list" ]; then
    total=$(wc -l < "$list" | tr -d ' ')
    done=0 missing=0
    while IFS= read -r file; do
        [ -e "$resources/Game/$file" ] || missing=$((missing+1))
        done=$((done+1))
        if [ $((done % 200)) -eq 0 ] || [ "$done" -eq "$total" ]; then bar "$done" "$total"; fi
    done < "$list"
    printf "\n\n"
    if [ "$missing" -eq 0 ]; then ok "Game data" "$total / $total files"
    else bad "Game data" "$missing of $total files are missing. Download the game again."; fi
else
    bad "Game data" "file list not found. Download the game again."
fi

count=0
for lib in libSDL3.0.dylib libdxvk_d3d9.0.dylib libMoltenVK.dylib libvulkan.1.dylib; do
    [ -f "$contents/Frameworks/$lib" ] && count=$((count+1))
done
if [ "$count" -eq 4 ]; then ok "Libraries" "4 / 4"; else bad "Libraries" "$count / 4. Download the game again."; fi

if codesign --verify --deep --strict "$app" 2>/dev/null; then ok "App signature" "valid"
else bad "App signature" "changed or damaged. Download the game again."; fi

version=$(sw_vers -productVersion)
if [ "${version%%.*}" -ge 14 ] && [ "$(uname -m)" = "arm64" ]; then ok "This Mac" "macOS $version, Apple Silicon"
else bad "This Mac" "macOS $version on $(uname -m). 4Story needs macOS 14 or newer on Apple Silicon."; fi

if [ -f "$settings" ]; then
    address=$(plutil -extract ServerAddress raw -o - "$settings" 2>/dev/null)
    case "$address" in
        "") bad "Settings" "ServerAddress is empty. View README." ;;
        127.*|localhost|::1) bad "Settings" "ServerAddress is $address. Use the server Mac's IP address. View README." ;;
        *) ok "Settings" "server $address" ;;
    esac
else
    bad "Settings" "not created yet. Open 4Story once."
fi

if mkdir -p "$logs" 2>/dev/null && [ -w "$logs" ]; then ok "Logs folder" "~/Library/Logs/4Story"
else bad "Logs folder" "cannot write to ~/Library/Logs/4Story"; fi

echo
if [ "$problems" -eq 0 ]; then printf "  ${green}${bold}Everything looks good.${reset}\n\n"
else printf "  ${red}${bold}%d problem(s) found.${reset} See the README.\n\n" "$problems"; fi
