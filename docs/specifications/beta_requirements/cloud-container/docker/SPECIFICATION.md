# Docker Container Specification

**Status:** Draft (Beta)
**Last Updated:** 2026-01-20

---

## 1) Purpose

Define the official ScratchBird Docker image requirements for development and
production deployments.

---

## 2) Scope

**In scope:**
- Official images on Docker Hub
- Multi-arch support (amd64/arm64)
- Development and production variants
- Health checks and graceful shutdown

**Out of scope:**
- Kubernetes manifests (see Kubernetes spec)
- Helm charts (see Helm spec)

---

## 3) Image Variants

- `scratchbird/scratchbird:latest` (production)
- `scratchbird/scratchbird:dev` (debug tools enabled)
- `scratchbird/scratchbird:<version>` (immutable release tags)

Base images:
- Debian slim (default)
- Distroless (optional)
- Alpine (dev-only, if supported)

---

## 4) Runtime Layout

- Config: `/etc/scratchbird/sb_server.conf`
- Data: `/var/lib/scratchbird`
- Logs: `/var/log/scratchbird`

Mounts:
- `-v /data:/var/lib/scratchbird`
- `-v /config:/etc/scratchbird`
- `-v /logs:/var/log/scratchbird`

---

## 5) Environment Variables

- `SCRATCHBIRD_CONFIG` (path override)
- `SCRATCHBIRD_LOG_LEVEL`
- `SCRATCHBIRD_DATA_DIR`
- `SCRATCHBIRD_LISTEN_ADDRESS`
- `SCRATCHBIRD_NATIVE_PORT` (default 3092)

---

## 6) Ports

- `3092/tcp` ScratchBird native protocol
- Optional protocol ports if enabled (pg/mysql/firebird)

---

## 7) Health Checks

- HTTP health endpoint (if enabled): `/health`
- Default check: `sb_admin show health`

---

## 8) Security

- Run as non-root user
- Drop capabilities not required
- Read-only root filesystem (production recommended)

---

## 9) Examples

### 9.1 Docker Run

```bash
docker run -d \
  --name scratchbird \
  -p 3092:3092 \
  -v /data/scratchbird:/var/lib/scratchbird \
  scratchbird/scratchbird:latest
```

### 9.2 Docker Compose

```yaml
services:
  scratchbird:
    image: scratchbird/scratchbird:latest
    ports:
      - "3092:3092"
    volumes:
      - ./data:/var/lib/scratchbird
      - ./config:/etc/scratchbird
```

---

## 10) Testing

- Smoke test container startup
- Health check verification
- Volume persistence
- Non-root enforcement

---

## 11) Related Documents

- `docs/specifications/beta_requirements/cloud-container/docker/README.md`
- `docs/specifications/deployment/SYSTEMD_SERVICE_SPECIFICATION.md`
- `docs/specifications/network/NETWORK_LISTENER_AND_PARSER_POOL_SPEC.md`
