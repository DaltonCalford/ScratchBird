# ScratchBird Wiki Documentation

**Purpose:** Source repository for ScratchBird's wiki documentation
**Sync Target:** GitHub Wiki at https://github.com/scratchbird/scratchbird.wiki
**Status:** Beta Documentation Structure
**Last Updated:** 2026-01-03

---

## Overview

This directory contains the **source of truth** for ScratchBird's wiki documentation. Content here is:
- **Version controlled** with the main codebase
- **Reviewed via pull requests** before publication
- **Automatically synced** to GitHub Wiki
- **Tested** for broken links and code examples

The wiki serves **user-facing documentation** (installation, guides, tutorials) while `docs/` contains **developer/technical documentation** (API reference, architecture, specifications).

---

## Directory Structure

```
wiki/
├── README.md                      # This file
├── WIKI_MAINTENANCE.md            # Maintenance procedures
├── SYNC_STATUS.md                 # Last sync status and schedule
│
├── content/                       # Wiki page content (markdown)
│   ├── Home.md                    # Wiki home page
│   ├── Getting-Started.md         # Quick start guide
│   ├── FAQ.md                     # Frequently asked questions
│   ├── Contributing.md            # How to contribute
│   ├── _Sidebar.md                # Wiki sidebar navigation
│   ├── _Footer.md                 # Wiki footer
│   │
│   ├── installation/              # Installation guides
│   │   ├── Docker.md
│   │   ├── Linux.md
│   │   ├── Windows.md
│   │   ├── macOS.md
│   │   ├── AppImage.md
│   │   ├── DEB-Package.md
│   │   ├── RPM-Package.md
│   │   └── Homebrew.md
│   │
│   ├── drivers/                   # Language driver guides
│   │   ├── Python.md
│   │   ├── NodeJS-TypeScript.md
│   │   ├── Java-JDBC.md
│   │   ├── CSharp-DotNet.md
│   │   ├── Go.md
│   │   ├── PHP.md
│   │   ├── Pascal-Delphi.md
│   │   ├── ODBC.md
│   │   └── Driver-Comparison.md
│   │
│   ├── migration/                 # Migration guides
│   │   ├── Migration-Overview.md
│   │   ├── From-PostgreSQL.md
│   │   ├── From-MySQL.md
│   │   ├── From-Firebird.md       # Critical for MGA users
│   │   ├── From-SQLite.md
│   │   ├── Migration-Checklist.md
│   │   └── Migration-Tools.md
│   │
│   ├── tutorials/                 # Step-by-step tutorials
│   │   ├── First-Application.md
│   │   ├── Web-App-Python-Flask.md
│   │   ├── Web-App-NodeJS-Express.md
│   │   ├── Desktop-App-Delphi.md
│   │   ├── REST-API-Design.md
│   │   ├── Data-Migration-Project.md
│   │   └── Docker-Deployment.md
│   │
│   ├── user-guides/               # Feature user guides
│   │   ├── Basic-Queries.md
│   │   ├── Transactions.md
│   │   ├── Indexes.md
│   │   ├── Security.md
│   │   ├── Backup-Restore.md
│   │   ├── Performance-Tuning.md
│   │   ├── Monitoring.md
│   │   └── Vector-Search.md
│   │
│   ├── reference/                 # Reference documentation
│   │   ├── SQL-Syntax.md
│   │   ├── Data-Types.md
│   │   ├── Functions.md
│   │   ├── Configuration.md
│   │   ├── Connection-Strings.md
│   │   ├── Error-Codes.md
│   │   └── Glossary.md
│   │
│   └── troubleshooting/           # Troubleshooting guides
│       ├── Common-Errors.md
│       ├── Performance-Issues.md
│       ├── Connection-Problems.md
│       ├── Installation-Issues.md
│       └── Debug-Guide.md
│
├── templates/                     # Page templates
│   ├── user-guide-template.md    # Template for user guides
│   ├── driver-guide-template.md  # Template for driver docs
│   ├── tutorial-template.md      # Template for tutorials
│   ├── migration-template.md     # Template for migration guides
│   └── troubleshooting-template.md
│
├── scripts/                       # Automation scripts
│   ├── sync-to-wiki.sh           # Sync content to GitHub wiki
│   ├── check-links.sh            # Check for broken links
│   ├── validate-content.sh       # Validate markdown and structure
│   ├── generate-toc.py           # Generate table of contents
│   ├── extract-code-samples.sh   # Extract and test code samples
│   └── update-timestamps.sh      # Update "last updated" timestamps
│
├── images/                        # Wiki images and diagrams
│   ├── architecture/              # Architecture diagrams
│   ├── screenshots/               # UI screenshots
│   ├── diagrams/                  # General diagrams
│   └── logos/                     # ScratchBird logos ✅
│       ├── TransparentScratchBirdLogoHeader.png  # Primary header logo
│       ├── ScratchBirdLogo.png    # Icon/avatar logo
│       ├── ScratchBirdLogoHeader.png  # Header with background
│       ├── LogoWork.svg           # Vector source
│       └── README.md              # Logo usage guide
│
└── assets/                        # Additional assets
    ├── code-samples/              # Tested code examples
    ├── data/                      # Sample data files
    └── configs/                   # Example configurations
```

