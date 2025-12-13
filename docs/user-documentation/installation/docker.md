# Docker Installation

Run ScratchBird in a Docker container.

[Back to Installation Index](index.md) | [Back to Documentation Index](../index.md)

---

## Prerequisites

- Docker Engine 20.10 or later
- Docker Compose 2.0 or later (optional)

---

## Quick Start

```bash
# Pull the image
docker pull scratchbird/scratchbird:0.9.0-beta0

# Run with default settings
docker run -d \
  --name scratchbird \
  -p 3092:3092 \
  -p 5432:5432 \
  -v scratchbird-data:/var/lib/scratchbird \
  scratchbird/scratchbird:0.9.0-beta0

# Connect
docker exec -it scratchbird sb_isql -H localhost
```

---

## Docker Run Options

### Basic Run

```bash
docker run -d \
  --name scratchbird \
  -p 3092:3092 \
  scratchbird/scratchbird:0.9.0-beta0
```

### All Protocols

```bash
docker run -d \
  --name scratchbird \
  -p 3092:3092 \
  -p 5432:5432 \
  -p 3306:3306 \
  -p 3050:3050 \
  scratchbird/scratchbird:0.9.0-beta0
```

### With Persistent Data

```bash
docker run -d \
  --name scratchbird \
  -p 3092:3092 \
  -p 5432:5432 \
  -v scratchbird-data:/var/lib/scratchbird \
  -v scratchbird-logs:/var/log/scratchbird \
  scratchbird/scratchbird:0.9.0-beta0
```

### With Custom Configuration

```bash
docker run -d \
  --name scratchbird \
  -p 3092:3092 \
  -v /path/to/sb_server.conf:/etc/scratchbird/sb_server.conf:ro \
  -v scratchbird-data:/var/lib/scratchbird \
  scratchbird/scratchbird:0.9.0-beta0
```

---

## Docker Compose

### Basic docker-compose.yml

```yaml
version: '3.8'

services:
  scratchbird:
    image: scratchbird/scratchbird:0.9.0-beta0
    container_name: scratchbird
    ports:
      - "3092:3092"   # Native protocol
      - "5432:5432"   # PostgreSQL protocol
    volumes:
      - scratchbird-data:/var/lib/scratchbird
      - scratchbird-logs:/var/log/scratchbird
    restart: unless-stopped

volumes:
  scratchbird-data:
  scratchbird-logs:
```

### With Custom Configuration

```yaml
version: '3.8'

services:
  scratchbird:
    image: scratchbird/scratchbird:0.9.0-beta0
    container_name: scratchbird
    ports:
      - "3092:3092"
      - "5432:5432"
      - "3306:3306"
      - "3050:3050"
    volumes:
      - ./sb_server.conf:/etc/scratchbird/sb_server.conf:ro
      - scratchbird-data:/var/lib/scratchbird
      - scratchbird-logs:/var/log/scratchbird
    environment:
      - SB_ADMIN_PASSWORD=changeme
    restart: unless-stopped
    healthcheck:
      test: ["CMD", "sb_isql", "-H", "localhost", "-c", "SELECT 1"]
      interval: 30s
      timeout: 10s
      retries: 3

volumes:
  scratchbird-data:
  scratchbird-logs:
```

### Start with Docker Compose

```bash
docker-compose up -d
```

---

## Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `SB_ADMIN_PASSWORD` | Admin user password | (generated) |
| `SB_DATA_DIR` | Data directory | `/var/lib/scratchbird` |
| `SB_LOG_LEVEL` | Logging level | `info` |
| `SB_MAX_CONNECTIONS` | Max connections | `100` |
| `SB_NATIVE_PORT` | Native protocol port | `3092` |
| `SB_PG_PORT` | PostgreSQL protocol port | `5432` |
| `SB_MYSQL_PORT` | MySQL protocol port | `3306` |
| `SB_FB_PORT` | Firebird protocol port | `3050` |

Example:

```bash
docker run -d \
  --name scratchbird \
  -e SB_ADMIN_PASSWORD=mysecretpassword \
  -e SB_MAX_CONNECTIONS=200 \
  -e SB_LOG_LEVEL=debug \
  -p 3092:3092 \
  scratchbird/scratchbird:0.9.0-beta0
```

---

## Connecting to Container

### Using sb_isql Inside Container

```bash
docker exec -it scratchbird sb_isql -H localhost
```

### Using External Client

```bash
# PostgreSQL client
psql -h localhost -p 5432 -U admin

# MySQL client
mysql -h 127.0.0.1 -P 3306 -u admin -p

# sb_isql from host
sb_isql -H localhost -P 3092
```

---

## Managing the Container

### Start/Stop/Restart

```bash
docker start scratchbird
docker stop scratchbird
docker restart scratchbird
```

### View Logs

