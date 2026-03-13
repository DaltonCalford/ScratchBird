#!/usr/bin/env python3
"""Generate deterministic compliance and legal-notice bundle artifacts."""

from __future__ import annotations

import argparse
import csv
import json
import os
import pathlib
import shutil
import sys
from typing import Any, Dict, List, Optional


SKIP_DIR_NAMES = {
    ".git",
    ".build",
    ".dart_tool",
    ".gradle",
    "__pycache__",
    "_build",
    "_deps",
    "artifacts",
    "bin",
    "deps",
    "dist",
    "node_modules",
    "obj",
    "results",
    "runtime",
    "target",
    "vendor",
}


def stable_json(data: Any) -> str:
    return json.dumps(data, indent=2, sort_keys=True) + "\n"


def sha256_file(path: pathlib.Path) -> str:
    import hashlib

    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def repo_slug(name: str) -> str:
    return "".join(ch.lower() if ch.isalnum() else "-" for ch in name).strip("-")


def should_skip_dir(name: str) -> bool:
    return name in SKIP_DIR_NAMES or name.startswith("build")


def is_legal_filename(file_name: str) -> bool:
    upper = file_name.upper()
    for stem in ("LICENSE", "NOTICE", "COPYING"):
        if upper == stem:
            return True
        if upper.startswith(stem + ".") or upper.startswith(stem + "-") or upper.startswith(stem + "_"):
            return True
    return False


def load_json(path: pathlib.Path) -> Dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def default_repo_roots(script_path: pathlib.Path) -> Dict[str, pathlib.Path]:
    scratchbird_root = script_path.resolve().parents[2]
    roots = {"ScratchBird": scratchbird_root}
    sibling = scratchbird_root.parent / "ScratchBird-driver"
    if sibling.exists():
        roots["ScratchBird-driver"] = sibling
    return roots


def scan_legal_files(repo_name: str, repo_root: pathlib.Path) -> List[Dict[str, str]]:
    discovered: List[Dict[str, str]] = []
    for current_root, dir_names, file_names in os.walk(repo_root):
        current = pathlib.Path(current_root)
        dir_names[:] = [name for name in dir_names if not should_skip_dir(name)]
        for file_name in sorted(file_names):
            if not is_legal_filename(file_name):
                continue
            file_path = current / file_name
            rel_path = file_path.relative_to(repo_root).as_posix()
            discovered.append(
                {
                    "repo_name": repo_name,
                    "repo_slug": repo_slug(repo_name),
                    "source_path": rel_path,
                    "sha256": sha256_file(file_path),
                    "scope": "repo-root" if len(pathlib.PurePosixPath(rel_path).parts) == 1 else "bundled-subtree",
                }
            )
    return discovered


def load_inventories(bundle_manifest_path: pathlib.Path) -> List[Dict[str, Any]]:
    bundle_dir = bundle_manifest_path.parent
    bundle_manifest = load_json(bundle_manifest_path)
    inventories: List[Dict[str, Any]] = []
    for entry in bundle_manifest.get("bundle_outputs", []):
        if entry["path"].endswith("-dependency-inventory.json"):
            inventories.append(load_json(bundle_dir / entry["path"]))
    if not inventories:
        raise ValueError("bundle manifest does not reference any dependency inventories")
    return inventories


def direct_legal_files_for_dir(legal_files: List[Dict[str, str]], dir_rel: str) -> List[Dict[str, str]]:
    result = []
    for item in legal_files:
        source = pathlib.PurePosixPath(item["source_path"])
        parent = source.parent.as_posix()
        parent = "" if parent == "." else parent
        if parent == dir_rel:
            result.append(item)
    return sorted(result, key=lambda item: item["source_path"])


def root_legal_files(legal_files: List[Dict[str, str]]) -> List[Dict[str, str]]:
    return [item for item in legal_files if item["scope"] == "repo-root"]


