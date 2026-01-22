# Docker Container Specification

**Priority:** P0 (Critical - Beta Required)
**Status:** Draft
**Target Market:** DevOps, cloud-native deployments, development environments, CI/CD pipelines
**Use Cases:** Local development, testing, production deployments, microservices, cloud platforms

---

## Overview

Docker containerization is the de facto standard for modern application deployment. Official Docker images are essential for ScratchBird adoption in cloud-native environments, development workflows, and production deployments.

**Key Requirements:**
- Official Docker Hub images
- Multi-architecture support (amd64, arm64)
- Multiple base image variants (Alpine, Debian, Distroless)
- Security best practices (non-root user, minimal attack surface)
- Health checks and graceful shutdown
- Volume management for data persistence
- Docker Compose examples
- Environment variable configuration
- Automated builds and security scanning

---

## Specification Documents

### Required Documents

- [x] **[SPECIFICATION.md](SPECIFICATION.md)** - Detailed technical specification (draft)
  - Dockerfile design and multi-stage builds
  - Image variants and tagging strategy
  - Architecture support (amd64, arm64, arm/v7)
  - Security hardening (USER, capabilities, read-only filesystem)
  - Configuration via environment variables
  - Volume management and data persistence
  - Health check implementation
  - Signal handling for graceful shutdown
  - Logging configuration

- [ ] **IMPLEMENTATION_PLAN.md** - Development roadmap
  - Milestone 1: Basic Docker image (Alpine-based)
  - Milestone 2: Multi-architecture builds (buildx)
  - Milestone 3: Additional variants (Debian, Distroless)
  - Milestone 4: Docker Hub automation
  - Milestone 5: Security scanning and compliance
  - Milestone 6: Docker Compose stack examples
  - Resource requirements
  - Timeline estimates

- [ ] **TESTING_CRITERIA.md** - Test requirements
  - Image build tests (all variants, all architectures)
  - Container startup and health check tests
  - Data persistence tests (volumes)
  - Performance tests (vs bare-metal)
  - Security scan tests (Trivy, Snyk, Docker Scan)
  - Docker Compose integration tests
  - Upgrade and migration tests
  - Resource limit tests (memory, CPU)

- [ ] **COMPATIBILITY_MATRIX.md** - Version support
  - Docker versions (20.10+, 23+, 24+)
  - Docker Compose versions (v2)
  - Container runtimes (Docker, Podman, containerd)
  - Orchestrators (Kubernetes, Docker Swarm, Nomad)
  - Cloud platforms (AWS ECS/EKS, GCP GKE, Azure AKS, Fly.io)
  - Operating systems (Linux, macOS, Windows with WSL2)

- [ ] **MIGRATION_GUIDE.md** - Migration and deployment
  - Migration from bare-metal to Docker
  - Upgrading between image versions
  - Data migration and backups
  - High availability deployment
  - Rolling updates
  - Disaster recovery

- [ ] **API_REFERENCE.md** - Complete configuration reference
  - Environment variables
  - Volume mount points
  - Port mappings
  - Health check endpoints
  - Entrypoint and command options
  - Init process and signal handling

### Examples Directory

