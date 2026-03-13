#!/usr/bin/env python3
"""Sign and verify release bundle manifests plus provenance attestations."""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
import pathlib
import subprocess
import sys
from typing import Any, Dict, List, Optional


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


def git_dirty(repo_root: pathlib.Path) -> bool:
    try:
        status = subprocess.check_output(
            ["git", "-C", str(repo_root), "status", "--porcelain"],
            stderr=subprocess.DEVNULL,
            text=True,
        )
        return bool(status.strip())
    except Exception:
        return False


def run_openssl(*args: str) -> bytes:
    return subprocess.check_output(["openssl", *args], stderr=subprocess.STDOUT)


def load_bundle_manifest(path: pathlib.Path) -> Dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def validate_subject_hashes(bundle_manifest_path: pathlib.Path) -> List[Dict[str, str]]:
    bundle_dir = bundle_manifest_path.parent
    manifest = load_bundle_manifest(bundle_manifest_path)
    subjects = manifest.get("bundle_outputs", [])
    if not subjects:
        raise ValueError("bundle manifest has no subjects")
    for entry in subjects:
        target = bundle_dir / entry["path"]
        if not target.exists():
            raise ValueError(f"missing subject: {target}")
        actual = sha256_file(target)
        if actual != entry["sha256"]:
            raise ValueError(f"sha256 mismatch for {target}")
    return subjects


def resolve_public_key(private_key: pathlib.Path, public_key: Optional[pathlib.Path], out_dir: pathlib.Path) -> pathlib.Path:
    derived_path = out_dir / "release-bundle.public.pem"
    if public_key is not None:
        derived_path.write_bytes(public_key.read_bytes())
        return derived_path
    run_openssl("pkey", "-in", str(private_key), "-pubout", "-out", str(derived_path))
    return derived_path


def build_provenance(
    bundle_manifest_path: pathlib.Path,
    subjects: List[Dict[str, str]],
    public_key_path: pathlib.Path,
    repo_paths: List[pathlib.Path],
    signing_key_id: str,
) -> Dict[str, Any]:
    repos: List[Dict[str, Any]] = []
    for repo_root in repo_paths:
        repos.append(
            {
                "repo_name": repo_root.name,
                "repo_root": str(repo_root),
                "git_head": git_value(repo_root, "rev-parse", "HEAD"),
                "git_commit_timestamp": git_value(repo_root, "show", "-s", "--format=%cI", "HEAD"),
                "dirty": git_dirty(repo_root),
            }
        )
    return {
        "schema": "scratchbird.release.provenance.v1",
        "bundle_manifest": {
            "path": bundle_manifest_path.name,
            "sha256": sha256_file(bundle_manifest_path),
        },
        "subjects": sorted(subjects, key=lambda item: item["path"]),
        "signer": {
            "algorithm": "rsa-sha256",
            "key_id": signing_key_id,
            "public_key_path": public_key_path.name,
            "public_key_sha256": sha256_file(public_key_path),
        },
        "repositories": repos,
    }


def sign_bytes(payload: bytes, private_key: pathlib.Path) -> bytes:
    proc = subprocess.run(
        ["openssl", "dgst", "-sha256", "-sign", str(private_key)],
        input=payload,
        check=True,
        capture_output=True,
    )
    return proc.stdout


def verify_bytes(payload: bytes, signature: bytes, public_key: pathlib.Path) -> None:
    proc = subprocess.run(
        ["openssl", "dgst", "-sha256", "-verify", str(public_key), "-signature", "/dev/stdin"],
        input=signature,
        check=False,
        capture_output=True,
    )
    if proc.returncode == 0:
        verify_proc = subprocess.run(
            ["openssl", "dgst", "-sha256", "-verify", str(public_key), "-signature", "/dev/stdin"],
            input=signature,
            check=False,
            capture_output=True,
        )
        if verify_proc.returncode != 0:
            raise ValueError("signature verification failed")
        return

    # Fallback path for payload verification using temp files.
    raise ValueError("signature verification failed")


def sign_bundle(
    bundle_manifest_path: pathlib.Path,
    private_key: pathlib.Path,
    public_key: Optional[pathlib.Path],
    out_dir: pathlib.Path,
    repo_paths: List[pathlib.Path],
    signing_key_id: str,
) -> pathlib.Path:
    subjects = validate_subject_hashes(bundle_manifest_path)
    out_dir.mkdir(parents=True, exist_ok=True)
    public_key_path = resolve_public_key(private_key, public_key, out_dir)

    provenance = build_provenance(bundle_manifest_path, subjects, public_key_path, repo_paths, signing_key_id)
    provenance_path = out_dir / "release-bundle-provenance.json"
    provenance_path.write_text(stable_json(provenance), encoding="utf-8")

    manifest_sig = sign_bytes(bundle_manifest_path.read_bytes(), private_key)
    manifest_sig_path = out_dir / "release-bundle-manifest.sig"
    manifest_sig_path.write_bytes(manifest_sig)

    provenance_sig = sign_bytes(provenance_path.read_bytes(), private_key)
    provenance_sig_path = out_dir / "release-bundle-provenance.sig"
    provenance_sig_path.write_bytes(provenance_sig)

    signing_manifest = {
        "schema": "scratchbird.release.signature_bundle.v1",
        "bundle_manifest": {
            "path": bundle_manifest_path.name,
            "sha256": sha256_file(bundle_manifest_path),
        },
        "provenance": {
            "path": provenance_path.name,
            "sha256": sha256_file(provenance_path),
        },
        "public_key": {
            "path": public_key_path.name,
            "sha256": sha256_file(public_key_path),
        },
        "signatures": [
            {
                "path": manifest_sig_path.name,
                "target": bundle_manifest_path.name,
                "sha256": sha256_file(manifest_sig_path),
                "encoding": "binary",
                "algorithm": "rsa-sha256",
            },
            {
                "path": provenance_sig_path.name,
                "target": provenance_path.name,
                "sha256": sha256_file(provenance_sig_path),
                "encoding": "binary",
                "algorithm": "rsa-sha256",
            },
        ],
    }
    signing_manifest_path = out_dir / "release-bundle-signing-manifest.json"
    signing_manifest_path.write_text(stable_json(signing_manifest), encoding="utf-8")
    return signing_manifest_path


