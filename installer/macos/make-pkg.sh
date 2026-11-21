#!/usr/bin/env bash
# Builds dist/EmberlineSpectralis-<version>-macos.pkg from signed bundles. Run on macOS.
# Usage: installer/macos/make-pkg.sh <version> <artefact dir> [signing identity for the installer]
set -euo pipefail
ver="${1:?usage: make-pkg.sh <version> <artefact dir> [installer identity]}"
art="${2:?artefact dir}"
ident="${3:-}"
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"
work="$(mktemp -d)"; trap 'rm -rf "$work"' EXIT
mkdir -p "$work/vst3/Library/Audio/Plug-Ins/VST3" "$work/app/Applications" "$work/res" "$root/dist"
ditto "$art/VST3/Emberline Spectralis.vst3" "$work/vst3/Library/Audio/Plug-Ins/VST3/Emberline Spectralis.vst3"
ditto "$art/Standalone/Emberline Spectralis.app" "$work/app/Applications/Emberline Spectralis.app"
cp "$root/LICENSE" "$work/res/LICENSE"
pkgbuild --root "$work/vst3" --identifier com.emberline.spectralis.vst3 --version "$ver" --install-location / "$work/vst3.pkg"
pkgbuild --root "$work/app"  --identifier com.emberline.spectralis.app  --version "$ver" --install-location / "$work/app.pkg"
sed "s/@VERSION@/$ver/g" "$here/distribution.xml" > "$work/distribution.xml"
# macOS ships bash 3.2, where an empty array is "unbound" under set -u, so branch instead of using an array
out="$root/dist/EmberlineSpectralis-$ver-macos.pkg"
if [ -n "$ident" ]; then
  productbuild --distribution "$work/distribution.xml" --resources "$work/res" --package-path "$work" --sign "$ident" "$out"
else
  productbuild --distribution "$work/distribution.xml" --resources "$work/res" --package-path "$work" "$out"
fi
