#!/usr/bin/env python3
"""Deterministic SBOM and dependency-inventory generator for ScratchBird lanes."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import re
import subprocess
import sys
import tomllib
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from typing import Any, Dict, Iterable, List, Optional


ROOT_INPUT_FILES = (
    "CMakePresets.json",
    "vcpkg-configuration.json",
)

PRIMARY_MANIFESTS = {
    "vcpkg.json": "vcpkg",
    "package.json": "npm",
    "composer.json": "composer",
    "pyproject.toml": "pyproject",
    "Cargo.toml": "cargo",
    "go.mod": "go",
    "Package.swift": "swift",
    "pubspec.yaml": "dart",
    "mix.exs": "elixir",
    "build.gradle": "gradle",
    "build.gradle.kts": "gradle-kts",
    "pom.xml": "maven",
}

LOCKFILES = {
    "package-lock.json": "npm-lock",
    "composer.lock": "composer-lock",
    "Cargo.lock": "cargo-lock",
    "mix.lock": "elixir-lock",
    "Package.resolved": "swift-lock",
    "pubspec.lock": "dart-lock",
}

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
    "build",
    "deps",
    "dist",
    "node_modules",
    "obj",
    "results",
    "target",
    "vendor",
}


@dataclass(frozen=True)
class RepoSpec:
    repo_id: str
    repo_root: pathlib.Path


def sha256_file(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def stable_json(data: Any) -> str:
    return json.dumps(data, indent=2, sort_keys=True) + "\n"


def git_value(repo_root: pathlib.Path, *args: str) -> str:
    try:
        return subprocess.check_output(
            ["git", "-C", str(repo_root), *args],
            stderr=subprocess.DEVNULL,
            text=True,
        ).strip()
    except Exception:
        return "unknown"


def repo_slug(repo_id: str) -> str:
    return re.sub(r"[^a-z0-9]+", "-", repo_id.lower()).strip("-")


def should_skip_dir(dir_name: str) -> bool:
    return dir_name in SKIP_DIR_NAMES


def should_skip_path(path: pathlib.Path, repo_root: pathlib.Path) -> bool:
    rel_parts = path.relative_to(repo_root).parts
    if len(rel_parts) >= 3 and rel_parts[0] == "tests" and rel_parts[1] == "compatibility" and rel_parts[2] == "firebird":
        return "repos" in rel_parts
    return False


def iter_primary_manifests(repo_root: pathlib.Path) -> Iterable[pathlib.Path]:
    for current_root, dir_names, file_names in os.walk(repo_root):
        current_path = pathlib.Path(current_root)
        dir_names[:] = [
            name
            for name in dir_names
            if not should_skip_dir(name)
            and not should_skip_path(current_path / name, repo_root)
        ]
        if should_skip_path(current_path, repo_root):
            continue
        for file_name in sorted(file_names):
            if file_name in PRIMARY_MANIFESTS:
                yield current_path / file_name


def find_associated_lockfiles(manifest_path: pathlib.Path) -> List[pathlib.Path]:
    parent = manifest_path.parent
    found: List[pathlib.Path] = []
    for file_name in sorted(LOCKFILES):
        candidate = parent / file_name
        if candidate.exists():
            found.append(candidate)
    return found


def dependency_record(
    name: str,
    version: str,
    scope: str,
    source: str,
    extraction: str,
) -> Dict[str, str]:
    return {
        "extraction": extraction,
        "name": name,
        "scope": scope,
        "source": source,
        "version": version,
    }


def parse_string_requirement(raw: str) -> Dict[str, str]:
    match = re.match(r"^\s*([A-Za-z0-9_.@/+:-]+)\s*(.*)$", raw.strip())
    if not match:
        return {"name": raw.strip(), "version": "unspecified"}
    tail = match.group(2).strip()
    return {"name": match.group(1), "version": tail or "unspecified"}


def parse_vcpkg(path: pathlib.Path) -> Dict[str, Any]:
    data = json.loads(path.read_text(encoding="utf-8"))
    deps: List[Dict[str, str]] = []
    for item in data.get("dependencies", []):
        if isinstance(item, str):
            deps.append(dependency_record(item, "unspecified", "runtime", "dependencies", "structured"))
        elif isinstance(item, dict):
            deps.append(
                dependency_record(
                    item.get("name", "unknown"),
                    item.get("version>=", item.get("version", "unspecified")),
                    "runtime",
                    "dependencies",
                    "structured",
                )
            )
    return {
        "component_name": data.get("name", path.parent.name),
        "component_version": data.get("version-string", data.get("version", "NOASSERTION")),
        "dependencies": deps,
        "metadata": {"builtin-baseline": data.get("builtin-baseline", "unknown")},
    }


def parse_package_json(path: pathlib.Path) -> Dict[str, Any]:
    data = json.loads(path.read_text(encoding="utf-8"))
    deps: List[Dict[str, str]] = []
    for key, scope in (
        ("dependencies", "runtime"),
        ("devDependencies", "dev"),
        ("peerDependencies", "peer"),
        ("optionalDependencies", "optional"),
    ):
        for name, version in sorted(data.get(key, {}).items()):
            deps.append(dependency_record(name, str(version), scope, key, "structured"))
    return {
        "component_name": data.get("name", path.parent.name),
        "component_version": data.get("version", "NOASSERTION"),
        "dependencies": deps,
        "metadata": {"package_manager": data.get("packageManager", "")},
    }


def parse_composer_json(path: pathlib.Path) -> Dict[str, Any]:
    data = json.loads(path.read_text(encoding="utf-8"))
    deps: List[Dict[str, str]] = []
    for key, scope in (("require", "runtime"), ("require-dev", "dev")):
        for name, version in sorted(data.get(key, {}).items()):
            deps.append(dependency_record(name, str(version), scope, key, "structured"))
    return {
        "component_name": data.get("name", path.parent.name),
        "component_version": data.get("version", "NOASSERTION"),
        "dependencies": deps,
        "metadata": {},
    }


def parse_pyproject(path: pathlib.Path) -> Dict[str, Any]:
    data = tomllib.loads(path.read_text(encoding="utf-8"))
    project = data.get("project", {})
    poetry = data.get("tool", {}).get("poetry", {})
    name = project.get("name") or poetry.get("name") or path.parent.name
    version = project.get("version") or poetry.get("version") or "NOASSERTION"
    deps: List[Dict[str, str]] = []

    for item in project.get("dependencies", []):
        parsed = parse_string_requirement(str(item))
        deps.append(dependency_record(parsed["name"], parsed["version"], "runtime", "project.dependencies", "structured"))

    for group_name, items in sorted(project.get("optional-dependencies", {}).items()):
        for item in items:
            parsed = parse_string_requirement(str(item))
            deps.append(
                dependency_record(
                    parsed["name"],
                    parsed["version"],
                    f"optional:{group_name}",
                    "project.optional-dependencies",
                    "structured",
                )
            )

    for name_key, value in sorted(poetry.get("dependencies", {}).items()):
        if name_key == "python":
            continue
        version_value = value if isinstance(value, str) else value.get("version", "unspecified")
        deps.append(dependency_record(name_key, str(version_value), "runtime", "tool.poetry.dependencies", "structured"))

    return {
        "component_name": name,
        "component_version": version,
        "dependencies": deps,
        "metadata": {},
    }


def parse_cargo_toml(path: pathlib.Path) -> Dict[str, Any]:
    data = tomllib.loads(path.read_text(encoding="utf-8"))
    package = data.get("package", {})
    deps: List[Dict[str, str]] = []
    for key, scope in (
        ("dependencies", "runtime"),
        ("dev-dependencies", "dev"),
        ("build-dependencies", "build"),
    ):
        for dep_name, value in sorted(data.get(key, {}).items()):
            if isinstance(value, str):
                version = value
            elif isinstance(value, dict):
                version = str(value.get("version", "unspecified"))
            else:
                version = "unspecified"
            deps.append(dependency_record(dep_name, version, scope, key, "structured"))
    return {
        "component_name": package.get("name", path.parent.name),
        "component_version": package.get("version", "NOASSERTION"),
        "dependencies": deps,
        "metadata": {},
    }


def parse_go_mod(path: pathlib.Path) -> Dict[str, Any]:
    module_name = path.parent.name
    deps: List[Dict[str, str]] = []
    in_require_block = False
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("//"):
            continue
        if line.startswith("module "):
            module_name = line.split(maxsplit=1)[1].strip()
            continue
        if line.startswith("require ("):
            in_require_block = True
            continue
        if in_require_block and line == ")":
            in_require_block = False
            continue
        if line.startswith("require "):
            entry = line[len("require ") :].strip()
        elif in_require_block:
            entry = line
        else:
            continue
        parts = entry.split()
        if len(parts) >= 2:
            deps.append(dependency_record(parts[0], parts[1], "runtime", "go.mod", "structured"))
    return {
        "component_name": module_name,
        "component_version": "NOASSERTION",
        "dependencies": deps,
        "metadata": {},
    }


def parse_pubspec(path: pathlib.Path) -> Dict[str, Any]:
    name = path.parent.name
    version = "NOASSERTION"
    deps: List[Dict[str, str]] = []
    current_scope: Optional[str] = None
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        if raw_line.startswith("name:"):
            name = raw_line.split(":", 1)[1].strip()
            continue
        if raw_line.startswith("version:"):
            version = raw_line.split(":", 1)[1].strip()
            continue
        if re.match(r"^[A-Za-z_][A-Za-z0-9_-]*:\s*$", raw_line) and raw_line.strip().rstrip(":") in {
            "dependencies",
            "dev_dependencies",
        }:
            current_scope = "runtime" if raw_line.strip().startswith("dependencies") else "dev"
            continue
        if current_scope and raw_line.startswith("  ") and ":" in raw_line:
            dep_name, dep_version = raw_line.strip().split(":", 1)
            deps.append(dependency_record(dep_name.strip(), dep_version.strip() or "unspecified", current_scope, "pubspec.yaml", "heuristic"))
        elif raw_line and not raw_line.startswith(" "):
            current_scope = None
    return {
        "component_name": name,
        "component_version": version,
        "dependencies": deps,
        "metadata": {},
    }


def parse_mix_exs(path: pathlib.Path) -> Dict[str, Any]:
    text = path.read_text(encoding="utf-8")
    deps: List[Dict[str, str]] = []
    for name, version in re.findall(r"\{\s*:([A-Za-z0-9_]+)\s*,\s*\"([^\"]+)\"", text):
        deps.append(dependency_record(name, version, "runtime", "mix.exs", "heuristic"))
    return {
        "component_name": path.parent.name,
        "component_version": "NOASSERTION",
        "dependencies": deps,
        "metadata": {},
    }


def parse_swift_package(path: pathlib.Path) -> Dict[str, Any]:
    text = path.read_text(encoding="utf-8")
    name_match = re.search(r"name:\s*\"([^\"]+)\"", text)
    deps: List[Dict[str, str]] = []
    for url, version in re.findall(r"\.package\(\s*(?:url|path):\s*\"([^\"]+)\"(?:,\s*(?:from|exact):\s*\"([^\"]+)\")?", text):
        deps.append(dependency_record(url, version or "unspecified", "runtime", "Package.swift", "heuristic"))
    return {
        "component_name": name_match.group(1) if name_match else path.parent.name,
        "component_version": "NOASSERTION",
        "dependencies": deps,
        "metadata": {},
    }


def parse_gradle(path: pathlib.Path) -> Dict[str, Any]:
    deps: List[Dict[str, str]] = []
    text = path.read_text(encoding="utf-8")
    pattern = re.compile(
        r"^\s*(implementation|api|compileOnly|runtimeOnly|testImplementation|testRuntimeOnly)\s+[('\"]([^:'\"\s]+):([^:'\"\s]+):([^'\"\s)]+)",
        re.MULTILINE,
    )
    for scope, group, artifact, version in pattern.findall(text):
        scope_name = "dev" if scope.startswith("test") else "runtime"
        deps.append(
            dependency_record(
                f"{group}:{artifact}",
                version,
                scope_name,
                path.name,
                "heuristic",
            )
        )
    return {
        "component_name": path.parent.name,
        "component_version": "NOASSERTION",
        "dependencies": deps,
        "metadata": {},
    }


def strip_xml_namespaces(xml_text: str) -> str:
    return re.sub(r'\sxmlns="[^"]+"', "", xml_text, count=1)


def parse_pom(path: pathlib.Path) -> Dict[str, Any]:
    root = ET.fromstring(strip_xml_namespaces(path.read_text(encoding="utf-8")))
    artifact_id = root.findtext("./artifactId") or path.parent.name
    version = root.findtext("./version") or "NOASSERTION"
    group_id = root.findtext("./groupId") or root.findtext("./parent/groupId") or ""
    deps: List[Dict[str, str]] = []
    for dependency in root.findall("./dependencies/dependency"):
        dep_group = dependency.findtext("groupId") or "unknown"
        dep_artifact = dependency.findtext("artifactId") or "unknown"
        dep_version = dependency.findtext("version") or "unspecified"
        scope = dependency.findtext("scope") or "runtime"
        deps.append(
            dependency_record(
                f"{dep_group}:{dep_artifact}",
                dep_version,
                scope,
                "pom.xml",
                "structured",
            )
        )
    name = f"{group_id}:{artifact_id}" if group_id else artifact_id
    return {
        "component_name": name,
        "component_version": version,
        "dependencies": deps,
        "metadata": {},
    }


def parse_cmake_placeholder(path: pathlib.Path) -> Dict[str, Any]:
    return {
        "component_name": path.parent.name,
        "component_version": "NOASSERTION",
        "dependencies": [],
        "metadata": {"note": "cmake input hashed at repo root"},
    }


PARSERS = {
    "vcpkg.json": parse_vcpkg,
    "package.json": parse_package_json,
    "composer.json": parse_composer_json,
    "pyproject.toml": parse_pyproject,
    "Cargo.toml": parse_cargo_toml,
    "go.mod": parse_go_mod,
    "Package.swift": parse_swift_package,
    "pubspec.yaml": parse_pubspec,
    "mix.exs": parse_mix_exs,
    "build.gradle": parse_gradle,
    "build.gradle.kts": parse_gradle,
    "pom.xml": parse_pom,
    "CMakePresets.json": parse_cmake_placeholder,
}


def build_component(repo: RepoSpec, manifest_path: pathlib.Path) -> Dict[str, Any]:
    parser = PARSERS[manifest_path.name]
    parsed = parser(manifest_path)
    rel_manifest = manifest_path.relative_to(repo.repo_root).as_posix()
    component_id = f"{repo_slug(repo.repo_id)}:{rel_manifest}"
    lockfiles = find_associated_lockfiles(manifest_path)
    return {
        "bom_ref": component_id,
        "component_name": parsed["component_name"],
        "component_version": parsed["component_version"],
        "dependencies": sorted(
            parsed["dependencies"],
            key=lambda item: (item["scope"], item["name"], item["version"]),
        ),
        "inputs": [
            {
                "path": rel_manifest,
                "role": "manifest",
                "sha256": sha256_file(manifest_path),
                "type": PRIMARY_MANIFESTS[manifest_path.name],
            },
            *[
                {
                    "path": lockfile.relative_to(repo.repo_root).as_posix(),
                    "role": "lockfile",
                    "sha256": sha256_file(lockfile),
                    "type": LOCKFILES[lockfile.name],
                }
                for lockfile in lockfiles
            ],
        ],
        "manifest_path": rel_manifest,
        "manifest_type": PRIMARY_MANIFESTS[manifest_path.name],
        "metadata": parsed["metadata"],
    }


def collect_repo_inventory(repo: RepoSpec) -> Dict[str, Any]:
    root_inputs = []
    for file_name in ROOT_INPUT_FILES:
        candidate = repo.repo_root / file_name
        if candidate.exists():
            root_inputs.append(
                {
                    "path": file_name,
                    "sha256": sha256_file(candidate),
                    "type": "repo-input",
                }
            )

    components = [
        build_component(repo, manifest_path)
        for manifest_path in sorted(iter_primary_manifests(repo.repo_root))
    ]
    dependency_count = sum(len(component["dependencies"]) for component in components)
    return {
        "schema": "scratchbird.release.dependency_inventory.v1",
        "repo_id": repo.repo_id,
        "repo_root": str(repo.repo_root),
        "repo_slug": repo_slug(repo.repo_id),
        "git_head": git_value(repo.repo_root, "rev-parse", "HEAD"),
        "git_commit_timestamp": git_value(repo.repo_root, "show", "-s", "--format=%cI", "HEAD"),
        "root_inputs": root_inputs,
        "component_count": len(components),
        "dependency_count": dependency_count,
        "components": components,
    }


def build_repo_sbom(inventory: Dict[str, Any]) -> Dict[str, Any]:
    components: List[Dict[str, Any]] = []
    graph: List[Dict[str, Any]] = []
    for component in inventory["components"]:
        component_hashes = [
            {"alg": "SHA-256", "content": item["sha256"], "path": item["path"], "role": item["role"]}
            for item in component["inputs"]
        ]
        dependency_refs = [
            {
                "name": dep["name"],
                "scope": dep["scope"],
                "source": dep["source"],
                "version": dep["version"],
            }
            for dep in component["dependencies"]
        ]
        components.append(
            {
                "bom_ref": component["bom_ref"],
                "name": component["component_name"],
                "version": component["component_version"],
                "type": component["manifest_type"],
                "path": component["manifest_path"],
                "hashes": component_hashes,
                "metadata": component["metadata"],
            }
        )
        graph.append({"bom_ref": component["bom_ref"], "depends_on": dependency_refs})
    return {
        "schema": "scratchbird.release.sbom.v1",
        "repo_id": inventory["repo_id"],
        "repo_slug": inventory["repo_slug"],
        "git_head": inventory["git_head"],
        "git_commit_timestamp": inventory["git_commit_timestamp"],
        "components": components,
        "dependency_graph": graph,
        "root_inputs": inventory["root_inputs"],
    }


def write_outputs(repos: List[RepoSpec], out_dir: pathlib.Path) -> Dict[str, Any]:
    out_dir.mkdir(parents=True, exist_ok=True)
    manifest: Dict[str, Any] = {
        "schema": "scratchbird.release.sbom_bundle_manifest.v1",
        "bundle_outputs": [],
        "repositories": [],
    }
    for repo in repos:
        inventory = collect_repo_inventory(repo)
        sbom = build_repo_sbom(inventory)
        slug = repo_slug(repo.repo_id)
        inventory_path = out_dir / f"{slug}-dependency-inventory.json"
        sbom_path = out_dir / f"{slug}-sbom.json"
        inventory_path.write_text(stable_json(inventory), encoding="utf-8")
        sbom_path.write_text(stable_json(sbom), encoding="utf-8")
        manifest["repositories"].append(
            {
                "repo_id": repo.repo_id,
                "repo_slug": slug,
                "git_head": inventory["git_head"],
                "component_count": inventory["component_count"],
                "dependency_count": inventory["dependency_count"],
            }
        )
        manifest["bundle_outputs"].extend(
            [
                {"path": inventory_path.name, "sha256": sha256_file(inventory_path)},
                {"path": sbom_path.name, "sha256": sha256_file(sbom_path)},
            ]
        )
    manifest_path = out_dir / "release-sbom-manifest.json"
    manifest_path.write_text(stable_json(manifest), encoding="utf-8")
    return {
        "manifest_path": manifest_path,
        "generated": [out_dir / entry["path"] for entry in manifest["bundle_outputs"]],
    }


def validate_outputs(out_dir: pathlib.Path) -> int:
    manifest_path = out_dir / "release-sbom-manifest.json"
    if not manifest_path.exists():
        print(f"missing manifest: {manifest_path}", file=sys.stderr)
        return 1
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    for entry in manifest.get("bundle_outputs", []):
        file_path = out_dir / entry["path"]
        if not file_path.exists():
            print(f"missing output: {file_path}", file=sys.stderr)
            return 1
        actual = sha256_file(file_path)
        if actual != entry["sha256"]:
            print(f"sha256 mismatch for {file_path}", file=sys.stderr)
            return 1
        data = json.loads(file_path.read_text(encoding="utf-8"))
        if entry["path"].endswith("-dependency-inventory.json") and data.get("component_count", 0) <= 0:
            print(f"empty inventory: {file_path}", file=sys.stderr)
            return 1
        if entry["path"].endswith("-sbom.json") and not data.get("components"):
            print(f"empty sbom: {file_path}", file=sys.stderr)
            return 1
    print(f"validated SBOM bundle: {manifest_path}")
    return 0


def parse_repo_arg(raw: str) -> RepoSpec:
    if "=" not in raw:
        raise argparse.ArgumentTypeError("repo arguments must be in the form NAME=/abs/path")
    repo_id, raw_path = raw.split("=", 1)
    repo_root = pathlib.Path(raw_path).expanduser().resolve()
    if not repo_root.exists():
        raise argparse.ArgumentTypeError(f"repo path does not exist: {repo_root}")
    return RepoSpec(repo_id=repo_id, repo_root=repo_root)


def default_repo_specs(script_path: pathlib.Path) -> List[RepoSpec]:
    scratchbird_root = script_path.resolve().parents[2]
    parent_root = scratchbird_root.parent
    repos = [RepoSpec("ScratchBird", scratchbird_root)]
    sibling_driver = parent_root / "ScratchBird-driver"
    if sibling_driver.exists():
        repos.append(RepoSpec("ScratchBird-driver", sibling_driver))
    return repos


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    generate = subparsers.add_parser("generate", help="generate dependency inventory and SBOM bundle")
    generate.add_argument(
        "--repo",
        action="append",
        type=parse_repo_arg,
        default=None,
        help="repo in the form NAME=/abs/path; defaults to ScratchBird and sibling ScratchBird-driver when present",
    )
    generate.add_argument(
        "--out-dir",
        type=pathlib.Path,
        default=pathlib.Path("artifacts/release_integrity/sbom"),
        help="output directory for generated bundle files",
    )

    validate = subparsers.add_parser("validate", help="validate a generated SBOM bundle")
    validate.add_argument(
        "--out-dir",
        type=pathlib.Path,
        default=pathlib.Path("artifacts/release_integrity/sbom"),
        help="directory containing release-sbom-manifest.json",
    )
    return parser


def main(argv: Optional[List[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    script_path = pathlib.Path(__file__)

    if args.command == "generate":
        repos = args.repo or default_repo_specs(script_path)
        result = write_outputs(repos, args.out_dir)
        print(f"wrote SBOM bundle: {result['manifest_path']}")
        return 0
    if args.command == "validate":
        return validate_outputs(args.out_dir)
    parser.error("unsupported command")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