def verify_with_openssl(payload_path: pathlib.Path, signature_path: pathlib.Path, public_key_path: pathlib.Path) -> None:
    proc = subprocess.run(
        [
            "openssl",
            "dgst",
            "-sha256",
            "-verify",
            str(public_key_path),
            "-signature",
            str(signature_path),
            str(payload_path),
        ],
        check=False,
        capture_output=True,
        text=True,
    )
    if proc.returncode != 0:
        raise ValueError(proc.stderr.strip() or proc.stdout.strip() or "signature verification failed")


def verify_bundle(bundle_manifest_path: pathlib.Path, signing_manifest_path: pathlib.Path) -> int:
    try:
        subjects = validate_subject_hashes(bundle_manifest_path)
        bundle_dir = bundle_manifest_path.parent
        signing_dir = signing_manifest_path.parent
        signing_manifest = json.loads(signing_manifest_path.read_text(encoding="utf-8"))

        public_key_path = signing_dir / signing_manifest["public_key"]["path"]
        if sha256_file(public_key_path) != signing_manifest["public_key"]["sha256"]:
            raise ValueError("public key hash mismatch")

        provenance_path = signing_dir / signing_manifest["provenance"]["path"]
        if sha256_file(provenance_path) != signing_manifest["provenance"]["sha256"]:
            raise ValueError("provenance hash mismatch")

        for entry in signing_manifest["signatures"]:
            signature_path = signing_dir / entry["path"]
            if sha256_file(signature_path) != entry["sha256"]:
                raise ValueError(f"signature hash mismatch: {signature_path}")
            target_path = bundle_dir / entry["target"] if entry["target"] == bundle_manifest_path.name else signing_dir / entry["target"]
            verify_with_openssl(target_path, signature_path, public_key_path)

        provenance = json.loads(provenance_path.read_text(encoding="utf-8"))
        if provenance["bundle_manifest"]["sha256"] != sha256_file(bundle_manifest_path):
            raise ValueError("provenance bundle manifest hash mismatch")
        provenance_subjects = sorted(provenance.get("subjects", []), key=lambda item: item["path"])
        if provenance_subjects != sorted(subjects, key=lambda item: item["path"]):
            raise ValueError("provenance subjects do not match bundle manifest")
    except Exception as exc:
        print(f"verification failed: {exc}", file=sys.stderr)
        return 1
    print(f"verified signed bundle: {signing_manifest_path}")
    return 0


def default_repo_paths(script_path: pathlib.Path) -> List[pathlib.Path]:
    scratchbird_root = script_path.resolve().parents[2]
    parent_root = scratchbird_root.parent
    paths = [scratchbird_root]
    sibling_driver = parent_root / "ScratchBird-driver"
    if sibling_driver.exists():
        paths.append(sibling_driver)
    return paths


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    sign = subparsers.add_parser("sign", help="sign a release bundle manifest and provenance")
    sign.add_argument("--bundle-manifest", type=pathlib.Path, required=True)
    sign.add_argument("--private-key", type=pathlib.Path, required=True)
    sign.add_argument("--public-key", type=pathlib.Path)
    sign.add_argument("--out-dir", type=pathlib.Path, required=True)
    sign.add_argument("--repo-path", action="append", type=pathlib.Path, default=None)
    sign.add_argument("--signing-key-id", default="release-default")

    verify = subparsers.add_parser("verify", help="verify a signed release bundle")
    verify.add_argument("--bundle-manifest", type=pathlib.Path, required=True)
    verify.add_argument("--signing-manifest", type=pathlib.Path, required=True)

    return parser


def main(argv: Optional[List[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    script_path = pathlib.Path(__file__)

    if args.command == "sign":
        repo_paths = [path.resolve() for path in (args.repo_path or default_repo_paths(script_path))]
        signing_manifest = sign_bundle(
            args.bundle_manifest.resolve(),
            args.private_key.resolve(),
            args.public_key.resolve() if args.public_key else None,
            args.out_dir.resolve(),
            repo_paths,
            args.signing_key_id,
        )
        print(f"wrote signed bundle: {signing_manifest}")
        return 0
    if args.command == "verify":
        return verify_bundle(args.bundle_manifest.resolve(), args.signing_manifest.resolve())
    parser.error("unsupported command")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
