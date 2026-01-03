# Wiki Directory Structure

**Created:** 2026-01-03
**Purpose:** Complete directory structure for ScratchBird wiki documentation
**Status:** Initial structure created, content pending

---

## Complete Structure

```
wiki/
├── README.md                           # Main documentation (this directory)
├── WIKI_MAINTENANCE.md                 # Maintenance procedures (to be created)
├── SYNC_STATUS.md                      # Sync status tracking
├── DIRECTORY_STRUCTURE.md              # This file
│
├── content/                            # Wiki page content (markdown)
│   ├── Home.md                         # Wiki home page ✅
│   ├── Getting-Started.md              # Quick start guide (to be created)
│   ├── FAQ.md                          # Frequently asked questions (to be created)
│   ├── Contributing.md                 # Contribution guide (to be created)
│   ├── _Sidebar.md                     # Wiki sidebar navigation ✅
│   ├── _Footer.md                      # Wiki footer ✅
│   │
│   ├── installation/                   # Installation guides (0/8 created)
│   │   ├── Docker.md
│   │   ├── Linux.md
│   │   ├── Windows.md
│   │   ├── macOS.md
│   │   ├── AppImage.md
│   │   ├── DEB-Package.md
│   │   ├── RPM-Package.md
│   │   └── Homebrew.md
│   │
│   ├── drivers/                        # Language driver guides (0/8 created)
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
│   ├── migration/                      # Migration guides (0/6 created)
│   │   ├── Migration-Overview.md
│   │   ├── From-PostgreSQL.md
│   │   ├── From-MySQL.md
│   │   ├── From-Firebird.md
│   │   ├── From-SQLite.md
│   │   ├── Migration-Checklist.md
│   │   └── Migration-Tools.md
│   │
│   ├── tutorials/                      # Step-by-step tutorials (0/7 created)
│   │   ├── First-Application.md
│   │   ├── Web-App-Python-Flask.md
│   │   ├── Web-App-NodeJS-Express.md
│   │   ├── Desktop-App-Delphi.md
│   │   ├── REST-API-Design.md
│   │   ├── Data-Migration-Project.md
│   │   └── Docker-Deployment.md
│   │
│   ├── user-guides/                    # Feature user guides (0/8 created)
│   │   ├── Basic-Queries.md
│   │   ├── Transactions.md
│   │   ├── Indexes.md
│   │   ├── Security.md
│   │   ├── Backup-Restore.md
│   │   ├── Performance-Tuning.md
│   │   ├── Monitoring.md
│   │   └── Vector-Search.md
│   │
│   ├── reference/                      # Reference documentation (0/7 created)
│   │   ├── SQL-Syntax.md
│   │   ├── Data-Types.md
│   │   ├── Functions.md
│   │   ├── Configuration.md
│   │   ├── Connection-Strings.md
│   │   ├── Error-Codes.md
│   │   └── Glossary.md
│   │
│   └── troubleshooting/                # Troubleshooting guides (0/5 created)
│       ├── Common-Errors.md
│       ├── Performance-Issues.md
│       ├── Connection-Problems.md
│       ├── Installation-Issues.md
│       └── Debug-Guide.md
│
├── templates/                          # Page templates for consistency
│   ├── user-guide-template.md         # Template for user guides ✅
│   ├── driver-guide-template.md       # Template for driver docs ✅
│   ├── tutorial-template.md           # Template for tutorials (to be created)
│   ├── migration-template.md          # Template for migration guides (to be created)
│   ├── troubleshooting-template.md    # Template for troubleshooting (to be created)
│   └── STYLE_GUIDE.md                 # Markdown style guide (to be created)
│
├── scripts/                            # Automation scripts
│   ├── sync-to-wiki.sh                # Sync content to GitHub wiki ✅
│   ├── check-links.sh                 # Check for broken links (to be created)
│   ├── validate-content.sh            # Validate markdown structure (to be created)
│   ├── generate-toc.py                # Generate table of contents (to be created)
│   ├── extract-code-samples.sh        # Extract and test code (to be created)
│   └── update-timestamps.sh           # Update timestamps (to be created)
│
├── images/                             # Wiki images and diagrams
│   ├── architecture/                  # Architecture diagrams
│   ├── screenshots/                   # UI screenshots
│   ├── diagrams/                      # General diagrams
│   └── logos/                         # Logos and branding
│
└── assets/                             # Additional assets
    ├── code-samples/                  # Tested code examples
    ├── data/                          # Sample data files
    └── configs/                       # Example configurations
```