---

## Workflow

### Creating New Wiki Content

1. **Create or edit** markdown file in appropriate `content/` subdirectory
2. **Use template** from `templates/` directory for consistency
3. **Add images** to `images/` with descriptive names
4. **Test code examples** (they must work!)
5. **Check links** locally: `./scripts/check-links.sh`
6. **Create pull request** for review
7. **After merge:** Content auto-syncs to GitHub Wiki (or manual sync)

### Updating Existing Content

1. **Edit** the markdown file in `content/`
2. **Update** "Last Updated" date at top of file
3. **Test changes** locally (preview markdown, test code)
4. **Create PR** with description of changes
5. **After merge:** Auto-sync to wiki

### Syncing to GitHub Wiki

**Automated (Recommended):**
GitHub Actions workflow runs on push to `main`:
- Syncs `content/` to wiki repository
- Preserves file structure
- Updates timestamps

**Manual:**
```bash
cd wiki
./scripts/sync-to-wiki.sh
```

---

## Logos and Branding

ScratchBird logos are available in `images/logos/`:

### Primary Logo
**TransparentScratchBirdLogoHeader.png** - Use this for wiki pages
```markdown
![ScratchBird Logo](images/logos/TransparentScratchBirdLogoHeader.png)
```

### Icon/Avatar
**ScratchBirdLogo.png** - Use for icons, avatars, small spaces

### Usage Guide
See [`LOGO_USAGE.md`](LOGO_USAGE.md) for complete guidelines and examples.

### Adding Logos to Pages
- ✅ Add to major pages (Home, Getting Started, installation guides)
- ✅ Add to tutorial headers
- ✅ Add to driver documentation
- ❌ Don't add to every page (avoid repetition)
- ❌ Don't use multiple times on same page

---

## File Naming Conventions

### Content Files
- **Use kebab-case:** `getting-started.md`, `from-postgresql.md`
- **Be descriptive:** `web-app-python-flask.md` not `tutorial-1.md`
- **Match wiki URLs:** `Getting-Started.md` → `wiki/Getting-Started`
- **No spaces:** Use hyphens instead

### Images
- **Descriptive names:** `architecture-overview.png` not `img1.png`
- **Include context:** `driver-python-connection-example.png`
- **Optimize size:** < 500KB per image
- **Use PNG** for screenshots, **SVG** for diagrams

### Code Samples
- **Language suffix:** `example.py`, `example.js`, `example.go`
- **Descriptive:** `flask-basic-query.py` not `example1.py`

---

## Content Standards

### Every Page Must Have

```markdown
# Page Title

**Version:** Beta 0.9.x
**Last Updated:** YYYY-MM-DD
**Difficulty:** Beginner | Intermediate | Advanced

[Brief description of what this page covers]

## Table of Contents
[Auto-generated or manual TOC]

[Content...]

## See Also
- [Related Page 1](link)
- [Related Page 2](link)

---
**Questions?** Ask on [Discord](#) or [Stack Overflow](#)
**Found an error?** [Edit this page](#) or [report an issue](#)
```

### Version Banners

For Beta documentation, include at top:

```markdown
> ⚠️ **Beta Documentation**
> This page describes ScratchBird Beta 0.9.x. Features may change before 1.0 release.
> Last verified: 2026-01-03 against commit [abc123]
```

### Code Examples

All code must:
- **Be tested** and working
- **Include comments** explaining non-obvious parts
- **Show output** where appropriate
- **Be minimal** but complete (runnable)

Example:
```python
# Connect to ScratchBird using Python
import scratchbird

# Connection string format
conn = scratchbird.connect(
    host='localhost',
    port=5432,
    database='mydb',
    user='myuser',
    password='mypass'
)

# Execute a simple query
cursor = conn.cursor()
cursor.execute('SELECT version()')
version = cursor.fetchone()
print(f"Connected to: {version[0]}")  # Output: ScratchBird 0.9.0

conn.close()
```