def component_notice_rows(inventories: List[Dict[str, Any]], legal_files_by_repo: Dict[str, List[Dict[str, str]]]) -> List[Dict[str, str]]:
    rows: List[Dict[str, str]] = []
    for inventory in inventories:
        repo_name = inventory["repo_id"]
        repo_legal_files = legal_files_by_repo.get(repo_name, [])
        root_files = root_legal_files(repo_legal_files)
        for component in inventory.get("components", []):
            manifest_dir = pathlib.PurePosixPath(component["manifest_path"]).parent
            manifest_dir_str = "" if manifest_dir.as_posix() == "." else manifest_dir.as_posix()
            local_files = direct_legal_files_for_dir(repo_legal_files, manifest_dir_str)
            if local_files:
                coverage_mode = "component-local"
                selected = local_files
            else:
                coverage_mode = "repo-root-fallback"
                selected = root_files
            rows.append(
                {
                    "repo_id": repo_name,
                    "manifest_path": component["manifest_path"],
                    "component_name": component["component_name"],
                    "manifest_type": component["manifest_type"],
                    "dependency_count": str(len(component.get("dependencies", []))),
                    "coverage_mode": coverage_mode,
                    "legal_files": ";".join(item["source_path"] for item in selected),
                }
            )
    return sorted(rows, key=lambda item: (item["repo_id"], item["manifest_path"], item["component_name"]))


def copy_legal_files(
    repo_roots: Dict[str, pathlib.Path],
    legal_files_by_repo: Dict[str, List[Dict[str, str]]],
    out_dir: pathlib.Path,
) -> List[Dict[str, str]]:
    copied: List[Dict[str, str]] = []
    for repo_name, files in legal_files_by_repo.items():
        repo_root = repo_roots[repo_name]
        slug = repo_slug(repo_name)
        for item in files:
            source_path = repo_root / item["source_path"]
            dest_path = out_dir / "licenses" / slug / item["source_path"]
            dest_path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source_path, dest_path)
            copied.append(
                {
                    **item,
                    "bundle_path": dest_path.relative_to(out_dir).as_posix(),
                }
            )
    return copied


def copy_support_artifact(source_path: pathlib.Path, out_dir: pathlib.Path, category: str) -> Dict[str, str]:
    destination = out_dir / "release_artifacts" / category / source_path.name
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source_path, destination)
    return {
        "category": category,
        "source_path": str(source_path),
        "bundle_path": destination.relative_to(out_dir).as_posix(),
        "sha256": sha256_file(destination),
    }


def write_notice_matrix(rows: List[Dict[str, str]], path: pathlib.Path) -> None:
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=[
                "repo_id",
                "manifest_path",
                "component_name",
                "manifest_type",
                "dependency_count",
                "coverage_mode",
                "legal_files",
            ],
        )
        writer.writeheader()
        writer.writerows(rows)


def generate_legal_notice_bundle_md(
    rows: List[Dict[str, str]],
    copied_legal_files: List[Dict[str, str]],
    copied_artifacts: List[Dict[str, str]],
) -> str:
    bundled_count = sum(1 for item in copied_legal_files if item["scope"] == "bundled-subtree")
    return "\n".join(
        [
            "# Legal Notice Bundle",
            "",
            f"- packaged legal files: `{len(copied_legal_files)}`",
            f"- bundled subtree legal files: `{bundled_count}`",
            f"- component coverage rows: `{len(rows)}`",
            "",
            "## Included Legal Files",
            *[f"- `{item['bundle_path']}` ({item['scope']})" for item in copied_legal_files],
            "",
            "## Included Release Artifacts",
            *[f"- `{item['bundle_path']}` ({item['category']})" for item in copied_artifacts],
            "",
            "## Coverage Rule",
            "- every SBOM component must resolve to component-local legal files or repo-root fallback coverage",
            "- bundled subtree legal files are packaged explicitly in the bundle",
            "- release-integrity artifacts from the SBOM, signing, and CVE triage lanes are attached alongside the notice pack",
            "",
        ]
    ) + "\n"


