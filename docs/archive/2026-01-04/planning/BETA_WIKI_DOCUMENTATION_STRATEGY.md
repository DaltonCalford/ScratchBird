# Beta Wiki Documentation Strategy

**Created:** 2026-01-03
**Purpose:** Strategy for maintaining wiki documentation during Beta with constantly changing codebase
**Status:** Planning

---

## Executive Summary

For a Beta with a constantly changing codebase, we need a documentation strategy that:
- **Stays synchronized** with code changes
- **Minimizes maintenance burden** on developers
- **Provides value** to early adopters and contributors
- **Scales** as the project matures
- **Leverages automation** where possible

**Recommendation:** Use a **hybrid approach** combining GitHub Wiki for user-facing docs with in-repo markdown for technical/developer docs.

---

## Option Analysis

### Option 1: GitHub Wiki (Separate Repository)

**Pros:**
- Easy to edit via web interface
- No pull request required for doc updates
- Accessible to non-developers
- Good for community contributions
- Searchable via GitHub
- Markdown-based

**Cons:**
- **Separate from code** - harder to keep in sync
- No code review process (can be good or bad)
- Not versioned with releases
- Can't easily test documentation changes
- Wiki changes don't trigger CI/CD
- Can become stale quickly

**Best For:**
- User guides and tutorials
- Getting started guides
- FAQ and troubleshooting
- Community-contributed content
- High-level conceptual docs

### Option 2: In-Repository Documentation (docs/ directory)

**Pros:**
- **Versioned with code** - docs and code stay in sync
- Pull request workflow - review before merge
- Can be tested (broken links, code samples)
- CI/CD integration possible
- Can be published to multiple targets (GitHub Pages, ReadTheDocs, etc.)
- Better for API documentation
- Supports versioning (docs for v1.0, v1.1, etc.)

**Cons:**
- Requires Git knowledge to contribute
- Pull request overhead for simple fixes
- Slower to update (PR review cycle)
- Less discoverable for casual users

**Best For:**
- API reference documentation
- Architecture and design docs
- Implementation specifications
- Developer guides
- Code examples and samples
- Release notes

### Option 3: Hybrid Approach (RECOMMENDED)

Use **both** approaches strategically:

**GitHub Wiki:**
- Getting Started Guide
- Installation Instructions
- User Tutorials
- FAQ
- Troubleshooting
- Community Resources
- Migration Guides (high-level)
- Glossary

**In-Repo docs/:**
- API Reference (auto-generated from code)
- Architecture Documentation
- Implementation Specifications
- Developer Guides
- Code Standards
- Testing Documentation
- Build Documentation
- Migration Technical Details

**Benefits:**
- Right tool for right audience
- Users get easy-to-edit wiki
- Developers get versioned technical docs
- Best of both worlds

---

## Recommended Hybrid Architecture

```
GitHub Repository Structure:
├── docs/                           # In-repo documentation (versioned)
│   ├── api/                        # API reference (auto-generated)
│   ├── architecture/               # System architecture
│   ├── development/                # Developer guides
│   ├── specifications/             # Technical specs (already exists)
│   ├── migration/                  # Technical migration guides
│   └── reference/                  # Reference materials
│
└── .github/
    └── workflows/
        └── docs-sync.yml           # Auto-sync to wiki

GitHub Wiki Structure (separate wiki repo):
├── Home.md                         # Landing page
├── Getting-Started.md              # Quick start
├── Installation/
│   ├── Linux.md
│   ├── Windows.md
│   ├── macOS.md
│   └── Docker.md
├── User-Guides/
│   ├── Basic-Queries.md
│   ├── Transactions.md
│   ├── Indexes.md
│   └── Security.md
├── Tutorials/
│   ├── First-Application.md
│   ├── Web-App-Integration.md
│   └── Data-Migration.md
├── Migration/
│   ├── From-PostgreSQL.md
│   ├── From-MySQL.md
│   ├── From-Firebird.md
│   └── Migration-Checklist.md
├── Drivers/
│   ├── Python.md
│   ├── Node.js.md
│   ├── Java.md
│   ├── C-Sharp.md
│   ├── Go.md
│   ├── PHP.md
│   └── Pascal-Delphi.md
├── FAQ.md
├── Troubleshooting.md
├── Community.md
└── Contributing.md
```

---

## Keeping Documentation Synchronized

