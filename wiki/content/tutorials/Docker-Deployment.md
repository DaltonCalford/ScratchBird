# Docker Deployment

**Last Updated:** 2026-01-30

---

## Overview

This guide covers deploying ScratchBird with Docker for development, testing, and production environments. You'll learn container configuration, data persistence, networking, and production hardening.

**What you'll learn:**
- Running ScratchBird in Docker
- Docker Compose configurations
- Data persistence with volumes
- Multi-container deployments
- Production considerations

---

## Part 1: Quick Start

### Pull the Image

```bash
docker pull scratchbird/scratchbird:latest
```

### Run ScratchBird

**Basic run:**
```bash
docker run -d \
    --name scratchbird \
    -p 3092:3092 \
    -p 5432:5432 \
    scratchbird/scratchbird:latest
```

**With persistent data:**
```bash
docker run -d \
    --name scratchbird \
    -p 3092:3092 \
    -p 5432:5432 \
    -v scratchbird_data:/var/lib/scratchbird \
    scratchbird/scratchbird:latest
```

### Verify It's Running

```bash
# Check container status
docker ps

# View logs
docker logs scratchbird

# Test connection
docker exec -it scratchbird sb_isql -U admin -d scratchbird -c "SELECT version()"
```

---

## Part 2: Docker Compose (Development)

### Basic Configuration

Create `docker-compose.yml`:

```yaml
version: '3.8'

services:
  scratchbird:
    image: scratchbird/scratchbird:latest
    container_name: scratchbird
    ports:
      - "3092:3092"   # Native protocol
      - "5432:5432"   # PostgreSQL protocol
      - "3306:3306"   # MySQL protocol
      - "3050:3050"   # Firebird protocol
    volumes:
      - scratchbird_data:/var/lib/scratchbird
    environment:
      SCRATCHBIRD_USER: admin
      SCRATCHBIRD_PASSWORD: devpassword
      SCRATCHBIRD_DB: development
    restart: unless-stopped

volumes:
  scratchbird_data:
```

### Start and Stop

```bash
# Start
docker compose up -d

# View logs
docker compose logs -f

# Stop
docker compose down

# Stop and remove volumes (WARNING: deletes data)
docker compose down -v
```

---

## Part 3: Configuration Options

### Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `SCRATCHBIRD_USER` | Admin username | `admin` |
| `SCRATCHBIRD_PASSWORD` | Admin password | (auto-generated) |
| `SCRATCHBIRD_DB` | Default database | `scratchbird` |
| `SCRATCHBIRD_MAX_CONNECTIONS` | Max connections | `100` |
| `SCRATCHBIRD_SHARED_BUFFERS` | Shared buffer size | `256MB` |
| `SCRATCHBIRD_WORK_MEM` | Work memory | `8MB` |

### Custom Configuration File

Create `sb_server.conf`:

```ini
[server]
mode = multi-database
data_dir = /var/lib/scratchbird
max_connections = 200
worker_threads = 0

[network]
bind_address = 0.0.0.0
native_port = 3092
pg_port = 5432
mysql_port = 3306
fb_port = 3050

[ssl]
enabled = false

[memory]
buffer_pool_size = 512MB
work_mem = 16MB

[logging]
level = info
destination = stderr
```

Mount in container:

```yaml
services:
  scratchbird:
    image: scratchbird/scratchbird:latest
    volumes:
      - scratchbird_data:/var/lib/scratchbird
      - ./sb_server.conf:/etc/scratchbird/sb_server.conf:ro
```

---

## Part 4: Data Persistence

### Named Volumes (Recommended)

```yaml
services:
  scratchbird:
    volumes:
      - scratchbird_data:/var/lib/scratchbird

volumes:
  scratchbird_data:
    driver: local
```

### Bind Mounts

For direct access to data files:

```yaml
services:
  scratchbird:
    volumes:
      - ./data:/var/lib/scratchbird
```

