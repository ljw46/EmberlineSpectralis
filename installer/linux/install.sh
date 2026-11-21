#!/usr/bin/env bash
# Per-user install (no root needed). Run "./install.sh --uninstall" to remove it again.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
vst3="$HOME/.vst3"
bin="$HOME/.local/bin"
apps="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
icons="${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor/256x256/apps"

if [ "${1:-}" = "--uninstall" ]; then
  rm -rf "$vst3/Emberline Spectralis.vst3" "$bin/emberline-spectralis" \
         "$apps/emberline-spectralis.desktop" "$icons/emberline-spectralis.png"
  echo "Removed."; exit 0
fi

mkdir -p "$vst3" "$bin" "$apps" "$icons"
rm -rf "$vst3/Emberline Spectralis.vst3"
cp -R "$here/Emberline Spectralis.vst3" "$vst3/"
install -m755 "$here/emberline-spectralis" "$bin/emberline-spectralis"
install -m644 "$here/emberline-spectralis.desktop" "$apps/"
install -m644 "$here/phoenix.png" "$icons/emberline-spectralis.png"
command -v update-desktop-database >/dev/null && update-desktop-database "$apps" 2>/dev/null || true

echo "Installed."
echo "  Plugin:     $vst3/Emberline Spectralis.vst3  (rescan plugins in your DAW)"
echo "  Standalone: emberline-spectralis  (or find it in your applications menu)"
case ":$PATH:" in *":$bin:"*) ;; *) echo "  Note: $bin is not on your PATH; use the menu entry or add it." ;; esac