### Strategy 1: Automated Sync (Partial)

Automatically sync certain stable docs from repo to wiki:

```yaml
# .github/workflows/docs-sync.yml
name: Sync Docs to Wiki

on:
  push:
    branches: [main]
    paths:
      - 'docs/user-guides/**'
      - 'docs/installation/**'

jobs:
  sync-to-wiki:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Checkout Wiki
        uses: actions/checkout@v4
        with:
          repository: ${{ github.repository }}.wiki
          path: wiki

      - name: Sync Getting Started
        run: |
          cp docs/user-guides/getting-started.md wiki/Getting-Started.md
          cp docs/installation/docker.md wiki/Installation-Docker.md

      - name: Commit to Wiki
        working-directory: wiki
        run: |
          git config user.name "GitHub Actions"
          git config user.email "actions@github.com"
          git add .
          git commit -m "Auto-sync from main repo" || echo "No changes"
          git push
```

### Strategy 2: Documentation Review in PRs

Make documentation updates **required** for code changes:

```yaml
# .github/workflows/doc-check.yml
name: Documentation Check

on: [pull_request]

jobs:
  check-docs:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 0

      - name: Check if API changed
        id: api-check
        run: |
          if git diff --name-only origin/main...HEAD | grep -q "include/"; then
            echo "api_changed=true" >> $GITHUB_OUTPUT
          fi

      - name: Check if docs updated
        if: steps.api-check.outputs.api_changed == 'true'
        run: |
          if ! git diff --name-only origin/main...HEAD | grep -q "docs/api/"; then
            echo "ERROR: API changed but docs/api/ not updated"
            exit 1
          fi
```

### Strategy 3: Documentation TODOs in Code

Use special TODO markers that get tracked:

```cpp
// In code:
/**
 * @brief Execute a query with parameters
 * @todo DOC: Update wiki with new parameter binding syntax
 */
void execute_query(const std::string& sql, const params& p);
```

Extract with script:

```bash
#!/bin/bash
# scripts/extract-doc-todos.sh
echo "# Documentation TODOs"
echo
grep -r "@todo DOC:" include/ src/ | sed 's/@todo DOC:/- [ ]/'
```

### Strategy 4: Version Banners

Add version warnings to wiki pages:

```markdown
<!-- At top of wiki page -->
> ⚠️ **Beta Documentation** - This page describes ScratchBird Beta 0.9.x.
> Some features may change before 1.0 release.
> Last verified: 2026-01-03 against commit [abc123]

> 📖 **For latest API details**, see [API Reference](https://scratchbird.dev/api/)
> (auto-generated from source code)
```

---

## Documentation Maintenance Workflow

### For New Features

