# Repository Bootstrap Notes

Repository list is defined in `configs/repositories.yaml`.

## Fields

- `id`: stable identifier.
- `group`: logical group (`scratchbird`, `engines`).
- `url`: clone URL.
- `path`: target directory under clone root.

## Clone root

Default clone root: `./repos` (relative to verification workspace root).

Override clone root:

```bash
SB_VERIFY_REPO_ROOT=/opt/sb/repos ./.venv/bin/python scripts/bootstrap_clone_repos.py --config configs/repositories.yaml --preset core
```

or

```bash
./.venv/bin/python scripts/bootstrap_clone_repos.py --config configs/repositories.yaml --repo-root /opt/sb/repos --preset core
```

## Updating existing repositories

```bash
./.venv/bin/python scripts/bootstrap_clone_repos.py --config configs/repositories.yaml --preset core --update-existing
```
