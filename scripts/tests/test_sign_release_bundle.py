#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import json
import pathlib
import subprocess
import sys
import tempfile
import unittest


SCRIPT_PATH = pathlib.Path(__file__).resolve().parents[1] / "release" / "sign_release_bundle.py"
SPEC = importlib.util.spec_from_file_location("sign_release_bundle", SCRIPT_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class SignReleaseBundleTest(unittest.TestCase):
    def generate_keypair(self, directory: pathlib.Path) -> tuple[pathlib.Path, pathlib.Path]:
        private_key = directory / "release.pem"
        public_key = directory / "release.pub.pem"
        subprocess.check_call(
            [
                "openssl",
                "genpkey",
                "-algorithm",
                "RSA",
                "-pkeyopt",
                "rsa_keygen_bits:2048",
                "-out",
                str(private_key),
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        subprocess.check_call(
            ["openssl", "pkey", "-in", str(private_key), "-pubout", "-out", str(public_key)],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        return private_key, public_key

    def create_bundle(self, directory: pathlib.Path) -> pathlib.Path:
        artifact = directory / "example-artifact.json"
        artifact.write_text('{"status":"ok"}\n', encoding="utf-8")
        manifest = {
            "schema": "scratchbird.release.sbom_bundle_manifest.v1",
            "bundle_outputs": [
                {
                    "path": artifact.name,
                    "sha256": MODULE.sha256_file(artifact),
                }
            ],
            "repositories": [],
        }
        manifest_path = directory / "release-sbom-manifest.json"
        manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        return manifest_path

    def test_sign_and_verify_bundle(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            bundle_dir = root / "bundle"
            bundle_dir.mkdir()
            manifest_path = self.create_bundle(bundle_dir)
            private_key, public_key = self.generate_keypair(root)
            signing_dir = root / "signed"

            signing_manifest = MODULE.sign_bundle(
                manifest_path,
                private_key,
                public_key,
                signing_dir,
                [root],
                "unit-test-key",
            )

            self.assertEqual(MODULE.verify_bundle(manifest_path, signing_manifest), 0)
            self.assertTrue((signing_dir / "release-bundle-provenance.json").exists())

    def test_verify_fails_after_subject_tamper(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            bundle_dir = root / "bundle"
            bundle_dir.mkdir()
            manifest_path = self.create_bundle(bundle_dir)
            private_key, public_key = self.generate_keypair(root)
            signing_manifest = MODULE.sign_bundle(
                manifest_path,
                private_key,
                public_key,
                root / "signed",
                [root],
                "unit-test-key",
            )

            (bundle_dir / "example-artifact.json").write_text('{"status":"tampered"}\n', encoding="utf-8")
            self.assertEqual(MODULE.verify_bundle(manifest_path, signing_manifest), 1)


if __name__ == "__main__":
    unittest.main()
