#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import json
import pathlib
import sys
import tempfile
import unittest


SCRIPT_PATH = pathlib.Path(__file__).resolve().parents[1] / "release" / "generate_sbom_inventory.py"
SPEC = importlib.util.spec_from_file_location("generate_sbom_inventory", SCRIPT_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class GenerateSbomInventoryTest(unittest.TestCase):
    def test_collect_repo_inventory_skips_generated_dirs_and_collects_dependencies(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            repo_root = pathlib.Path(tmp) / "Repo"
            repo_root.mkdir()
            (repo_root / "package.json").write_text(
                json.dumps(
                    {
                        "name": "root-node",
                        "version": "1.0.0",
                        "dependencies": {"left-pad": "^1.0.0"},
                    }
                )
                + "\n",
                encoding="utf-8",
            )
            (repo_root / "build").mkdir()
            (repo_root / "build" / "package.json").write_text('{"name":"skip-me"}\n', encoding="utf-8")
            (repo_root / "service").mkdir()
            (repo_root / "service" / "pyproject.toml").write_text(
                "[project]\nname = 'svc'\nversion = '0.4.0'\ndependencies = ['requests>=2']\n",
                encoding="utf-8",
            )

            inventory = MODULE.collect_repo_inventory(MODULE.RepoSpec("Repo", repo_root))

            self.assertEqual(inventory["component_count"], 2)
            component_names = {component["component_name"] for component in inventory["components"]}
            self.assertEqual(component_names, {"root-node", "svc"})

        self.assertTrue(True)

    def test_generate_and_validate_bundle(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            repo_one = root / "ScratchBird"
            repo_two = root / "ScratchBird-driver"
            repo_one.mkdir()
            repo_two.mkdir()
            (repo_one / "vcpkg.json").write_text(
                json.dumps({"name": "scratchbird", "dependencies": ["openssl", {"name": "zlib", "version>=": "1.3"}]})
                + "\n",
                encoding="utf-8",
            )
            (repo_two / "tracks").mkdir()
            (repo_two / "tracks" / "alpha").mkdir(parents=True, exist_ok=True)
            (repo_two / "tracks" / "alpha" / "drivers").mkdir(parents=True, exist_ok=True)
            node_dir = repo_two / "tracks" / "alpha" / "drivers" / "node"
            node_dir.mkdir(parents=True)
            (node_dir / "package.json").write_text(
                json.dumps({"name": "@scratchbird/node", "version": "0.2.0", "dependencies": {"ws": "^8.0.0"}})
                + "\n",
                encoding="utf-8",
            )
            (node_dir / "package-lock.json").write_text('{"name":"@scratchbird/node","lockfileVersion":3}\n', encoding="utf-8")

            out_dir = root / "out"
            repos = [
                MODULE.RepoSpec("ScratchBird", repo_one),
                MODULE.RepoSpec("ScratchBird-driver", repo_two),
            ]
            MODULE.write_outputs(repos, out_dir)

            self.assertEqual(MODULE.validate_outputs(out_dir), 0)
            manifest = json.loads((out_dir / "release-sbom-manifest.json").read_text(encoding="utf-8"))
            output_names = {entry["path"] for entry in manifest["bundle_outputs"]}
            self.assertIn("scratchbird-dependency-inventory.json", output_names)
            self.assertIn("scratchbird-driver-sbom.json", output_names)

    def test_parse_multiple_manifest_types(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            cargo_dir = root / "rust"
            cargo_dir.mkdir()
            (cargo_dir / "Cargo.toml").write_text(
                "[package]\nname = 'sb-rust'\nversion = '0.1.0'\n[dependencies]\nbytes = '1'\n",
                encoding="utf-8",
            )
            maven_dir = root / "jdbc"
            maven_dir.mkdir()
            (maven_dir / "pom.xml").write_text(
                "<project><groupId>com.scratchbird</groupId><artifactId>jdbc</artifactId><version>0.1.0</version><dependencies><dependency><groupId>org.slf4j</groupId><artifactId>slf4j-api</artifactId><version>2.0.0</version></dependency></dependencies></project>",
                encoding="utf-8",
            )

            cargo_component = MODULE.build_component(MODULE.RepoSpec("Repo", root), cargo_dir / "Cargo.toml")
            pom_component = MODULE.build_component(MODULE.RepoSpec("Repo", root), maven_dir / "pom.xml")

            self.assertEqual(cargo_component["component_name"], "sb-rust")
            self.assertEqual(cargo_component["dependencies"][0]["name"], "bytes")
            self.assertEqual(pom_component["component_name"], "com.scratchbird:jdbc")
            self.assertEqual(pom_component["dependencies"][0]["name"], "org.slf4j:slf4j-api")


if __name__ == "__main__":
    unittest.main()