**Note:** Ensure proper permissions:
```bash
mkdir -p ./data
chmod 700 ./data
```

### Volume Backup

```bash
# Backup volume
docker run --rm \
    -v scratchbird_data:/source:ro \
    -v $(pwd):/backup \
    alpine tar czf /backup/scratchbird_backup_$(date +%Y%m%d).tar.gz -C /source .

# Restore volume
docker run --rm \
    -v scratchbird_data:/target \
    -v $(pwd):/backup:ro \
    alpine sh -c "rm -rf /target/* && tar xzf /backup/scratchbird_backup_20260118.tar.gz -C /target"
```

---

## Part 5: Multi-Container Deployment

### With Application Stack

```yaml
version: '3.8'

services:
  scratchbird:
    image: scratchbird/scratchbird:latest
    container_name: scratchbird
    volumes:
      - scratchbird_data:/var/lib/scratchbird
    environment:
      SCRATCHBIRD_PASSWORD: ${DB_PASSWORD:-secret}
    networks:
      - backend
    healthcheck:
      test: ["CMD", "sb_isql", "-c", "SELECT 1", "-H", "localhost", "-p", "3092", "-U", "admin"]
      interval: 30s
      timeout: 10s
      retries: 3
      start_period: 30s

  api:
    build: ./api
    container_name: api
    ports:
      - "3000:3000"
    environment:
      DB_HOST: scratchbird
      DB_PORT: 5432
      DB_NAME: myapp
      DB_USER: admin
      DB_PASSWORD: ${DB_PASSWORD:-secret}
    depends_on:
      scratchbird:
        condition: service_healthy
    networks:
      - backend
      - frontend

  web:
    build: ./web
    container_name: web
    ports:
      - "80:80"
    depends_on:
      - api
    networks:
      - frontend

networks:
  backend:
    driver: bridge
  frontend:
    driver: bridge

volumes:
  scratchbird_data:
```

### With Redis Cache

```yaml
version: '3.8'

services:
  scratchbird:
    image: scratchbird/scratchbird:latest
    volumes:
      - scratchbird_data:/var/lib/scratchbird
    networks:
      - backend

  redis:
    image: redis:7-alpine
    volumes:
      - redis_data:/data
    networks:
      - backend

  api:
    build: ./api
    environment:
      DB_HOST: scratchbird
      REDIS_HOST: redis
    depends_on:
      - scratchbird
      - redis
    networks:
      - backend

networks:
  backend:

volumes:
  scratchbird_data:
  redis_data:
```

---

## Part 6: Networking

### Internal Network Only

Database not exposed to host:

```yaml
services:
  scratchbird:
    image: scratchbird/scratchbird:latest
    # No ports exposed to host
    networks:
      - internal

  api:
    build: ./api
    ports:
      - "3000:3000"  # Only API exposed
    networks:
      - internal

networks:
  internal:
    driver: bridge
```

### Multiple Networks

Isolate database from public network:

```yaml
services:
  scratchbird:
    networks:
      - database

  api:
    networks:
      - database
      - public

  nginx:
    networks:
      - public

networks:
  database:
    internal: true  # No external access
  public:
```

### Custom Network Configuration

```yaml
networks:
  backend:
    driver: bridge
    ipam:
      config:
        - subnet: 172.28.0.0/16
          gateway: 172.28.0.1
```

---

## Part 7: Production Configuration

### Production docker-compose.yml