```bash
# Recent logs
docker logs scratchbird

# Follow logs
docker logs -f scratchbird

# Last 100 lines
docker logs --tail 100 scratchbird
```

### Shell Access

```bash
docker exec -it scratchbird /bin/sh
```

### Check Status

```bash
docker ps
docker inspect scratchbird
```

---

## Data Persistence

### Named Volume (Recommended)

```bash
docker volume create scratchbird-data

docker run -d \
  --name scratchbird \
  -v scratchbird-data:/var/lib/scratchbird \
  scratchbird/scratchbird:0.9.0-beta0
```

### Bind Mount

```bash
mkdir -p /opt/scratchbird-data

docker run -d \
  --name scratchbird \
  -v /opt/scratchbird-data:/var/lib/scratchbird \
  scratchbird/scratchbird:0.9.0-beta0
```

### Backup Volume Data

```bash
# Backup
docker run --rm \
  -v scratchbird-data:/data \
  -v $(pwd):/backup \
  alpine tar czf /backup/scratchbird-backup.tar.gz -C /data .

# Restore
docker run --rm \
  -v scratchbird-data:/data \
  -v $(pwd):/backup \
  alpine tar xzf /backup/scratchbird-backup.tar.gz -C /data
```

---

## Building from Dockerfile

### Sample Dockerfile

```dockerfile
FROM alpine:3.19

# Install runtime dependencies
RUN apk add --no-cache \
    libstdc++ \
    openssl \
    lz4-libs

# Create user
RUN adduser -D -H -s /sbin/nologin scratchbird

# Copy binaries
COPY --from=build /opt/scratchbird/bin /usr/local/bin/
COPY --from=build /opt/scratchbird/lib /usr/local/lib/

# Create directories
RUN mkdir -p /var/lib/scratchbird /var/log/scratchbird /etc/scratchbird \
    && chown -R scratchbird:scratchbird /var/lib/scratchbird /var/log/scratchbird

# Copy default config
COPY sb_server.conf /etc/scratchbird/sb_server.conf

# Expose ports
EXPOSE 3092 5432 3306 3050

# Set user
USER scratchbird

# Healthcheck
HEALTHCHECK --interval=30s --timeout=10s --retries=3 \
    CMD sb_isql -H localhost -c "SELECT 1" || exit 1

# Start server
CMD ["sb_server", "--config", "/etc/scratchbird/sb_server.conf"]
```

### Build Image

```bash
docker build -t my-scratchbird:latest .
```

---

## Resource Limits

### Memory Limits

```bash
docker run -d \
  --name scratchbird \
  --memory=2g \
  --memory-swap=2g \
  scratchbird/scratchbird:0.9.0-beta0
```

### CPU Limits

```bash
docker run -d \
  --name scratchbird \
  --cpus=2.0 \
  scratchbird/scratchbird:0.9.0-beta0
```

### Docker Compose Resource Limits

```yaml
services:
  scratchbird:
    image: scratchbird/scratchbird:0.9.0-beta0
    deploy:
      resources:
        limits:
          cpus: '2.0'
          memory: 2G
        reservations:
          cpus: '0.5'
          memory: 512M
```

---

## Networking

### Custom Network

```bash
# Create network
docker network create scratchbird-net

# Run container on network
docker run -d \
  --name scratchbird \
  --network scratchbird-net \
  scratchbird/scratchbird:0.9.0-beta0

# Connect application
docker run -d \
  --name myapp \
  --network scratchbird-net \
  myapp:latest
```

Application can connect using hostname `scratchbird`.

---

## Upgrading

```bash
# Pull new image
docker pull scratchbird/scratchbird:X.Y.Z

# Stop and remove old container
docker stop scratchbird
docker rm scratchbird

# Start new container (data persists in volume)
docker run -d \
  --name scratchbird \
  -v scratchbird-data:/var/lib/scratchbird \
  scratchbird/scratchbird:X.Y.Z
```

---

## Troubleshooting

### Container Won't Start

```bash
# Check logs
docker logs scratchbird

# Run interactively
docker run -it --rm scratchbird/scratchbird:0.9.0-beta0 /bin/sh
```

### Permission Issues

```bash
# Check volume permissions
docker run --rm -v scratchbird-data:/data alpine ls -la /data

# Fix permissions
docker run --rm -v scratchbird-data:/data alpine chown -R 1000:1000 /data
```

### Port Conflicts

```bash
# Check what's using the port
netstat -tlnp | grep 5432

# Use different host port
docker run -d -p 15432:5432 scratchbird/scratchbird:0.9.0-beta0
```

---

## Next Steps

1. [Configure the server](../configuration/sb_server.conf.md)
2. [Create your first database](../getting-started/first-database.md)
3. [Connect with a client](../getting-started/first-connection.md)