- [ ] **examples/basic/** - Basic single-container deployment
  - **docker-run.sh** - Simple docker run command
  - **Dockerfile** - Custom Dockerfile extending official image
- [ ] **examples/docker-compose/** - Docker Compose examples
  - **docker-compose.yml** - Basic setup
  - **docker-compose.prod.yml** - Production configuration
  - **docker-compose.dev.yml** - Development with hot reload
  - **.env.example** - Environment variables template
- [ ] **examples/multi-node/** - Multi-container setup
  - **docker-compose.yml** - Primary + replicas
  - **haproxy.cfg** - Load balancer configuration
- [ ] **examples/with-backup/** - With automated backups
  - **docker-compose.yml** - DB + backup sidecar
  - **backup.sh** - Backup script
- [ ] **examples/monitoring/** - With monitoring stack
  - **docker-compose.yml** - DB + Prometheus + Grafana
  - **prometheus.yml** - Prometheus configuration
  - **dashboards/** - Grafana dashboards
- [ ] **examples/development/** - Local development environment
  - **docker-compose.yml** - DB + seed data + tools
  - **init.sql** - Initial database schema
- [ ] **examples/ci-cd/** - CI/CD integration
  - **github-actions.yml** - GitHub Actions workflow
  - **gitlab-ci.yml** - GitLab CI configuration

---

## Key Integration Points

### Docker Hub Official Images

**Critical**: Official images on Docker Hub provide trust and discoverability.

Requirements:
- Docker Official Images program compliance
- Automated builds from GitHub
- Security scanning (Docker Scout, Trivy)
- Image signing (Docker Content Trust)
- Multi-architecture manifests
- Clear tagging strategy

Image naming and tags:
```
docker.io/scratchbird/scratchbird:latest
docker.io/scratchbird/scratchbird:1.0.0
docker.io/scratchbird/scratchbird:1.0
docker.io/scratchbird/scratchbird:1
docker.io/scratchbird/scratchbird:1.0.0-alpine
docker.io/scratchbird/scratchbird:1.0.0-debian
docker.io/scratchbird/scratchbird:1.0.0-distroless
```

### Multi-Architecture Support

**Critical**: Support for both AMD64 and ARM64 architectures.

Requirements:
- Docker Buildx for multi-arch builds
- Platform-specific optimizations
- QEMU emulation for cross-building
- Manifest lists for automatic platform selection

Supported platforms:
```
linux/amd64    - x86_64, most common
linux/arm64    - ARM64, Apple Silicon, AWS Graviton
linux/arm/v7   - ARMv7, Raspberry Pi (optional)
```

Example multi-arch build:
```bash
docker buildx create --use
docker buildx build \
  --platform linux/amd64,linux/arm64 \
  -t scratchbird/scratchbird:latest \
  --push \
  .
```

### Docker Compose Integration

**Important**: Docker Compose is the standard for local multi-container setups.

Example basic docker-compose.yml:
```yaml
version: '3.8'

services:
  scratchbird:
    image: scratchbird/scratchbird:latest
    container_name: scratchbird
    restart: unless-stopped
    ports:
      - "5432:5432"
    environment:
      SCRATCHBIRD_USER: ${DB_USER:-scratchbird}
      SCRATCHBIRD_PASSWORD: ${DB_PASSWORD:?Database password required}
      SCRATCHBIRD_DB: ${DB_NAME:-scratchbird}
    volumes:
      - scratchbird_data:/var/lib/scratchbird/data
      - ./init:/docker-entrypoint-initdb.d:ro
    healthcheck:
      test: ["CMD-SHELL", "scratchbird-healthcheck"]
      interval: 10s
      timeout: 5s
      retries: 5
      start_period: 30s
    networks:
      - scratchbird_network

volumes:
  scratchbird_data:
    driver: local

networks:
  scratchbird_network:
    driver: bridge
```

Example production docker-compose.yml:
```yaml
version: '3.8'

services:
  scratchbird-primary:
    image: scratchbird/scratchbird:1.0.0-alpine
    container_name: scratchbird-primary
    restart: always
    ports:
      - "5432:5432"
    environment:
      SCRATCHBIRD_USER: ${DB_USER}
      SCRATCHBIRD_PASSWORD_FILE: /run/secrets/db_password
      SCRATCHBIRD_DB: ${DB_NAME}
      SCRATCHBIRD_MAX_CONNECTIONS: 200
      SCRATCHBIRD_SHARED_BUFFERS: 2GB
      SCRATCHBIRD_EFFECTIVE_CACHE_SIZE: 6GB
      SCRATCHBIRD_REPLICATION_MODE: primary
    volumes:
      - scratchbird_data:/var/lib/scratchbird/data
      - ./postgresql.conf:/etc/scratchbird/scratchbird.conf:ro
      - ./logs:/var/log/scratchbird
    secrets:
      - db_password
    healthcheck:
      test: ["CMD-SHELL", "scratchbird-healthcheck"]
      interval: 10s
      timeout: 5s
      retries: 5
    deploy:
      resources:
        limits:
          cpus: '4'
          memory: 8G
        reservations:
          cpus: '2'
          memory: 4G
    networks:
      - backend
    logging:
      driver: "json-file"
      options:
        max-size: "10m"
        max-file: "3"

  scratchbird-replica:
    image: scratchbird/scratchbird:1.0.0-alpine
    container_name: scratchbird-replica
    restart: always
    ports:
      - "5434:5432"
    environment:
      SCRATCHBIRD_REPLICATION_MODE: replica
      SCRATCHBIRD_PRIMARY_HOST: scratchbird-primary
      SCRATCHBIRD_PRIMARY_PORT: 5432
      SCRATCHBIRD_REPLICATION_USER: replicator
      SCRATCHBIRD_REPLICATION_PASSWORD_FILE: /run/secrets/replication_password
    volumes:
      - scratchbird_replica_data:/var/lib/scratchbird/data
    secrets:
      - replication_password
    depends_on:
      scratchbird-primary:
        condition: service_healthy
    networks:
      - backend

volumes:
  scratchbird_data:
  scratchbird_replica_data:

secrets:
  db_password:
    file: ./secrets/db_password.txt
  replication_password:
    file: ./secrets/replication_password.txt

networks:
  backend:
    driver: bridge
```

### Environment Variable Configuration

Requirements:
- Comprehensive environment variable support
- Secrets via Docker secrets or files
- Sensible defaults
- Validation on startup

Common environment variables:
```bash
# Core configuration
SCRATCHBIRD_USER=scratchbird          # Database superuser name
SCRATCHBIRD_PASSWORD=secret           # Password (or use _FILE variant)
SCRATCHBIRD_PASSWORD_FILE=/run/secrets/db_password
SCRATCHBIRD_DB=scratchbird            # Default database name
SCRATCHBIRD_INITDB_ARGS=              # Arguments to initdb

# Connection settings
SCRATCHBIRD_PORT=5432                 # Listen port
SCRATCHBIRD_LISTEN_ADDRESSES=*        # Listen addresses
SCRATCHBIRD_MAX_CONNECTIONS=100       # Max client connections

# Memory settings
SCRATCHBIRD_SHARED_BUFFERS=128MB      # Shared memory buffers
SCRATCHBIRD_EFFECTIVE_CACHE_SIZE=4GB  # Planner cache size hint
SCRATCHBIRD_WORK_MEM=4MB              # Working memory per query
SCRATCHBIRD_MAINTENANCE_WORK_MEM=64MB # Maintenance operations memory

# Write-after log (WAL) and checkpointing (optional, post-gold)
# Note: MGA does not use write-after log (WAL) for recovery; these settings are reserved for future replication/PITR.
SCRATCHBIRD_WAL_LEVEL=replica         # write-after log (WAL) level (minimal, replica, logical)
SCRATCHBIRD_MAX_WAL_SIZE=1GB          # Maximum write-after log (WAL) size
SCRATCHBIRD_CHECKPOINT_TIMEOUT=5min   # Checkpoint timeout

# Logging
SCRATCHBIRD_LOG_DESTINATION=stderr    # Log destination
SCRATCHBIRD_LOG_STATEMENT=none        # Log statements (none, ddl, mod, all)
SCRATCHBIRD_LOG_MIN_DURATION=0        # Log slow queries (ms, 0=disabled)

# Replication
SCRATCHBIRD_REPLICATION_MODE=standalone  # standalone, primary, replica
SCRATCHBIRD_PRIMARY_HOST=             # Primary host (replica mode)
SCRATCHBIRD_PRIMARY_PORT=5432         # Primary port (replica mode)
SCRATCHBIRD_REPLICATION_USER=         # Replication user
SCRATCHBIRD_REPLICATION_PASSWORD=     # Replication password

# Custom configuration
SCRATCHBIRD_CONFIG_FILE=/etc/scratchbird/scratchbird.conf
SCRATCHBIRD_HBA_FILE=/etc/scratchbird/pg_hba.conf
```

### Volume Management

Requirements:
- Data volume for persistence
- Configuration volume (optional)
- Log volume (optional)
- Backup volume (optional)
- Proper permissions and ownership

Volume mount points:
```
/var/lib/scratchbird/data              # Main data directory (PGDATA)
/etc/scratchbird/                      # Configuration files
/var/log/scratchbird/                  # Log files
/docker-entrypoint-initdb.d/           # Initialization scripts (.sql, .sh)
/docker-entrypoint-preinitdb.d/        # Pre-initialization scripts
/run/secrets/                          # Docker secrets (read-only)
/backup/                               # Backup destination
```

Example docker run with volumes:
```bash
docker run -d \
  --name scratchbird \
  -p 5432:5432 \
  -e SCRATCHBIRD_PASSWORD=mypassword \
  -v scratchbird_data:/var/lib/scratchbird/data \
  -v $(pwd)/config:/etc/scratchbird:ro \
  -v $(pwd)/init:/docker-entrypoint-initdb.d:ro \
  -v $(pwd)/logs:/var/log/scratchbird \
  scratchbird/scratchbird:latest
```

---

## Dockerfile Design

### Alpine-based Image (Minimal Size)

```dockerfile
# syntax=docker/dockerfile:1
FROM alpine:3.19 AS builder

# Install build dependencies
RUN apk add --no-cache \
    gcc g++ cmake make \
    musl-dev linux-headers \
    openssl-dev zlib-dev \
    lz4-dev

# Build ScratchBird
WORKDIR /build
COPY . .
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build -j$(nproc) && \
    cmake --install build --prefix /usr/local

# Runtime image
FROM alpine:3.19

# Install runtime dependencies
RUN apk add --no-cache \
    libstdc++ libgcc \
    openssl zlib lz4-libs \
    tzdata \
    su-exec

# Create scratchbird user and group
RUN addgroup -g 1000 scratchbird && \
    adduser -u 1000 -G scratchbird -D -h /var/lib/scratchbird scratchbird

# Copy binaries from builder
COPY --from=builder /usr/local/bin/* /usr/local/bin/
COPY --from=builder /usr/local/lib/* /usr/local/lib/

# Create directories
RUN mkdir -p /var/lib/scratchbird/data \
             /var/log/scratchbird \
             /etc/scratchbird \
             /docker-entrypoint-initdb.d && \
    chown -R scratchbird:scratchbird \
        /var/lib/scratchbird \
        /var/log/scratchbird \
        /etc/scratchbird

# Copy entrypoint script
COPY docker-entrypoint.sh /usr/local/bin/
RUN chmod +x /usr/local/bin/docker-entrypoint.sh

# Environment variables
ENV SCRATCHBIRD_USER=scratchbird \
    SCRATCHBIRD_DB=scratchbird \
    PGDATA=/var/lib/scratchbird/data

# Expose port
EXPOSE 5432

# Volume for data
VOLUME ["/var/lib/scratchbird/data"]

# Health check
HEALTHCHECK --interval=10s --timeout=5s --start-period=30s --retries=3 \
    CMD scratchbird-healthcheck || exit 1

# Use dumb-init or tini for proper signal handling
RUN apk add --no-cache tini
ENTRYPOINT ["/sbin/tini", "--", "/usr/local/bin/docker-entrypoint.sh"]

# Default command
CMD ["scratchbird"]
```

### Debian-based Image (Broader Compatibility)

```dockerfile
# syntax=docker/dockerfile:1
FROM debian:bookworm-slim AS builder

RUN apt-get update && apt-get install -y \
    gcc g++ cmake make \
    libssl-dev zlib1g-dev liblz4-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY . .
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build -j$(nproc) && \
    cmake --install build --prefix /usr/local

# Runtime image
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y \
    libssl3 zlib1g liblz4-1 \
    gosu tini \
    && rm -rf /var/lib/apt/lists/*

RUN groupadd -r -g 1000 scratchbird && \
    useradd -r -u 1000 -g scratchbird -d /var/lib/scratchbird -s /bin/bash scratchbird

COPY --from=builder /usr/local/bin/* /usr/local/bin/
COPY --from=builder /usr/local/lib/* /usr/local/lib/

RUN mkdir -p /var/lib/scratchbird/data \
             /var/log/scratchbird \
             /etc/scratchbird \
             /docker-entrypoint-initdb.d && \
    chown -R scratchbird:scratchbird \
        /var/lib/scratchbird \
        /var/log/scratchbird \
        /etc/scratchbird

COPY docker-entrypoint.sh /usr/local/bin/
RUN chmod +x /usr/local/bin/docker-entrypoint.sh

ENV SCRATCHBIRD_USER=scratchbird \
    SCRATCHBIRD_DB=scratchbird \
    PGDATA=/var/lib/scratchbird/data

EXPOSE 5432
VOLUME ["/var/lib/scratchbird/data"]

HEALTHCHECK --interval=10s --timeout=5s --start-period=30s --retries=3 \
    CMD scratchbird-healthcheck || exit 1

ENTRYPOINT ["/usr/bin/tini", "--", "/usr/local/bin/docker-entrypoint.sh"]
CMD ["scratchbird"]
```

### Entrypoint Script

```bash
#!/usr/bin/env bash
set -Eeuo pipefail

# Docker entrypoint script for ScratchBird

# Logging function
log() {
    echo "[$(date +'%Y-%m-%d %H:%M:%S')] $*" >&2
}

# Initialize database if needed
if [ ! -s "$PGDATA/PG_VERSION" ]; then
    log "Initializing database in $PGDATA"

    # Use secrets file if available
    if [ -n "${SCRATCHBIRD_PASSWORD_FILE:-}" ] && [ -f "$SCRATCHBIRD_PASSWORD_FILE" ]; then
        SCRATCHBIRD_PASSWORD=$(cat "$SCRATCHBIRD_PASSWORD_FILE")
    fi

    # Initialize database
    gosu scratchbird initdb \
        -D "$PGDATA" \
        -U "$SCRATCHBIRD_USER" \
        --pwfile=<(echo "$SCRATCHBIRD_PASSWORD") \
        ${SCRATCHBIRD_INITDB_ARGS:-}

    log "Database initialized"

    # Run pre-init scripts
    if [ -d /docker-entrypoint-preinitdb.d ]; then
        for f in /docker-entrypoint-preinitdb.d/*; do
            case "$f" in
                *.sh)  log "Running $f"; . "$f" ;;
                *.sql) log "Running $f"; gosu scratchbird scratchbird -f "$f" ;;
                *)     log "Ignoring $f" ;;
            esac
        done
    fi

    # Start temporary server for initialization
    log "Starting temporary server for initialization"
    gosu scratchbird scratchbird -D "$PGDATA" &
    pid=$!

    # Wait for server to be ready
    for i in {1..30}; do
        if gosu scratchbird scratchbird-ready; then
            break
        fi
        sleep 1
    done

    # Create default database
    if [ "$SCRATCHBIRD_DB" != "postgres" ]; then
        log "Creating database: $SCRATCHBIRD_DB"
        gosu scratchbird createdb -U "$SCRATCHBIRD_USER" "$SCRATCHBIRD_DB"
    fi

    # Run init scripts
    if [ -d /docker-entrypoint-initdb.d ]; then
        for f in /docker-entrypoint-initdb.d/*; do
            case "$f" in
                *.sh)  log "Running $f"; . "$f" ;;
                *.sql) log "Running $f"; gosu scratchbird scratchbird -U "$SCRATCHBIRD_USER" -d "$SCRATCHBIRD_DB" -f "$f" ;;
                *)     log "Ignoring $f" ;;
            esac
        done
    fi

    # Stop temporary server
    log "Stopping temporary server"
    kill -TERM "$pid"
    wait "$pid"

    log "Initialization complete"
fi

# Start server
log "Starting ScratchBird"
exec gosu scratchbird scratchbird -D "$PGDATA" "$@"
```

---

## Performance Targets

Container overhead compared to bare-metal:

| Metric | Target |
|--------|--------|
| Startup time | <5 seconds |
| CPU overhead | <2% |
| Memory overhead | <50 MB |
| I/O throughput | >95% of bare-metal |
| Network latency | <1ms added latency |
| Query performance | >98% of bare-metal |

---

## Security Best Practices

1. **Non-root user**: Run as `scratchbird` user (UID 1000)
2. **Read-only root filesystem**: Where possible
3. **Minimal attack surface**: Alpine-based image <100 MB
4. **No unnecessary packages**: Only runtime dependencies
5. **Security scanning**: Trivy, Snyk, Docker Scout
6. **Image signing**: Docker Content Trust
7. **Secrets management**: Use Docker secrets or mounted files
8. **Network isolation**: Use Docker networks
9. **Resource limits**: CPU and memory limits

---

## Documentation Requirements

- [ ] Quick start guide
- [ ] Docker run command reference
- [ ] Docker Compose examples
- [ ] Environment variable reference
- [ ] Volume management guide
- [ ] Networking guide
- [ ] Security best practices
- [ ] Performance tuning
- [ ] Troubleshooting guide
- [ ] Upgrade and migration guide

---

## Release Criteria

### Functional Completeness

- [ ] Docker image builds successfully
- [ ] Multi-architecture support (amd64, arm64)
- [ ] All variants working (Alpine, Debian, Distroless)
- [ ] Health checks functional
- [ ] Graceful shutdown on SIGTERM
- [ ] Data persistence via volumes
- [ ] Init scripts execution
- [ ] Secrets support

### Quality Metrics

- [ ] Image size: Alpine <150 MB, Debian <250 MB
- [ ] Security scan: No HIGH or CRITICAL vulnerabilities
- [ ] Startup time: <5 seconds
- [ ] Performance: >98% of bare-metal
- [ ] Documentation complete

### Packaging

- [ ] Docker Hub official images
- [ ] GitHub Container Registry
- [ ] Multi-arch manifest lists
- [ ] Automated builds (GitHub Actions)
- [ ] Image signing enabled

---

**Document Version:** 1.0 (Draft)
**Last Updated:** 2026-01-03
**Status:** Draft
**Assigned To:** TBD
