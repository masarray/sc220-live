#!/usr/bin/env bash
# Package only a successfully compiled SC220Live JUCE GUI target.
# The release assets contain no source, private submodules or debug symbols.
set -euo pipefail

if (( $# != 3 )); then
  echo "Usage: $0 <linux|macos> <version> <cmake-build-directory>" >&2
  exit 2
fi

platform="$1"
version="$2"
build_dir="$3"
[[ "$version" =~ ^v[0-9]+\.[0-9]+\.[0-9]+(-[a-zA-Z0-9][a-zA-Z0-9.-]*)?$ ]] || {
  echo "Invalid version: $version" >&2
  exit 2
}

build_dir="$(cd "$build_dir" && pwd)"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

dist="$repo_root/dist"
mkdir -p "$dist"
stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT
common="SC220-Live-${version}"
notes="$repo_root/docs/PORTABLE_RELEASE.md"
[[ -f "$notes" && -f "$repo_root/EULA.txt" ]] || exit 3

case "$platform" in
  linux)
    executable="$(find "$build_dir/SC220Live_artefacts" -type f -name 'SC220 Live' -perm /111 -print -quit)"
    [[ -n "$executable" ]] || { echo "Linux executable not found" >&2; exit 3; }
    folder="$stage/$common-linux-x64"
    mkdir -p "$folder"
    cp "$executable" "$folder/SC220 Live"
    cp "$notes" "$folder/README-Linux-macOS.md"
    cp "$repo_root/EULA.txt" "$folder/EULA.txt"
    chmod 755 "$folder/SC220 Live"
    tar -C "$stage" -czf "$dist/$common-linux-x64.tar.gz" "$(basename "$folder")"
    ;;
  macos)
    bundle="$(find "$build_dir/SC220Live_artefacts" -type d -name 'SC220 Live.app' -print -quit)"
    [[ -n "$bundle" ]] || { echo "macOS .app bundle not found" >&2; exit 3; }
    folder="$stage/SC220 Live"
    mkdir -p "$folder"
    ditto "$bundle" "$folder/SC220 Live.app"
    cp "$notes" "$folder/README-Linux-macOS.md"
    cp "$repo_root/EULA.txt" "$folder/EULA.txt"
    # Ad-hoc signing is for local testing only. It is NOT Developer ID notarization.
    codesign --force --deep --sign - "$folder/SC220 Live.app"
    codesign --verify --deep "$folder/SC220 Live.app"
    hdiutil create -volname 'SC220 Live (Preview)' -srcfolder "$folder" -format UDZO -ov "$dist/$common-macos-universal.dmg"
    ;;
  *)
    echo "Unknown platform $platform" >&2
    exit 2
    ;;
esac

cd "$dist"
for file in "$common"-*; do
  case "$file" in *.dmg|*.tar.gz) shasum -a 256 "$file" > "$file.sha256" ;; esac
done
ls -lh .
