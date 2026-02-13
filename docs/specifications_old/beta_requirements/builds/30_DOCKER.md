# Docker Container Build Requirements

**Technology:** Docker
**Target Platforms:** Linux (multi-architecture)
**Document Version:** 1.0
**Last Updated:** 2026-01-03

---

## 1. Overview

This document specifies all requirements for building and deploying ScratchBird as Docker containers. Covers single-platform and multi-platform builds, optimization strategies, and production deployment.

---

## 2. System Requirements

### 2.1 Host System Requirements

| Component | Requirement |
|-----------|-------------|
| **OS** | Linux, macOS, or Windows with WSL2 |
| **Docker Version** | 20.10.0 or later |
| **Docker Compose** | 2.0.0 or later (optional) |
| **RAM** | 8 GB minimum (16 GB recommended) |
| **Disk Space** | 20 GB free (images + build cache) |

### 2.2 Supported Host Platforms

**Tier 1 (Fully Supported)**:
- Linux (Ubuntu 22.04+, Debian 12+, Fedora 38+)
- macOS (Docker Desktop)
- Windows 11 (Docker Desktop with WSL2)

**Tier 2 (Community Supported)**:
- Windows Server with containers
- Other Linux distributions

---

## 3. Docker Installation

### 3.1 Linux (Ubuntu/Debian)

```bash
# Install Docker Engine
sudo apt update
sudo apt install -y ca-certificates curl gnupg

# Add Docker's official GPG key
sudo install -m 0755 -d /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
sudo chmod a+r /etc/apt/keyrings/docker.gpg

# Set up repository
echo \
  "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu \
  $(. /etc/os-release && echo "$VERSION_CODENAME") stable" | \
  sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

# Install Docker
sudo apt update
sudo apt install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin

# Add user to docker group (logout/login required)
sudo usermod -aG docker $USER
```

### 3.2 Linux (Fedora/RHEL)

```bash
# Install Docker Engine
sudo dnf -y install dnf-plugins-core
sudo dnf config-manager --add-repo https://download.docker.com/linux/fedora/docker-ce.repo

sudo dnf install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin

# Start Docker
sudo systemctl start docker
sudo systemctl enable docker

# Add user to docker group
sudo usermod -aG docker $USER
```

### 3.3 macOS / Windows

**Download Docker Desktop**:
- macOS: https://docs.docker.com/desktop/install/mac-install/
- Windows: https://docs.docker.com/desktop/install/windows-install/

**System Requirements**:
- macOS: macOS 11 or later
- Windows: Windows 10/11 with WSL2 enabled

---

## 4. Verification

### 4.1 Verify Docker Installation

```bash
# Check Docker version
docker --version  # Should show 20.10.0+

# Check Docker is running
docker ps

# Test Docker
docker run hello-world

# Check BuildKit support
docker buildx version
```

### 4.2 Enable BuildKit

```bash
# Set BuildKit as default builder (recommended)
export DOCKER_BUILDKIT=1

# Or create buildx builder
docker buildx create --name scratchbird-builder --use
docker buildx inspect --bootstrap
```

---

## 5. Base Image Selection

### 5.1 Recommended Base Images

**For Production (Minimal Size)**:
- **Alpine Linux 3.19+**: ~5 MB base, musl libc
- **Debian Slim (Bookworm)**: ~70 MB base, glibc
- **Ubuntu Minimal (22.04)**: ~30 MB base, glibc

**For Development (Full Tools)**:
- **Ubuntu 22.04**: ~80 MB base, full package ecosystem
- **Debian 12 (Bookworm)**: ~120 MB base

**Comparison**:
| Base Image | Size | Libc | Package Manager | Best For |
|------------|------|------|-----------------|----------|
| Alpine 3.19 | ~5 MB | musl | apk | Production (smallest) |
| Debian Slim | ~70 MB | glibc | apt | Production (compatible) |
| Ubuntu Minimal | ~30 MB | glibc | apt | Production (middle ground) |
| Ubuntu 22.04 | ~80 MB | glibc | apt | Development |

