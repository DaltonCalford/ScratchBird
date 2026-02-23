#!/usr/bin/env python3
import hashlib
import json
import pathlib
import subprocess
import sys
from typing import Dict, Any, List


def sha256_file(path: pathlib.Path) -> str:
    if not path.exists():
        return "missing"
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def git_head(repo_root: pathlib.Path) -> str:
    try:
        out = subprocess.check_output(
            ["git", "-C", str(repo_root), "rev-parse", "HEAD"],
            stderr=subprocess.DEVNULL,
            text=True,
        ).strip()
        return out
    except Exception:
        return "unknown"


def main() -> int:
    repo_root = pathlib.Path(__file__).resolve().parents[2]
    output = (
        pathlib.Path(sys.argv[1])
        if len(sys.argv) > 1
        else repo_root / "artifacts/cross_os/p6s2w1/xos-037-build-manifest.json"
    )
    output.parent.mkdir(parents=True, exist_ok=True)

    presets_path = repo_root / "CMakePresets.json"
    vcpkg_manifest = repo_root / "vcpkg.json"
    vcpkg_config = repo_root / "vcpkg-configuration.json"
    mingw_toolchain = repo_root / "cmake/toolchains/mingw-w64-x86_64.cmake"

    with presets_path.open("r", encoding="utf-8") as f:
        presets = json.load(f)

    configure_presets: List[Dict[str, Any]] = presets.get("configurePresets", [])
    build_presets: List[Dict[str, Any]] = presets.get("buildPresets", [])
    test_presets: List[Dict[str, Any]] = presets.get("testPresets", [])

    manifest: Dict[str, Any] = {
        "schema": "scratchbird.cross_os.build_manifest.v1",
        "project_version": "0.1.0",
        "git_head": git_head(repo_root),
        "inputs": {
            "cmake_presets": {"path": "CMakePresets.json", "sha256": sha256_file(presets_path)},
            "vcpkg_manifest": {"path": "vcpkg.json", "sha256": sha256_file(vcpkg_manifest)},
            "vcpkg_configuration": {
                "path": "vcpkg-configuration.json",
                "sha256": sha256_file(vcpkg_config),
            },
            "mingw_toolchain": {
                "path": "cmake/toolchains/mingw-w64-x86_64.cmake",
                "sha256": sha256_file(mingw_toolchain),
            },
        },
        "configure_presets": configure_presets,
        "build_presets": build_presets,
        "test_presets": test_presets,
    }

    # Stable sorting and deterministic formatting.
    with output.open("w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2, sort_keys=True)
        f.write("\n")

    print(f"Wrote build manifest: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
