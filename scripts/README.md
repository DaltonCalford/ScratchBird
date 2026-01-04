# ScratchBird Project Statistics Scripts

**Purpose:** Automated generation of comprehensive project statistics and metrics  
**Last Updated:** 2026-01-03

---

## 📊 Available Scripts

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