---

## 6. Dockerfile Strategies

### 6.1 Multi-Stage Build (Recommended)

**Advantages**:
- Small final image (only runtime dependencies)
- Build dependencies not included in final image
- Secure (no build tools in production)

**Dockerfile.multistage**:
```dockerfile
# syntax=docker/dockerfile:1

# ============================================
# Stage 1: Build Environment
# ============================================
FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    git \
    libspdlog-dev \
    libgtest-dev \
    libssl-dev \
    liblz4-dev \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /src

# Copy source code
COPY . .

# Build ScratchBird
RUN cmake -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/opt/scratchbird \
    -DBUILD_SHARED_LIBS=OFF \
    -B build && \
    ninja -C build && \
    ninja -C build install

# ============================================
# Stage 2: Runtime Environment
# ============================================
FROM ubuntu:22.04 AS runtime

# Install runtime dependencies only
RUN apt-get update && apt-get install -y --no-install-recommends \
    libssl3 \
    liblz4-1 \
    zlib1g \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user
RUN useradd -r -u 1000 -m -s /bin/bash scratchbird

# Copy built binaries from builder stage
COPY --from=builder /opt/scratchbird /opt/scratchbird

# Set up runtime environment
ENV PATH="/opt/scratchbird/bin:${PATH}"
WORKDIR /data
RUN chown scratchbird:scratchbird /data

# Switch to non-root user
USER scratchbird

# Expose default port (adjust as needed)
EXPOSE 5432

# Health check
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD scratchbird --health || exit 1

# Entry point
ENTRYPOINT ["/opt/scratchbird/bin/scratchbird"]
CMD ["--help"]
```

### 6.2 Alpine-Based Build (Smallest Size)

**Dockerfile.alpine**:
```dockerfile
# syntax=docker/dockerfile:1

# ============================================
# Stage 1: Build Environment
# ============================================
FROM alpine:3.19 AS builder

# Install build dependencies
RUN apk add --no-cache \
    build-base \
    cmake \
    ninja \
    git \
    spdlog-dev \
    gtest-dev \
    openssl-dev \
    lz4-dev \
    zlib-dev

WORKDIR /src
COPY . .

# Build with musl libc
RUN cmake -G Ninja \
    -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DCMAKE_INSTALL_PREFIX=/opt/scratchbird \
    -DBUILD_SHARED_LIBS=OFF \
    -B build && \
    ninja -C build && \
    ninja -C build install && \
    strip /opt/scratchbird/bin/*

# ============================================
# Stage 2: Runtime Environment
# ============================================
FROM alpine:3.19 AS runtime

# Install runtime dependencies
RUN apk add --no-cache \
    libssl3 \
    lz4-libs \
    zlib \
    ca-certificates

# Create non-root user
RUN adduser -D -u 1000 -s /bin/sh scratchbird

# Copy binaries
COPY --from=builder /opt/scratchbird /opt/scratchbird

ENV PATH="/opt/scratchbird/bin:${PATH}"
WORKDIR /data
RUN chown scratchbird:scratchbird /data

USER scratchbird
EXPOSE 5432

HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD scratchbird --health || exit 1

ENTRYPOINT ["/opt/scratchbird/bin/scratchbird"]
CMD ["--help"]
```

### 6.3 Distroless Build (Security-Focused)

