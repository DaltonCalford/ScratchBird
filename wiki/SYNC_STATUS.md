# Wiki Sync Status

**Last Sync:** Not yet synced
**Sync Method:** Manual / GitHub Actions
**Status:** ✅ Ready | ⏳ In Progress | ❌ Failed

---

## Sync Information

```
Last Successful Sync: Never
Source Commit: N/A
Synced By: N/A
Files Synced: 0
Sync Duration: N/A
```

---

## Sync History

| Date | Time | Commit | Status | Files Changed | Notes |
|------|------|--------|--------|---------------|-------|
| - | - | - | - | - | Initial setup |

---

## Manual Sync

To manually sync wiki content:

```bash
cd wiki/scripts
./sync-to-wiki.sh

# Or dry-run to preview
./sync-to-wiki.sh --dry-run
```

---

## Automated Sync

GitHub Actions workflow automatically syncs wiki on:
- Push to `main` branch with changes in `wiki/content/`
- Manual workflow dispatch
- Scheduled daily sync (if configured)

Workflow file: `.github/workflows/wiki-sync.yml`

---

## Sync Coverage

| Category | Files | Status |
|----------|-------|--------|
| Installation Guides | 0/8 | 🚧 Pending |
| Driver Documentation | 0/7 | 🚧 Pending |
| Migration Guides | 0/4 | 🚧 Pending |
| User Guides | 0/7 | 🚧 Pending |
| Tutorials | 0/5 | 🚧 Pending |
| Reference Docs | 0/6 | 🚧 Pending |
| Troubleshooting | 0/4 | 🚧 Pending |
| **Total** | **0/41** | **0% Complete** |

---

## Next Sync Tasks

- [ ] Create initial Getting Started guide
- [ ] Complete Docker installation guide
- [ ] Document Python driver
- [ ] Create Firebird migration guide
- [ ] Add troubleshooting common errors
- [ ] Sync all changes to GitHub wiki

---

**Auto-updated by:** sync-to-wiki.sh script
**Last Updated:** 2026-01-03