---

## Templates

Templates ensure consistency. Use them as starting points:

### User Guide Template
`templates/user-guide-template.md` - For feature documentation

### Driver Guide Template
`templates/driver-guide-template.md` - For language driver docs

### Tutorial Template
`templates/tutorial-template.md` - For step-by-step tutorials

### Migration Guide Template
`templates/migration-template.md` - For database migration guides

See each template for detailed structure.

---

## Automation Scripts

### sync-to-wiki.sh
Syncs content to GitHub wiki repository.

```bash
# Usage
./scripts/sync-to-wiki.sh

# Options
./scripts/sync-to-wiki.sh --dry-run  # Preview changes
./scripts/sync-to-wiki.sh --force    # Force overwrite
```

### check-links.sh
Validates all links in wiki content.

```bash
# Check all content
./scripts/check-links.sh

# Check specific file
./scripts/check-links.sh content/drivers/Python.md
```

### validate-content.sh
Validates markdown syntax and structure.

```bash
# Validate all
./scripts/validate-content.sh

# Validate specific directory
./scripts/validate-content.sh content/tutorials/
```

### generate-toc.py
Auto-generates table of contents from headings.

```bash
# Generate TOC for file
./scripts/generate-toc.py content/user-guides/Transactions.md
```

---

## GitHub Wiki Integration

### Wiki Sidebar (`_Sidebar.md`)

Automatically generated navigation sidebar for GitHub wiki.

### Wiki Footer (`_Footer.md`)

Common footer with links and help information.

### Wiki Home Page

Entry point for wiki, provides overview and navigation.

---

## Maintenance

### Weekly Tasks
- [ ] Review analytics (which pages viewed most)
- [ ] Check for broken links (automated)
- [ ] Review open documentation issues
- [ ] Update FAQ with new questions

### Monthly Tasks
- [ ] Review all pages for accuracy against current codebase
- [ ] Update screenshots if UI changed
- [ ] Review and merge community contributions
- [ ] Update "Last Updated" dates for verified pages

### Before Each Release
- [ ] Verify all documentation against release candidate
- [ ] Update version numbers throughout wiki
- [ ] Update Getting Started guide
- [ ] Update driver compatibility matrices
- [ ] Snapshot wiki state (git tag)

See `WIKI_MAINTENANCE.md` for detailed procedures.

---

## Contributing

### For Team Members

1. Create branch: `docs/wiki-feature-name`
2. Make changes in `content/`
3. Test locally (links, code examples)
4. Create PR with description
5. Address review feedback
6. After merge: auto-syncs to wiki

### For Community Contributors

Community members can:
- **Edit directly** on GitHub wiki (for small fixes)
- **Create issues** for documentation bugs/improvements
- **Submit PRs** to this repo for larger changes

All wiki edits are reviewed and synced back to this repo weekly.

---

## Quality Checklist

Before submitting documentation:

- [ ] Content follows template structure
- [ ] "Last Updated" date is current
- [ ] All code examples tested and working
- [ ] All links checked and valid
- [ ] Images optimized (< 500KB)
- [ ] Spelling and grammar checked
- [ ] Appropriate difficulty level indicated
- [ ] "See Also" section includes relevant links
- [ ] Version banner present (if Beta content)

---

## Status Dashboard

Current wiki statistics:

```markdown
Last Sync: [Auto-updated by CI]
Total Pages: [Auto-updated]
Broken Links: [Auto-updated]
Coverage:
  - Installation: [%]
  - Drivers: [%]
  - Migration: [%]
  - Tutorials: [%]
  - User Guides: [%]
```

See `SYNC_STATUS.md` for real-time status.

---

## Getting Help

### For Documentation Questions
- **Discord:** #documentation channel
- **Issues:** Label with `documentation`
- **Email:** docs@scratchbird.dev

### For Technical Questions
- **Discord:** #help channel
- **Stack Overflow:** Tag `scratchbird`
- **GitHub Issues:** For bugs/features

---

## Additional Resources

- [Documentation Strategy](/docs/planning/BETA_WIKI_DOCUMENTATION_STRATEGY.md)
- [Style Guide](templates/STYLE_GUIDE.md)
- [API Documentation](/docs/api/) (auto-generated)
- [Architecture Docs](/docs/architecture/)
- [GitHub Wiki](https://github.com/scratchbird/scratchbird/wiki)

---

**Maintainers:** Documentation Team
**Review Cycle:** Weekly during Beta
**Feedback:** #documentation on Discord