**Dockerfile.distroless**:
```dockerfile
# syntax=docker/dockerfile:1

FROM ubuntu:22.04 AS builder

# Build stage (same as multi-stage example)
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build git \
    libspdlog-dev libgtest-dev libssl-dev liblz4-dev zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/opt/scratchbird \
    -DBUILD_SHARED_LIBS=OFF -B build && \
    ninja -C build && ninja -C build install

# ============================================
# Stage 2: Distroless Runtime
# ============================================
FROM gcr.io/distroless/base-debian12

# Copy binaries and libraries
COPY --from=builder /opt/scratchbird /opt/scratchbird
COPY --from=builder /usr/lib/x86_64-linux-gnu/libssl.so.3 /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libcrypto.so.3 /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/liblz4.so.1 /usr/lib/x86_64-linux-gnu/
COPY --from=builder /lib/x86_64-linux-gnu/libz.so.1 /lib/x86_64-linux-gnu/

ENV PATH="/opt/scratchbird/bin:${PATH}"
WORKDIR /data

EXPOSE 5432
ENTRYPOINT ["/opt/scratchbird/bin/scratchbird"]
CMD ["--help"]
```

---

## 7. Building Images

### 7.1 Basic Build

```bash
# Build image
docker build -t scratchbird:latest .

# Build with specific Dockerfile
docker build -f Dockerfile.alpine -t scratchbird:alpine .

# Build with build arguments
docker build --build-arg CMAKE_BUILD_TYPE=Release -t scratchbird:latest .
```

### 7.2 Multi-Architecture Build

**Build for multiple architectures**:
```bash
# Create multi-platform builder
docker buildx create --name multiarch --use

# Build for x86_64 and ARM64
docker buildx build \
  --platform linux/amd64,linux/arm64 \
  -t scratchbird:latest \
  --push \
  .

# Build and load for local testing (single platform)
docker buildx build \
  --platform linux/amd64 \
  -t scratchbird:latest \
  --load \
  .
```

### 7.3 BuildKit Optimizations

```bash
# Use BuildKit cache mounts
docker buildx build \
  --cache-from=type=registry,ref=scratchbird:buildcache \
  --cache-to=type=registry,ref=scratchbird:buildcache,mode=max \
  -t scratchbird:latest \
  .
```

**Dockerfile with cache mounts**:
```dockerfile
# syntax=docker/dockerfile:1

FROM ubuntu:22.04 AS builder

# Install dependencies with cache mount
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    apt-get update && apt-get install -y \
    build-essential cmake ninja-build

# Build with cache mount for CMake build directory
RUN --mount=type=cache,target=/src/build/.cache \
    cmake -G Ninja -B build && ninja -C build
```

---

## 8. Running Containers

### 8.1 Basic Run

```bash
# Run interactively
docker run -it --rm scratchbird:latest

# Run as daemon
docker run -d --name scratchbird-server \
  -p 5432:5432 \
  -v scratchbird-data:/data \
  scratchbird:latest server

# Run with environment variables
docker run -d \
  -e SCRATCHBIRD_PORT=5432 \
  -e SCRATCHBIRD_MAX_CONNECTIONS=100 \
  scratchbird:latest
```

### 8.2 Volume Management

```bash
# Create named volume
docker volume create scratchbird-data

# Run with volume
docker run -d \
  -v scratchbird-data:/data \
  scratchbird:latest

# Run with bind mount
docker run -d \
  -v $(pwd)/data:/data \
  scratchbird:latest
```

### 8.3 Networking

```bash
# Create custom network
docker network create scratchbird-net

# Run container on custom network
docker run -d --name scratchbird-server \
  --network scratchbird-net \
  scratchbird:latest

# Connect to container from another container
docker run -it --rm \
  --network scratchbird-net \
  scratchbird:latest client connect scratchbird-server
```

---

## 9. Docker Compose

### 9.1 Basic Compose File

**docker-compose.yml**:
```yaml
version: '3.8'

services:
  scratchbird:
    build:
      context: .
      dockerfile: Dockerfile.multistage
    image: scratchbird:latest
    container_name: scratchbird-server
    ports:
      - "5432:5432"
    volumes:
      - scratchbird-data:/data
      - ./config:/config:ro
    environment:
      - SCRATCHBIRD_PORT=5432
      - SCRATCHBIRD_MAX_CONNECTIONS=100
    restart: unless-stopped
    healthcheck:
      test: ["CMD", "scratchbird", "--health"]
      interval: 30s
      timeout: 3s
      retries: 3
      start_period: 5s

volumes:
  scratchbird-data:
    driver: local
```

