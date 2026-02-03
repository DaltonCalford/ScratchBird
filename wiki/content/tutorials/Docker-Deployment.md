# Docker Deployment

**Last Updated:** 2026-02-03

---

## Goal

Run ScratchBird in a container with persistent storage and configuration.

---

## Example Layout

- `./data` for database files
- `./sb_server.conf` for configuration

---

## Example Run

```bash
docker run -d   -p 3092:3092   -v $(pwd)/data:/var/lib/scratchbird   -v $(pwd)/sb_server.conf:/etc/scratchbird/sb_server.conf:ro   scratchbird/scratchbird:latest
```

---

## Common Adjustments

- Add ports for additional listeners
- Mount a TLS certificate directory
- Adjust `sb_server.conf` limits

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