1. **Developer writes code**
2. **Developer updates in-repo docs/** (required for PR merge)
3. **PR reviewer checks docs** as part of review
4. **After merge:** Developer updates wiki (or creates wiki PR)
5. **Weekly:** Doc lead reviews wiki for accuracy

### For Bug Fixes

1. **Developer fixes bug**
2. **If user-facing:** Update troubleshooting/FAQ in wiki
3. **If API change:** Update API reference in docs/

### For Beta Releases

1. **Freeze code** for release
2. **Review all docs** against current code
3. **Update version numbers** in wiki pages
4. **Create snapshot** of wiki (git tag in wiki repo)
5. **Publish release notes** with doc links
6. **Announce** doc updates to community

---

## Documentation Types and Ownership

| Documentation Type | Location | Owner | Update Frequency |
|-------------------|----------|-------|------------------|
| **Getting Started** | Wiki | Doc Lead | After each release |
| **Installation** | Wiki | DevOps | When install process changes |
| **API Reference** | docs/api/ (auto-gen) | Auto | Every commit |
| **Architecture** | docs/architecture/ | Architects | When design changes |
| **User Guides** | Wiki | Doc Lead | As features stabilize |
| **Tutorials** | Wiki | Community/Doc Lead | Monthly |
| **FAQ** | Wiki | Support Team | Weekly |
| **Troubleshooting** | Wiki | Support Team | Weekly |
| **Developer Guides** | docs/development/ | Senior Devs | When process changes |
| **Specifications** | docs/specifications/ | Feature Owners | When spec changes |
| **Migration Guides** | Wiki + docs/ | Migration Team | As drivers mature |
| **Release Notes** | GitHub Releases | Release Manager | Each release |

---

## Automation Tools and Scripts

### 1. Auto-Generate API Docs

Use Doxygen for C++ code:

```bash
# scripts/generate-api-docs.sh
#!/bin/bash
set -e

echo "Generating API documentation..."
doxygen Doxyfile

echo "Converting to markdown..."
# Use doxygen2md or similar tool
python3 scripts/doxygen-to-markdown.py

echo "Copying to docs/api/"
cp -r generated/markdown/* docs/api/

echo "API docs generated successfully"
```

Run in CI:

```yaml
# .github/workflows/api-docs.yml
name: Generate API Docs

on:
  push:
    branches: [main]
    paths:
      - 'include/**'
      - 'src/**'

jobs:
  generate:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install Doxygen
        run: sudo apt-get install -y doxygen
      - name: Generate docs
        run: ./scripts/generate-api-docs.sh
      - name: Commit changes
        run: |
          git config user.name "API Doc Bot"
          git config user.email "bot@scratchbird.dev"
          git add docs/api/
          git commit -m "Auto-update API docs" || echo "No changes"
          git push
```

### 2. Check for Broken Links

```yaml
# .github/workflows/link-check.yml
name: Check Documentation Links

on:
  schedule:
    - cron: '0 0 * * 0'  # Weekly
  workflow_dispatch:

jobs:
  check-links:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Check links in docs/
        uses: gaurav-nelson/github-action-markdown-link-check@v1
        with:
          use-quiet-mode: 'yes'
          folder-path: 'docs/'

      - name: Check wiki links
        run: |
          git clone https://github.com/${{ github.repository }}.wiki.git wiki
          cd wiki
          find . -name "*.md" -exec markdown-link-check {} \;
```

### 3. Documentation Coverage Report

```python
# scripts/doc-coverage.py
#!/usr/bin/env python3
"""
Check documentation coverage for public APIs
"""

import re
import sys
from pathlib import Path

def check_header_file(filepath):
    """Check if public functions have documentation"""
    with open(filepath) as f:
        content = f.read()

    # Find public function declarations
    functions = re.findall(r'^\s*\w+\s+(\w+)\s*\([^)]*\);', content, re.MULTILINE)

    # Check for Doxygen comments
    documented = re.findall(r'/\*\*.*?\*/\s*\w+\s+(\w+)\s*\(', content, re.DOTALL)

    return functions, documented

def main():
    include_dir = Path('include')
    total_functions = 0
    documented_functions = 0

    for header in include_dir.rglob('*.h'):
        if 'internal' in str(header):
            continue  # Skip internal headers

        funcs, docs = check_header_file(header)
        total_functions += len(funcs)
        documented_functions += len(docs)

        if funcs and len(docs) < len(funcs):
            print(f"⚠️  {header}: {len(docs)}/{len(funcs)} functions documented")

    coverage = (documented_functions / total_functions * 100) if total_functions > 0 else 0
    print(f"\n📊 Documentation Coverage: {coverage:.1f}% ({documented_functions}/{total_functions})")

    if coverage < 80:
        print("❌ Coverage below 80% threshold")
        sys.exit(1)
    else:
        print("✅ Coverage meets threshold")

if __name__ == '__main__':
    main()
```

### 4. Wiki Update Notifications

```yaml
# .github/workflows/wiki-notify.yml
name: Wiki Update Notification

on:
  gollum:  # Triggered on wiki edits

jobs:
  notify:
    runs-on: ubuntu-latest
    steps:
      - name: Send Discord notification
        env:
          DISCORD_WEBHOOK: ${{ secrets.DISCORD_WEBHOOK }}
        run: |
          curl -H "Content-Type: application/json" \
               -d "{\"content\": \"📝 Wiki updated: ${{ github.event.pages[0].title }} by ${{ github.actor }}\"}" \
               $DISCORD_WEBHOOK
```

---

## Documentation Quality Standards

### Markdown Style Guide

Create `docs/STYLE_GUIDE.md`:

```markdown
# Documentation Style Guide

## File Naming
- Use kebab-case: `getting-started.md`, `api-reference.md`
- Avoid special characters except hyphens
- Keep names descriptive but concise

## Headings
- Use ATX-style headings: `## Heading` not `Heading\n-------`
- One H1 per document (title)
- Don't skip heading levels

## Code Blocks
- Always specify language: ```python not ```
- Test all code examples
- Keep examples minimal but complete

## Links
- Use relative links for in-repo docs: `[API](../api/reference.md)`
- Use absolute URLs for external links
- Check links regularly (automated)

## Images
- Store in `docs/images/` directory
- Use descriptive filenames: `architecture-diagram.png`
- Include alt text: `![Architecture Diagram](images/arch.png)`
- Optimize images (< 500KB)

## Version Information
- Add version/date to top of doc:
  ```
  **Version:** Beta 0.9.x
  **Last Updated:** 2026-01-03
  ```

## Code Examples
- Must be runnable (test in CI)
- Include necessary imports/setup
- Show expected output
- Comment non-obvious code
```

### Documentation Templates

**Template: User Guide Page**

```markdown
# [Feature Name]

**Version:** Beta 0.9.x
**Last Updated:** 2026-01-03

## Overview

Brief description of the feature (1-2 sentences).

## When to Use

- Use case 1
- Use case 2
- Use case 3

## Prerequisites

- Requirement 1
- Requirement 2

## Quick Start

Minimal example to get started:

```sql
-- Example code
SELECT * FROM users;
```

## Detailed Usage

### Topic 1

Explanation and examples.

### Topic 2

Explanation and examples.

## Best Practices

- Tip 1
- Tip 2

## Common Issues

### Issue 1

**Symptom:** What the user sees
**Cause:** Why it happens
**Solution:** How to fix

## See Also

- [Related Feature 1](link)
- [Related Feature 2](link)
```

**Template: API Reference Page**

```markdown
# API: [Function/Class Name]

**Module:** `scratchbird.module`
**Since:** v0.9.0
**Stability:** Beta

## Signature

```python
def function_name(param1: type1, param2: type2) -> return_type:
    """Brief description."""
```

## Parameters

- `param1` (type1): Description
- `param2` (type2): Description

## Returns

Description of return value.

## Raises

- `ExceptionType1`: When this happens
- `ExceptionType2`: When that happens

## Example

```python
# Basic usage
result = function_name("value1", 42)
print(result)  # Output: ...
```

## Notes

Additional information, warnings, or tips.

## See Also

- [Related Function](link)
```

---

## Wiki-Specific Features

### Wiki Sidebar

Create `_Sidebar.md` in wiki:

```markdown
### Quick Links
- [Home](Home)
- [Getting Started](Getting-Started)
- [FAQ](FAQ)

### Installation
- [Docker](Installation-Docker)
- [Linux](Installation-Linux)
- [Windows](Installation-Windows)

### Drivers
- [Python](Driver-Python)
- [Node.js](Driver-NodeJS)
- [Java](Driver-Java)

### Help
- [Troubleshooting](Troubleshooting)
- [Community](Community)
```

### Wiki Footer

Create `_Footer.md`:

```markdown
📖 **Documentation Status:** Beta
🐛 **Found an error?** [Edit this page](link) or [report an issue](link)
💬 **Need help?** Join our [Discord](link) or [ask on Stack Overflow](link)
⭐ **Like ScratchBird?** [Star us on GitHub](link)
```

---

## Migration from Alpha to Beta Docs

Since you're transitioning from Alpha to Beta:

### Phase 1: Audit and Archive (Week 1)

1. **Audit existing docs** in `docs/` directory
2. **Archive Alpha-specific docs** to `docs/archive/alpha/`
3. **Identify what's still relevant** for Beta
4. **Create doc gap analysis** (what's missing?)

### Phase 2: Set Up Infrastructure (Week 1-2)

1. **Enable GitHub Wiki** on repository
2. **Set up wiki repository** (clone and initialize)
3. **Create initial structure** (Home, Getting Started, etc.)
4. **Set up CI/CD** for doc automation
5. **Create templates** for consistency

### Phase 3: Initial Content (Week 2-3)

1. **Write Beta Getting Started** guide
2. **Migrate stable content** from repo to wiki
3. **Create driver documentation** (one page per P0 driver)
4. **Write installation guides** (Docker, Linux, Windows)
5. **Create FAQ** from common issues

### Phase 4: Automation (Week 3-4)

1. **Set up API doc generation** (Doxygen)
2. **Configure link checking**
3. **Set up doc sync workflows**
4. **Create documentation coverage checks**
5. **Test automation**

### Phase 5: Community Enablement (Week 4+)

1. **Create CONTRIBUTING.md** for docs
2. **Enable wiki editing** for trusted contributors
3. **Set up documentation issues** in GitHub
4. **Create documentation roadmap**
5. **Announce documentation** to community

---

## Metrics and Success Criteria

Track documentation health:

```yaml
# Documentation Metrics
- API Coverage: >80% of public APIs documented
- Link Health: <5% broken links
- Freshness: <10% pages >60 days old without review
- Completeness: All P0 drivers have user guides
- Accessibility: Getting Started guide <5 minutes to first query
- Community: >10% of wiki edits from community (not core team)
```

Dashboard in `docs/metrics/`:

```markdown
# Documentation Health Dashboard

Last Updated: 2026-01-03

## Coverage
- ✅ API Documentation: 87% (target: >80%)
- ✅ Driver Guides: 7/7 P0 drivers (100%)
- ⚠️  Tutorial Coverage: 3/10 planned tutorials (30%)

## Quality
- ✅ Broken Links: 2 (target: <5)
- ⚠️  Pages >60 days old: 12 (target: <10%)
- ✅ Code Examples Tested: 45/47 (96%)

## Community
- ✅ Wiki Edits (Community): 15% (target: >10%)
- ✅ Documentation Issues: 3 open (target: <10)

## Action Items
- [ ] Update 12 stale pages
- [ ] Complete 7 remaining tutorials
- [ ] Fix 2 broken links
```

---

## Tools and Resources

### Recommended Tools

1. **Doxygen** - API doc generation from C++ code
2. **MkDocs** - Static site generator (if moving beyond wiki)
3. **markdown-link-check** - Broken link detection
4. **vale** - Prose linting (style guide enforcement)
5. **mermaid** - Diagrams in markdown
6. **carbon.now.sh** - Beautiful code screenshots
7. **asciinema** - Terminal session recordings

### Example Configuration

**Doxygen Configuration (`Doxyfile`):**
```
PROJECT_NAME           = "ScratchBird"
OUTPUT_DIRECTORY       = generated/docs
GENERATE_HTML          = YES
GENERATE_MARKDOWN      = YES
MARKDOWN_SUPPORT       = YES
EXTRACT_ALL            = NO
EXTRACT_PRIVATE        = NO
EXTRACT_STATIC         = NO
```

**Vale Configuration (`.vale.ini`):**
```ini
[*.md]
BasedOnStyles = Vale, Microsoft
MinAlertLevel = suggestion

[**/api/**]
BasedOnStyles = Vale
```

---

## Recommendations Summary

### For ScratchBird Beta

**Immediate Actions (Week 1):**
1. ✅ Enable GitHub Wiki
2. ✅ Create wiki structure from template above
3. ✅ Write Getting Started guide
4. ✅ Set up basic CI for API doc generation
5. ✅ Create documentation style guide

**Short Term (Month 1):**
1. ✅ Complete all P0 driver documentation
2. ✅ Set up automated sync workflows
3. ✅ Create installation guides
4. ✅ Build FAQ from common issues
5. ✅ Enable community contributions with guidelines

**Medium Term (Month 2-3):**
1. ✅ Complete tutorials for common use cases
2. ✅ Implement documentation coverage checks
3. ✅ Set up link checking automation
4. ✅ Create migration guides for all supported databases
5. ✅ Establish regular doc review cadence

**Best Practices:**
- ✅ Use hybrid approach (Wiki + In-Repo)
- ✅ Automate what you can
- ✅ Make docs part of PR review
- ✅ Version awareness (banner warnings)
- ✅ Community involvement with guard rails
- ✅ Metrics and continuous improvement

---

## Questions to Consider

Before implementing, decide:

1. **Who can edit the wiki?**
   - Anyone (public)?
   - Collaborators only?
   - Hybrid (public with moderation)?

2. **What's the review process?**
   - No review (wiki is quick)?
   - Post-publication review?
   - Required review for certain pages?

3. **How to handle versions?**
   - Separate wiki per release?
   - Version banners on pages?
   - Branch-based docs in repo?

4. **Who owns documentation?**
   - Technical writers?
   - Developers (docs with code)?
   - Community?
   - Mixed model?

5. **What's the source of truth?**
   - Wiki is canonical?
   - Code is canonical (docs generated)?
   - Hybrid based on doc type?

---

**Author:** Beta Planning Team
**Review Cycle:** Monthly during Beta
**Status:** Recommendations for Review
