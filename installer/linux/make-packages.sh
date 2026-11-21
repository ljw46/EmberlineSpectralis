#!/usr/bin/env bash
# Builds the Linux packages from a finished build:
#   dist/emberline-spectralis_<version>_amd64.deb      system-wide (needs sudo to install)
#   dist/EmberlineSpectralis-linux.tar.gz              per-user install via install.sh, no root
# Usage: installer/linux/make-packages.sh <version> [artefact dir]
set -euo pipefail
ver="${1:?usage: make-packages.sh <version> [artefact dir]}"
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"
art="${2:-$root/build/EmberlineSpectralis_artefacts/Release}"
vst="$art/VST3/Emberline Spectralis.vst3"
exe="$art/Standalone/Emberline Spectralis"
[ -d "$vst" ] && [ -x "$exe" ] || { echo "build artefacts not found in $art" >&2; exit 1; }
out="$root/dist"; rm -rf "$out"; mkdir -p "$out"
work="$(mktemp -d)"; trap 'rm -rf "$work"' EXIT

# ---- .deb
d="$work/deb"
mkdir -p "$d/DEBIAN" "$d/usr/lib/vst3" "$d/usr/bin" "$d/usr/share/applications" \
         "$d/usr/share/icons/hicolor/256x256/apps" "$d/usr/share/doc/emberline-spectralis"
cp -R "$vst" "$d/usr/lib/vst3/"
install -m755 "$exe" "$d/usr/bin/emberline-spectralis"
install -m644 "$here/emberline-spectralis.desktop" "$d/usr/share/applications/"
install -m644 "$root/Assets/phoenix.png" "$d/usr/share/icons/hicolor/256x256/apps/emberline-spectralis.png"
install -m644 "$root/LICENSE" "$root/LICENSE-CONTENT.md" "$d/usr/share/doc/emberline-spectralis/"
find "$d" -type d -exec chmod 755 {} +
size=$(du -sk "$d" | cut -f1)
cat > "$d/DEBIAN/control" <<CTL
Package: emberline-spectralis
Version: $ver
Section: sound
Priority: optional
Architecture: amd64
Installed-Size: $size
Depends: libasound2 | libasound2t64, libfreetype6, libfontconfig1, libx11-6, libxext6, libxrandr2, libxinerama1, libxcursor1, libgl1, xdg-utils
Maintainer: Emberline <noreply@users.noreply.github.com>
Homepage: https://github.com/ljw46/EmberlineSpectralis
Description: Emberline Spectralis wavetable synthesizer (VST3 + Standalone)
 A wavetable synthesizer for melodic, deep and organic house, with 160 factory
 presets. Installs the VST3 plugin to /usr/lib/vst3 and the Standalone app.
CTL
dpkg-deb --root-owner-group --build "$d" "$out/emberline-spectralis_${ver}_amd64.deb" >/dev/null

# ---- per-user tarball
t="$work/EmberlineSpectralis-linux"
mkdir -p "$t"
cp -R "$vst" "$t/"
install -m755 "$exe" "$t/emberline-spectralis"
cp "$here/emberline-spectralis.desktop" "$root/Assets/phoenix.png" "$root/LICENSE" "$root/LICENSE-CONTENT.md" "$t/"
install -m755 "$here/install.sh" "$t/install.sh"
cp "$root/docs/install-linux.txt" "$t/INSTALL.txt"
tar -C "$work" -czf "$out/EmberlineSpectralis-linux.tar.gz" EmberlineSpectralis-linux
ls -l "$out"