```yaml
version: '3.8'

services:
  scratchbird:
    image: scratchbird/scratchbird:latest
    container_name: scratchbird-prod
    restart: always
    volumes:
      - scratchbird_data:/var/lib/scratchbird
      - ./config/sb_server.conf:/etc/scratchbird/sb_server.conf:ro
      - ./certs:/etc/scratchbird/certs:ro
    environment:
      SCRATCHBIRD_PASSWORD_FILE: /run/secrets/db_password
    secrets:
      - db_password
    networks:
      - backend
    deploy:
      resources:
        limits:
          cpus: '4'
          memory: 8G
        reservations:
          cpus: '2'
          memory: 4G
    healthcheck:
      test: ["CMD", "sb_isql", "-c", "SELECT 1", "-H", "localhost", "-p", "3092", "-U", "admin"]
      interval: 30s
      timeout: 10s
      retries: 5
      start_period: 60s
    logging:
      driver: "json-file"
      options:
        max-size: "100m"
        max-file: "5"

secrets:
  db_password:
    file: ./secrets/db_password.txt

networks:
  backend:
    driver: bridge

volumes:
  scratchbird_data:
    driver: local
```

### Production Configuration File

Create `config/sb_server.conf`:

```ini
[server]
mode = multi-database
data_dir = /var/lib/scratchbird
max_connections = 500
worker_threads = 0
shutdown_timeout = 60

[network]
bind_address = 0.0.0.0
native_port = 3092
pg_port = 5432
mysql_port = 3306
fb_port = 3050

[ssl]
enabled = true
cert_file = /etc/scratchbird/certs/server.crt
key_file = /etc/scratchbird/certs/server.key
ca_file = /etc/scratchbird/certs/ca.crt

[memory]
buffer_pool_size = 2GB
work_mem = 64MB
maintenance_work_mem = 512MB

[logging]
level = warning
destination = stderr

[statistics]
enabled = true
export = prometheus
prometheus_port = 9090
```

### SSL/TLS Setup

Generate certificates:

```bash
mkdir -p certs

# Generate CA
openssl genrsa -out certs/ca.key 4096
openssl req -x509 -new -nodes -key certs/ca.key -sha256 -days 3650 \
    -out certs/ca.crt -subj "/CN=ScratchBird CA"

# Generate server certificate
openssl genrsa -out certs/server.key 2048
openssl req -new -key certs/server.key \
    -out certs/server.csr -subj "/CN=scratchbird"
openssl x509 -req -in certs/server.csr -CA certs/ca.crt -CAkey certs/ca.key \
    -CAcreateserial -out certs/server.crt -days 365 -sha256

# Set permissions
chmod 600 certs/server.key
```

---

## Part 8: Monitoring

### With Prometheus

```yaml
version: '3.8'

services:
  scratchbird:
    image: scratchbird/scratchbird:latest
    volumes:
      - scratchbird_data:/var/lib/scratchbird
      - ./config/sb_server.conf:/etc/scratchbird/sb_server.conf:ro
    ports:
      - "9090:9090"  # Prometheus metrics
    networks:
      - monitoring

  prometheus:
    image: prom/prometheus:latest
    volumes:
      - ./prometheus.yml:/etc/prometheus/prometheus.yml:ro
      - prometheus_data:/prometheus
    ports:
      - "9091:9090"
    networks:
      - monitoring

  grafana:
    image: grafana/grafana:latest
    volumes:
      - grafana_data:/var/lib/grafana
    ports:
      - "3001:3000"
    environment:
      GF_SECURITY_ADMIN_PASSWORD: admin
    networks:
      - monitoring

networks:
  monitoring:

volumes:
  scratchbird_data:
  prometheus_data:
  grafana_data:
```

Create `prometheus.yml`:

```yaml
global:
  scrape_interval: 15s

scrape_configs:
  - job_name: 'scratchbird'
    static_configs:
      - targets: ['scratchbird:9090']
```

### Health Check Endpoint

```bash
# Check from host
curl http://localhost:9090/metrics

# Key metrics to monitor:
# - scratchbird_connections_active
# - scratchbird_queries_total
# - scratchbird_transactions_committed
# - scratchbird_buffer_pool_hit_ratio
```

---

## Part 9: Backup and Restore

### Automated Backup with Cron

Create `backup.sh`:

```bash
#!/bin/bash
BACKUP_DIR=/backups
DATE=$(date +%Y%m%d_%H%M%S)
RETENTION_DAYS=7

# Create backup
docker exec scratchbird sb_backup -U admin -o /tmp/backup_${DATE}.sbk

# Copy from container
docker cp scratchbird:/tmp/backup_${DATE}.sbk ${BACKUP_DIR}/

# Compress
gzip ${BACKUP_DIR}/backup_${DATE}.sbk

# Clean old backups
find ${BACKUP_DIR} -name "backup_*.sbk.gz" -mtime +${RETENTION_DAYS} -delete

echo "Backup completed: ${BACKUP_DIR}/backup_${DATE}.sbk.gz"
```

Add to crontab:
```bash
0 2 * * * /path/to/backup.sh >> /var/log/scratchbird-backup.log 2>&1
```

### Using Docker Compose

```yaml
services:
  backup:
    image: scratchbird/scratchbird:latest
    volumes:
      - scratchbird_data:/var/lib/scratchbird:ro
      - ./backups:/backups
    command: >
      sh -c "sb_backup -U admin -d scratchbird -o /backups/backup_$$(date +%Y%m%d).sbk"
    profiles:
      - backup
```

Run backup:
```bash
docker compose --profile backup run --rm backup
```

### Restore

```bash
# Stop the database
docker compose stop scratchbird

# Restore from backup
docker run --rm \
    -v scratchbird_data:/var/lib/scratchbird \
    -v $(pwd)/backups:/backups:ro \
    scratchbird/scratchbird:latest \
    sb_restore -i /backups/backup_20260118.sbk -d /var/lib/scratchbird

# Start the database
docker compose start scratchbird
```

---

## Part 10: Troubleshooting

### Container Won't Start

```bash
# Check logs
docker logs scratchbird

# Common issues:
# - Port already in use
# - Volume permissions
# - Configuration errors

# Test configuration
docker run --rm \
    -v ./sb_server.conf:/etc/scratchbird/sb_server.conf:ro \
    scratchbird/scratchbird:latest \
    sb_server --config /etc/scratchbird/sb_server.conf --check
```

### Connection Refused

```bash
# Check if container is running
docker ps

# Check if port is listening inside container
docker exec scratchbird ss -tlnp

# Check network connectivity
docker exec scratchbird ping api  # If using compose
```

### Performance Issues

```bash
# Check resource usage
docker stats scratchbird

# Check for slow queries
docker exec scratchbird sb_isql -U admin -c "SELECT * FROM sb_catalog.slow_queries"

# Increase resources in compose
deploy:
  resources:
    limits:
      memory: 16G
```

### Data Recovery

```bash
# If container crashed but volume is intact
docker run -it --rm \
    -v scratchbird_data:/var/lib/scratchbird \
    scratchbird/scratchbird:latest \
    sb_admin --repair /var/lib/scratchbird
```

---

## Quick Reference

### Common Commands

```bash
# Start
docker compose up -d

# Stop
docker compose down

# Logs
docker compose logs -f scratchbird

# Shell access
docker exec -it scratchbird /bin/sh

# SQL client
docker exec -it scratchbird sb_isql -U admin -d scratchbird

# Backup
docker exec scratchbird sb_backup -U admin -o /tmp/backup.sbk
docker cp scratchbird:/tmp/backup.sbk ./

# Status
docker compose ps
docker stats scratchbird
```

### Port Reference

| Port | Protocol | Description |
|------|----------|-------------|
| 3092 | Native | ScratchBird native protocol |
| 5432 | PostgreSQL | PostgreSQL wire protocol |
| 3306 | MySQL | MySQL wire protocol |
| 3050 | Firebird | Firebird wire protocol |
| 9090 | HTTP | Prometheus metrics |

---

## See Also

- [Kubernetes Installation](../installation/Kubernetes.md)
- [Linux Installation](../installation/Linux.md)
- [Backup and Restore](../admin/backup-restore.md)
- [Monitoring Guide](../admin/monitoring.md)

