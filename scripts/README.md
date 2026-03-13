# ScratchBird Project Statistics Scripts

**Purpose:** Automated generation of comprehensive project statistics and metrics  
**Last Updated:** 2026-01-03

---

## 📊 Available Scripts

### Verification Bundle (External Beta Validation)

Portable object: `scripts/verification_bundle/`

Use:
```bash
cd scripts/verification_bundle
sudo ./SB_Dev_Bootstrap.sh --yes
```

This provisions host dependencies, creates the `scratchbird` user/group, clones/updates source repos, runs full build + `ctest`, runs verification lanes, and emits a support zip artifact.

Legacy non-root entrypoint remains available:
```bash
./bootstrap.sh
```

For an already-provisioned local developer environment (repos already pulled, tools already installed), run:
```bash
./scripts/run_fresh_local_verification.sh
```

This archives prior verification artifacts, restarts the runtime stack, and executes a fresh verification pass.

### 1. generate-project-stats.sh
Generates comprehensive project statistics.

**Output:** `PROJECT_STATS.md` (project root)

**Usage:**
```bash
./scripts/generate-project-stats.sh
./scripts/generate-project-stats.sh --format json
./scripts/generate-project-stats.sh --output custom.md
```

### 2. beta-requirements-tracker.sh
Tracks Beta requirements completion status.

**Output:** `docs/specifications/beta_requirements/COMPLETION_STATUS.md`

**Usage:**
```bash
./scripts/beta-requirements-tracker.sh
```

### 3. documentation-coverage.sh
Analyzes documentation coverage.

**Output:** `docs/DOCUMENTATION_COVERAGE.md`

**Usage:**
```bash
./scripts/documentation-coverage.sh
```

### 4. generate-all-stats.sh
Master script that runs all three statistics scripts.

**Usage:**
```bash
./scripts/generate-all-stats.sh
```

### 5. release/generate_sbom_inventory.py
Generates deterministic dependency-inventory and SBOM bundle artifacts for the
release-integrity lane.

**Output:** `artifacts/release_integrity/sbom/`

**Usage:**
```bash
python3 scripts/release/generate_sbom_inventory.py generate
python3 scripts/release/generate_sbom_inventory.py validate
python3 scripts/release/generate_sbom_inventory.py generate \
  --repo ScratchBird=/home/dcalford/CliWork/ScratchBird \
  --repo ScratchBird-driver=/home/dcalford/CliWork/ScratchBird-driver \
  --out-dir /tmp/sb-release-sbom
```

### 6. release/sign_release_bundle.py
Signs a generated release bundle manifest and emits provenance attestations plus
verification metadata.

**Usage:**
```bash
python3 scripts/release/sign_release_bundle.py sign \
  --bundle-manifest /tmp/sb-release-sbom/release-sbom-manifest.json \
  --private-key /path/to/release.pem \
  --out-dir /tmp/sb-release-signatures
python3 scripts/release/sign_release_bundle.py verify \
  --bundle-manifest /tmp/sb-release-sbom/release-sbom-manifest.json \
  --signing-manifest /tmp/sb-release-signatures/release-bundle-signing-manifest.json
```

### 7. release/triage_cve_feed.py
Ingests advisory data against the generated dependency inventories and produces
deterministic patch-SLA triage plus remediation queue outputs.

**Usage:**
```bash
python3 scripts/release/triage_cve_feed.py triage \
  --bundle-manifest /tmp/sb-release-sbom/release-sbom-manifest.json \
  --advisories /tmp/cve-feed.json \
  --out-dir /tmp/sb-cve-triage \
  --now 2026-03-13T00:00:00Z
python3 scripts/release/triage_cve_feed.py validate \
  --out-dir /tmp/sb-cve-triage
```

### 8. release/verify_repro_build.py
Re-runs manifest generators, compares outputs byte-for-byte, and optionally
verifies the signed release bundle from the prior PH6 lanes.

**Usage:**
```bash
python3 scripts/release/verify_repro_build.py generate \
  --out-dir /tmp/sb-repro-check \
  --linux-build-dir /home/dcalford/CliWork/ScratchBird/build \
  --bundle-manifest /tmp/sb-release-sbom-ncw065/release-sbom-manifest.json \
  --signing-manifest /tmp/sb-release-signatures-ncw066/release-bundle-signing-manifest.json
python3 scripts/release/verify_repro_build.py validate \
  --report-path /tmp/sb-repro-check/repro-build-report.json
```

### 9. release/generate_compliance_bundle.py
Packages in-tree legal artifacts and the PH6 release-integrity outputs into one
deterministic compliance bundle.

**Usage:**
```bash
python3 scripts/release/generate_compliance_bundle.py generate \
  --out-dir /tmp/sb-compliance-bundle \
  --bundle-manifest /tmp/sb-release-sbom-ncw065/release-sbom-manifest.json \
  --signing-manifest /tmp/sb-release-signatures-ncw066/release-bundle-signing-manifest.json \
  --triage-report /tmp/sb-cve-triage-ncw067/cve-triage-report.json
python3 scripts/release/generate_compliance_bundle.py validate \
  --bundle-dir /tmp/sb-compliance-bundle
```

### 10. release/run_integrated_gameday.py
Executes the PH7 non-cluster integrated gameday by validating the PH6 release
bundle and running focused adversarial recovery, security, forensic, and
reliability drills against `scratchbird_tests`.

**Usage:**
```bash
python3 scripts/release/run_integrated_gameday.py generate \
  --out-dir /tmp/sb-integrated-gameday \
  --scratchbird-tests /home/dcalford/CliWork/ScratchBird/build/tests/scratchbird_tests \
  --bundle-manifest /tmp/sb-release-sbom-ncw065/release-sbom-manifest.json \
  --signing-manifest /tmp/sb-release-signatures-ncw066/release-bundle-signing-manifest.json \
  --triage-dir /tmp/sb-cve-triage-ncw067 \
  --repro-report /tmp/sb-repro-check-ncw068/repro-build-report.json \
  --compliance-bundle-dir /tmp/sb-compliance-bundle-ncw069
python3 scripts/release/run_integrated_gameday.py validate \
  --report-path /tmp/sb-integrated-gameday/integrated-gameday-report.json
```

---

## 🚀 Quick Start

```bash
# Generate all statistics
./scripts/generate-all-stats.sh

# View results
cat PROJECT_STATS.md
cat docs/DOCUMENTATION_COVERAGE.md
cat docs/specifications/beta_requirements/COMPLETION_STATUS.md
```

---

## 📦 Requirements

- Bash 4.0+
- Standard Unix tools (find, grep, wc, awk)
- **Optional:** cloc (for detailed code statistics)

Install cloc:
```bash
sudo apt install cloc      # Debian/Ubuntu
brew install cloc          # macOS
```

---

## 🤖 Automation

Run weekly via cron:
```bash
0 9 * * 1 cd /path/to/ScratchBird && ./scripts/generate-all-stats.sh
```

---

For detailed documentation, see the individual script files.
