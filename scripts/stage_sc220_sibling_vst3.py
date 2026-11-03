#!/usr/bin/env python3
"""Stage pinned sibling VST3s; never scan repos or download mutable "latest".

No external Python deps. CI downloads exactly the reviewed GitHub Release asset
and verifies the full archive SHA-256 before extracting its one VST3 bundle.
Does not build, load or execute sibling code during archive extraction.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import stat
import sys
import tempfile
import urllib.request
import zipfile
from pathlib import Path, PurePosixPath

MANIFEST = Path(__file__).with_name("sibling-vst3-manifest.json")
MAX_ARCHIVE = 35 * 1024 * 1024
MAX_TOTAL_UNCOMPRESSED = 130 * 1024 * 1024
MAX_SINGLE_FILE = 65 * 1024 * 1024


def safe_parts(name: str) -> tuple[str, ...]:
    """Reject zip-slip, NTFS alternate streams, backslash tricks and links."""
    if "\\" in name or name.startswith("/") or "\x00" in name:
        raise ValueError("unsafe ZIP member path")
    parts = PurePosixPath(name).parts
    if not parts or any(p in ("", ".", "..") or ":" in p for p in parts):
        raise ValueError("unsafe ZIP member component")
    return parts


def release_url(item: dict) -> str:
    owner_repo = item["repo"]
    tag, asset = item["tag"], item["asset"]
    if (not owner_repo.startswith("masarray/") or owner_repo.count("/") != 1
            or not tag.startswith("v") or "/" in tag
            or not asset.endswith(".zip") or "/" in asset):
        raise ValueError("manifest is not a pinned sibling GitHub Release asset")
    return f"https://github.com/{owner_repo}/releases/download/{tag}/{asset}"


def check_manifest(document: dict) -> list[dict]:
    if document.get("schema") != 1:
        raise ValueError("unsupported manifest schema")
    items = document.get("plugins")
    if not isinstance(items, list) or [x.get("name") for x in items] != [
        "ArSonEQ", "ArSonComp", "ArSonVerb"
    ]:
        raise ValueError("expected exact independently versioned three-plugin catalogue")
    for item in items:
        release_url(item)
        sha = item["sha256"]
        if len(sha) != 64 or any(c not in "0123456789abcdef" for c in sha):
            raise ValueError("release ZIP SHA256 missing or invalid")
    return items


def download_verified(item: dict, destination: Path) -> None:
    url = release_url(item)
    request = urllib.request.Request(
        url,
        headers={"User-Agent": "SC220Live-P16-VST3-Provenance/1.0",
                 "Accept": "application/octet-stream"},
    )
    digest = hashlib.sha256()
    received = 0
    with urllib.request.urlopen(request, timeout=90) as source, destination.open("wb") as output:
        while chunk := source.read(1024 * 256):
            received += len(chunk)
            if received > MAX_ARCHIVE:
                raise ValueError(f"{item['name']}: archive exceeds security bound")
            digest.update(chunk)
            output.write(chunk)
    if digest.hexdigest() != item["sha256"]:
        raise ValueError(f"{item['name']}: SHA-256 mismatch, abort staging")
    print(f"VERIFY {item['name']}: {digest.hexdigest()} ({received} bytes)", flush=True)


def stage_from_verified_zip(item: dict, archive: Path, directory: Path) -> None:
    name = item["name"]
    root = name + ".vst3"
    target = directory / root
    if target.exists():
        raise ValueError(f"{name}: destination already exists; refuse unexpected overwrite")
    directory.mkdir(parents=True, exist_ok=True)
    total = 0
    module_bytes = 0
    has_license = False
    try:
        with zipfile.ZipFile(archive) as zf:
            if zf.testzip() is not None:
                raise ValueError(f"{name}: corrupted ZIP entry")
            for entry in zf.infolist():
                parts = safe_parts(entry.filename)
                mode = entry.external_attr >> 16
                if stat.S_IFMT(mode) == stat.S_IFLNK:
                    raise ValueError("symlinks are not accepted in VST3 release archives")
                total += entry.file_size
                if entry.file_size > MAX_SINGLE_FILE or total > MAX_TOTAL_UNCOMPRESSED:
                    raise ValueError(f"{name}: suspicious decompression size")
                if entry.is_dir():
                    continue
                if parts[-1] == "AGPL-3.0-LICENSE.txt":
                    (directory / (name + "-AGPL-3.0.txt")).write_bytes(zf.read(entry))
                    has_license = True
                try:
                    index = parts.index(root)
                except ValueError:
                    continue
                relative = parts[index + 1:]
                if not relative:
                    raise ValueError("VST3 must be a Windows directory bundle")
                output = target.joinpath(*relative)
                output.parent.mkdir(parents=True, exist_ok=True)
                with zf.open(entry, "r") as inp, output.open("wb") as dst:
                    shutil.copyfileobj(inp, dst)
                if output.name.lower().endswith(".vst3") and output.is_file():
                    module_bytes += output.stat().st_size
    except Exception:
        if target.exists():
            shutil.rmtree(target)
        raise
    if module_bytes < 100_000 or not has_license:
        shutil.rmtree(target, ignore_errors=True)
        raise ValueError(f"{name}: no plausible VST3 module or missing AGPL licence")
    print(f"STAGED {name}: verified x64 VST3 module bytes={module_bytes}", flush=True)


def self_test() -> None:
    items = check_manifest(json.loads(MANIFEST.read_text(encoding="utf-8")))
    assert safe_parts("ArSonEQ.vst3/Contents/x86_64-win/ArSonEQ.vst3")
    for bad in ("../escape.dll", "/etc/passwd", "C:/Windows/win.ini",
                "ArSonComp.vst3/../escape.dll", "a\\..\\b", ""):
        try:
            safe_parts(bad)
        except ValueError:
            pass
        else:
            raise AssertionError("unsafe ZIP path accepted: " + bad)
    assert [x["prepackaged_by_prior_step"] for x in items if x["name"] == "ArSonVerb"] == [True]
    print("PASS: pinned three-plugin manifest, zip traversal, bounded staging policies")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, help="SC220 Live portable Plugins directory")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return 0
    if args.out is None:
        parser.error("--out is required except for --self-test")
    items = check_manifest(json.loads(MANIFEST.read_text(encoding="utf-8")))
    args.out.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="sc220-pinned-vst3-") as tmp:
        for item in items:
            if item.get("prepackaged_by_prior_step", False):
                if not (args.out / (item["name"] + ".vst3")).exists():
                    raise ValueError(f"{item['name']} was not supplied by the pinned Verb build step")
                continue
            archive = Path(tmp) / item["asset"]
            download_verified(item, archive)
            stage_from_verified_zip(item, archive, args.out)
    (args.out / "SIBLING-PROVENANCE.json").write_text(
        json.dumps({"schema": 1, "plugins": items}, indent=2) + "\n", encoding="utf-8")
    print("PASS: exact sibling VST3 bundles staged beside the SC220 executable")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:
        print(f"FAIL P16 sibling VST3 integration: {exc}", file=sys.stderr)
        sys.exit(1)