---

## File Count Summary

| Category | Created | Pending | Total | Completion |
|----------|---------|---------|-------|------------|
| **Core Files** | 4 | 2 | 6 | 67% |
| **Templates** | 2 | 3 | 5 | 40% |
| **Scripts** | 1 | 5 | 6 | 17% |
| **Content: Installation** | 0 | 8 | 8 | 0% |
| **Content: Drivers** | 0 | 9 | 9 | 0% |
| **Content: Migration** | 0 | 6 | 6 | 0% |
| **Content: Tutorials** | 0 | 7 | 7 | 0% |
| **Content: User Guides** | 0 | 8 | 8 | 0% |
| **Content: Reference** | 0 | 7 | 7 | 0% |
| **Content: Troubleshooting** | 0 | 5 | 5 | 0% |
| **TOTAL** | **7** | **60** | **67** | **10%** |

---

## Created Files ✅

1. `wiki/README.md` - Main documentation for wiki directory
2. `wiki/SYNC_STATUS.md` - Sync status tracking
3. `wiki/content/Home.md` - Wiki home page
4. `wiki/content/_Sidebar.md` - Navigation sidebar
5. `wiki/content/_Footer.md` - Common footer
6. `wiki/templates/user-guide-template.md` - User guide template
7. `wiki/templates/driver-guide-template.md` - Driver documentation template
8. `wiki/scripts/sync-to-wiki.sh` - Sync automation script (executable)
9. `.github/workflows/wiki-sync.yml` - GitHub Actions workflow

---

## Priority Order for Content Creation

### Week 1: Foundation (Most Critical)
1. **Getting-Started.md** - Essential first doc
2. **installation/Docker.md** - Fastest installation method
3. **drivers/Python.md** - Most popular language
4. **migration/From-Firebird.md** - Strategic for MGA users
5. **FAQ.md** - Common questions

### Week 2: Core Documentation
6. **installation/Linux.md** - Primary development platform
7. **drivers/NodeJS-TypeScript.md** - Second most popular
8. **user-guides/Transactions.md** - Core MGA feature
9. **troubleshooting/Common-Errors.md** - Reduce support load
10. **tutorials/First-Application.md** - Learning resource

### Week 3: Expand Coverage
11. **drivers/Java-JDBC.md** - Enterprise users
12. **drivers/CSharp-DotNet.md** - Windows/enterprise
13. **migration/From-PostgreSQL.md** - Compatibility story
14. **user-guides/Indexes.md** - Performance
15. **reference/SQL-Syntax.md** - Reference documentation

### Week 4: Complete Foundation
16. **drivers/Go.md**, **PHP.md**, **Pascal-Delphi.md** - Complete P0 drivers
17. **installation/Windows.md**, **macOS.md** - Platform coverage
18. **user-guides/Security.md**, **Performance-Tuning.md** - Production readiness
19. **tutorials/** - Additional learning resources
20. **reference/** - Complete reference documentation

---

## Automation Setup

### GitHub Actions Workflow
Location: `.github/workflows/wiki-sync.yml`

**Triggers:**
- Automatic on push to `main` with `wiki/content/**` changes
- Manual workflow dispatch
- Optional: Daily scheduled sync

**Actions:**
- Syncs `wiki/content/` to GitHub wiki repository
- Syncs `wiki/images/` to wiki
- Updates timestamps
- Commits and pushes to wiki
- Sends notifications (if Discord webhook configured)

### Manual Sync
```bash
cd wiki/scripts
./sync-to-wiki.sh           # Normal sync
./sync-to-wiki.sh --dry-run # Preview changes
./sync-to-wiki.sh --force   # Force overwrite
```

---

## Next Steps

1. **Enable GitHub Wiki**
   - Go to repository Settings → Features → Enable Wikis
   - Initialize with placeholder page

2. **Create Priority Content**
   - Start with Getting Started guide
   - Use templates for consistency
   - Test all code examples

3. **Set Up Automation**
   - Verify GitHub Actions workflow works
   - Optional: Add Discord webhook for notifications
   - Test manual sync script

4. **Continuous Improvement**
   - Review analytics (most-viewed pages)
   - Update based on user feedback
   - Keep synchronized with code changes

---

**Status:** Infrastructure complete, content creation in progress
**Maintained by:** Documentation Team
**Last Updated:** 2026-01-03
