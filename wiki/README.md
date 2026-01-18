# ScratchBird Wiki Documentation

**Purpose:** Source repository for ScratchBird's wiki documentation
**Sync Target:** GitHub Wiki at https://github.com/scratchbird/scratchbird.wiki
**Status:** Alpha documentation (in progress)
**Last Updated:** 2026-01-18

---

## Overview

This directory contains the **source of truth** for ScratchBird's wiki content.
It is version-controlled with the main repository and is intended for
user-facing documentation (installation, guides, CLI, dialect references).
Technical specifications and engine design details remain under `docs/`.

---

## Content Coverage (Current)

Total pages under `wiki/content`: **153**

| Area | Files | Notes |
| --- | --- | --- |
| Core root pages | 6 | Home, Getting Started, FAQ, Contributing, Sidebar, Footer |
| Getting Started (subdir) | 2 | first-connection, basic-sql |
| Installation | 10 | Docker, Linux, Windows, macOS, Kubernetes, package guides |
| Drivers | 9 | Python, Node.js/TS, Java, C#, Go, PHP, Pascal, ODBC, comparison |
| Migration | 5 | Overview, From Firebird/PG/MySQL, Checklist |
| Tutorials | 8 | First app + sample apps + README |
| User Guides | 11 | Transactions, Sequences, Indexes, Triggers, Procedures, Security, etc. |
| Reference | 7 | SQL syntax, types, context variables, operators, functions, etc. |
| Troubleshooting | 5 | Common, performance, connection, install, debug |
| Developer Guide | 10 | Architecture, storage, MGA, SBLR, parsers, security, testing |
| CLI Tools | 10 | sb_server, sb_isql, emulated clients, sb_security, sb_backup, etc. |
| Language Guides | 61 | Native + Firebird + PostgreSQL + MySQL (14 topics each) |
| Admin | 5 | Monitoring, backup, security, user management |
| Configuration | 2 | sb_server.conf, hba.conf |
| Connectivity | 1 | PostgreSQL client notes |
| Applications | 1 | WordPress guide |

---

## Directory Highlights

- `content/` holds all wiki pages (Markdown).
- `content/language-guides/` contains 4 full dialect guides (native, Firebird, PostgreSQL, MySQL).
- `content/developer-guide/` links major specs for architecture and engine behavior.
- `content/cli-tools/` documents server/client utilities and emulation clients.
- `images/` holds logos and diagrams used by the wiki.
- `scripts/` currently contains `sync-to-wiki.sh` for manual syncing.
- `templates/` provides authoring templates for user/driver docs.

See `DIRECTORY_STRUCTURE.md` for the full tree.

---

## Workflow (Typical)

1. Edit or add pages under `content/`.
2. Update the page's **Status** and **Last Updated** header.
3. Run a local link check (manual or scripted) before syncing.
4. Sync to the GitHub wiki (manual or via CI if configured).

Manual sync:

```bash
cd wiki
./scripts/sync-to-wiki.sh
```

---

## Content Standards (Current)

Use a minimal, consistent header at the top of each page:

```markdown
# Page Title

**Status:** Alpha documentation (in progress)
**Last Updated:** YYYY-MM-DD
```

Optional sections (use when helpful):
- **Overview**
- **Examples**
- **Known Limitations**
- **References**

---

## Automation and Templates

**Scripts** (current):
- `scripts/sync-to-wiki.sh` (manual sync)

**Templates** (current):
- `templates/user-guide-template.md`
- `templates/driver-guide-template.md`

---

## Logos and Branding

Primary header logo:

```markdown
![ScratchBird Logo](images/logos/TransparentScratchBirdLogoHeader.png)
```

Logo assets live in `images/logos/`. See `LOGO_USAGE.md` for guidelines.

---

## Related Files

- `DIRECTORY_STRUCTURE.md` - Full tree and coverage summary
- `SYNC_STATUS.md` - Sync readiness and coverage snapshot
- `SETUP_COMPLETE.md` - Historical setup notes