def generate_bundle(
    out_dir: pathlib.Path,
    bundle_manifest: pathlib.Path,
    signing_manifest: pathlib.Path,
    triage_report: pathlib.Path,
    repo_roots: Dict[str, pathlib.Path],
) -> pathlib.Path:
    inventories = load_inventories(bundle_manifest)
    legal_files_by_repo = {repo: scan_legal_files(repo, root) for repo, root in repo_roots.items()}
    notice_rows = component_notice_rows(inventories, legal_files_by_repo)

    out_dir.mkdir(parents=True, exist_ok=True)
    copied_legal_files = copy_legal_files(repo_roots, legal_files_by_repo, out_dir)
    copied_artifacts = [
        copy_support_artifact(bundle_manifest, out_dir, "sbom"),
        copy_support_artifact(signing_manifest, out_dir, "signing"),
        copy_support_artifact(triage_report, out_dir, "cve-triage"),
    ]

    notice_matrix_path = out_dir / "dependency-notice-matrix.csv"
    write_notice_matrix(notice_rows, notice_matrix_path)

    legal_notice_path = out_dir / "legal-notice-bundle.md"
    legal_notice_path.write_text(
        generate_legal_notice_bundle_md(notice_rows, copied_legal_files, copied_artifacts),
        encoding="utf-8",
    )

    manifest = {
        "schema": "scratchbird.release.compliance_bundle.v1",
        "bundle_manifest": {
            "source_path": str(bundle_manifest),
            "sha256": sha256_file(bundle_manifest),
        },
        "signing_manifest": {
            "source_path": str(signing_manifest),
            "sha256": sha256_file(signing_manifest),
        },
        "triage_report": {
            "source_path": str(triage_report),
            "sha256": sha256_file(triage_report),
        },
        "legal_files": copied_legal_files,
        "release_artifacts": copied_artifacts,
        "component_coverage_count": len(notice_rows),
    }
    manifest_path = out_dir / "compliance-bundle-manifest.json"
    manifest_path.write_text(stable_json(manifest), encoding="utf-8")
    return manifest_path


def validate_bundle(bundle_dir: pathlib.Path) -> int:
    try:
        manifest_path = bundle_dir / "compliance-bundle-manifest.json"
        manifest = load_json(manifest_path)
        for item in manifest.get("legal_files", []):
            file_path = bundle_dir / item["bundle_path"]
            if not file_path.exists():
                raise ValueError(f"missing legal file: {file_path}")
            if sha256_file(file_path) != item["sha256"]:
                raise ValueError(f"sha256 mismatch: {file_path}")
        for item in manifest.get("release_artifacts", []):
            file_path = bundle_dir / item["bundle_path"]
            if not file_path.exists():
                raise ValueError(f"missing release artifact: {file_path}")
            if sha256_file(file_path) != item["sha256"]:
                raise ValueError(f"sha256 mismatch: {file_path}")
        with (bundle_dir / "dependency-notice-matrix.csv").open("r", encoding="utf-8", newline="") as handle:
            rows = list(csv.DictReader(handle))
        if not rows:
            raise ValueError("dependency notice matrix is empty")
        uncovered = [row for row in rows if not row["legal_files"]]
        if uncovered:
            raise ValueError("component coverage rows missing legal files")
        if not (bundle_dir / "legal-notice-bundle.md").exists():
            raise ValueError("missing legal notice bundle")
    except Exception as exc:
        print(f"validation failed: {exc}", file=sys.stderr)
        return 1
    print(f"validated compliance bundle: {bundle_dir / 'compliance-bundle-manifest.json'}")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    generate = subparsers.add_parser("generate", help="generate compliance bundle")
    generate.add_argument("--out-dir", type=pathlib.Path, required=True)
    generate.add_argument("--bundle-manifest", type=pathlib.Path, required=True)
    generate.add_argument("--signing-manifest", type=pathlib.Path, required=True)
    generate.add_argument("--triage-report", type=pathlib.Path, required=True)

    validate = subparsers.add_parser("validate", help="validate a compliance bundle")
    validate.add_argument("--bundle-dir", type=pathlib.Path, required=True)
    return parser


def main(argv: Optional[List[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    repo_roots = default_repo_roots(pathlib.Path(__file__))
    if args.command == "generate":
        manifest_path = generate_bundle(
            args.out_dir.resolve(),
            args.bundle_manifest.resolve(),
            args.signing_manifest.resolve(),
            args.triage_report.resolve(),
            repo_roots,
        )
        print(f"wrote compliance bundle: {manifest_path}")
        return 0
    if args.command == "validate":
        return validate_bundle(args.bundle_dir.resolve())
    parser.error("unsupported command")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