### 9.2 Production Compose (with Monitoring)

**docker-compose.prod.yml**:
```yaml
version: '3.8'

services:
  scratchbird:
    image: scratchbird:latest
    container_name: scratchbird-server
    ports:
      - "5432:5432"
    volumes:
      - scratchbird-data:/data
      - ./config:/config:ro
    environment:
      - SCRATCHBIRD_PORT=5432
      - SCRATCHBIRD_METRICS_ENABLED=true
      - SCRATCHBIRD_METRICS_PORT=9090
    restart: unless-stopped
    networks:
      - scratchbird-net
    healthcheck:
      test: ["CMD", "scratchbird", "--health"]
      interval: 30s
      timeout: 3s
      retries: 3

  prometheus:
    image: prom/prometheus:latest
    container_name: prometheus
    ports:
      - "9091:9090"
    volumes:
      - ./prometheus.yml:/etc/prometheus/prometheus.yml:ro
      - prometheus-data:/prometheus
    command:
      - '--config.file=/etc/prometheus/prometheus.yml'
      - '--storage.tsdb.path=/prometheus'
    networks:
      - scratchbird-net
    restart: unless-stopped

  grafana:
    image: grafana/grafana:latest
    container_name: grafana
    ports:
      - "3000:3000"
    volumes:
      - grafana-data:/var/lib/grafana
    environment:
      - GF_SECURITY_ADMIN_PASSWORD=admin
    networks:
      - scratchbird-net
    restart: unless-stopped

volumes:
  scratchbird-data:
  prometheus-data:
  grafana-data:

networks:
  scratchbird-net:
    driver: bridge
```

### 9.3 Running with Compose

```bash
# Start services
docker compose up -d

# View logs
docker compose logs -f scratchbird

# Stop services
docker compose down

# Stop and remove volumes
docker compose down -v

# Rebuild and restart
docker compose up -d --build
```

---

## 10. Image Optimization

### 10.1 Reduce Image Size

**Techniques**:
1. Multi-stage builds
2. Minimal base images (Alpine, Distroless)
3. Static linking
4. Strip binaries
5. Remove unnecessary files

**Example size comparison**:
```bash
# Ubuntu base: ~300 MB
# Alpine base: ~50 MB
# Distroless: ~40 MB
```

### 10.2 Layer Optimization

**Best Practices**:
```dockerfile
# ❌ BAD: Creates many layers
RUN apt-get update
RUN apt-get install -y cmake
RUN apt-get install -y ninja-build
RUN apt-get install -y git

# ✅ GOOD: Single layer, cleanup
RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake \
    ninja-build \
    git \
    && rm -rf /var/lib/apt/lists/*
```

### 10.3 .dockerignore

**Create `.dockerignore`**:
```
# Git
.git
.gitignore

# Build artifacts
build/
*.o
*.a

# Documentation
docs/
*.md
!README.md

# IDE
.vscode/
.idea/
*.swp

# Tests (if not needed)
tests/

# CI/CD
.github/
.gitlab-ci.yml
```

---

## 11. Security Best Practices

### 11.1 Non-Root User

```dockerfile
# ✅ GOOD: Run as non-root
RUN useradd -r -u 1000 -m scratchbird
USER scratchbird

# ❌ BAD: Running as root
# (default if USER not specified)
```

### 11.2 Read-Only Root Filesystem

```bash
# Run with read-only root filesystem
docker run -d \
  --read-only \
  --tmpfs /tmp \
  -v scratchbird-data:/data \
  scratchbird:latest
```

### 11.3 Drop Capabilities

```bash
# Run with minimal capabilities
docker run -d \
  --cap-drop=ALL \
  --cap-add=NET_BIND_SERVICE \
  scratchbird:latest
```

### 11.4 Security Scanning

```bash
# Scan image for vulnerabilities (using Trivy)
docker run --rm -v /var/run/docker.sock:/var/run/docker.sock \
  aquasec/trivy image scratchbird:latest

# Scan with Snyk
snyk container test scratchbird:latest
```

---

## 12. Registry and Distribution

### 12.1 Docker Hub

```bash
# Login
docker login

# Tag image
docker tag scratchbird:latest yourusername/scratchbird:latest
docker tag scratchbird:latest yourusername/scratchbird:v0.1.0

# Push
docker push yourusername/scratchbird:latest
docker push yourusername/scratchbird:v0.1.0
```

### 12.2 GitHub Container Registry (GHCR)

```bash
# Login
echo $GITHUB_TOKEN | docker login ghcr.io -u USERNAME --password-stdin

# Tag
docker tag scratchbird:latest ghcr.io/yourusername/scratchbird:latest

# Push
docker push ghcr.io/yourusername/scratchbird:latest
```

### 12.3 Private Registry

```bash
# Run private registry
docker run -d -p 5000:5000 --name registry registry:2

# Tag and push
docker tag scratchbird:latest localhost:5000/scratchbird:latest
docker push localhost:5000/scratchbird:latest
```

---

## 13. CI/CD Integration

### 13.1 GitHub Actions

**.github/workflows/docker-build.yml**:
```yaml
name: Docker Build and Push

on:
  push:
    branches: [main]
    tags: ['v*']
  pull_request:
    branches: [main]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Set up Docker Buildx
        uses: docker/setup-buildx-action@v3

      - name: Login to GitHub Container Registry
        uses: docker/login-action@v3
        with:
          registry: ghcr.io
          username: ${{ github.actor }}
          password: ${{ secrets.GITHUB_TOKEN }}

      - name: Extract metadata
        id: meta
        uses: docker/metadata-action@v5
        with:
          images: ghcr.io/${{ github.repository }}
          tags: |
            type=ref,event=branch
            type=semver,pattern={{version}}
            type=semver,pattern={{major}}.{{minor}}
            type=sha

      - name: Build and push
        uses: docker/build-push-action@v5
        with:
          context: .
          platforms: linux/amd64,linux/arm64
          push: ${{ github.event_name != 'pull_request' }}
          tags: ${{ steps.meta.outputs.tags }}
          labels: ${{ steps.meta.outputs.labels }}
          cache-from: type=gha
          cache-to: type=gha,mode=max
```

---

## 14. Troubleshooting

### 14.1 Build Failures

```bash
# Clean build cache
docker builder prune -a

# Build with no cache
docker build --no-cache -t scratchbird:latest .

# Check build logs
docker build --progress=plain -t scratchbird:latest .
```

### 14.2 Container Not Starting

```bash
# View container logs
docker logs scratchbird-server

# Run interactively to debug
docker run -it --rm scratchbird:latest /bin/bash

# Check health
docker inspect --format='{{.State.Health.Status}}' scratchbird-server
```

### 14.3 Networking Issues

```bash
# Inspect network
docker network inspect bridge

# Check container IP
docker inspect -f '{{range.NetworkSettings.Networks}}{{.IPAddress}}{{end}}' scratchbird-server

# Test connectivity
docker run --rm --network container:scratchbird-server nicolaka/netshoot ping localhost
```

---

## 15. Additional Resources

- **Docker Documentation:** https://docs.docker.com/
- **Docker Build Best Practices:** https://docs.docker.com/develop/dev-best-practices/
- **Multi-Stage Builds:** https://docs.docker.com/build/building/multi-stage/
- **Docker Compose:** https://docs.docker.com/compose/

---

**Document Version:** 1.0
**Last Updated:** 2026-01-03
**Maintainer:** Build Infrastructure Team
**Status:** Beta Preparation
