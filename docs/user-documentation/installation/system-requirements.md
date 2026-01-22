# System Requirements

Hardware and software prerequisites for running ScratchBird.

[Back to Installation Index](index.md) | [Back to Documentation Index](../index.md)

---

## Minimum Requirements

### Hardware

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| **CPU** | x86-64 processor | Multi-core processor (4+ cores) |
| **RAM** | 512 MB | 4 GB or more |
| **Storage** | 100 MB (software) | SSD recommended for production |
| **Network** | 100 Mbps | Gigabit Ethernet |

### Memory Considerations

ScratchBird uses memory for:
- **Shared Buffers**: Cache for database pages (default: 128 MB)
- **Work Memory**: Per-operation memory for sorts and joins (default: 4 MB)
- **Connection Overhead**: ~2 MB per connection

**Memory Formula:**
```
Total Memory = Shared Buffers + (Max Connections x Work Memory) + OS Overhead
```

---

## Supported Operating Systems

### Linux

| Distribution | Version | Status |
|--------------|---------|--------|
| Ubuntu | 20.04 LTS, 22.04 LTS, 24.04 LTS | Fully Supported |
| Debian | 11 (Bullseye), 12 (Bookworm) | Fully Supported |
| RHEL | 8, 9 | Fully Supported |
| Fedora | 38, 39, 40 | Fully Supported |
| Rocky Linux | 8, 9 | Fully Supported |
| AlmaLinux | 8, 9 | Fully Supported |
| Arch Linux | Rolling | Community Supported |

**Kernel Requirement:** Linux kernel 4.4 or later

### Windows

| Version | Architecture | Status |
|---------|-------------|--------|
| Windows 10 | x64 | Fully Supported |
| Windows 11 | x64 | Fully Supported |
| Windows Server 2019 | x64 | Fully Supported |
| Windows Server 2022 | x64 | Fully Supported |

### Docker

Any platform supporting Docker Engine 20.10 or later.

---

## Required Dependencies

### Linux Runtime Dependencies

**Debian/Ubuntu:**
```bash
sudo apt install libc6 libstdc++6 libssl3 libsystemd0
```

**RHEL/Fedora:**
```bash
sudo dnf install glibc libstdc++ openssl-libs systemd-libs
```

### Optional Dependencies

| Package | Purpose | Install Command (Debian) |
|---------|---------|-------------------------|
| liblz4-1 | Compression support | `apt install liblz4-1` |
| libgeos-c1 | Spatial functions | `apt install libgeos-c1v5` |
| libproj25 | Coordinate systems | `apt install libproj25` |
| libxml2 | XML functions | `apt install libxml2` |

---

## Build Dependencies

For compiling from source:

### Debian/Ubuntu
```bash
sudo apt install \
    build-essential \
    cmake \
    pkg-config \
    libssl-dev \
    liblz4-dev \
    libgeos-dev \
    libproj-dev \
    libxml2-dev
```

### RHEL/Fedora
```bash
sudo dnf install \
    gcc-c++ \
    cmake \
    pkg-config \
    openssl-devel \
    lz4-devel \
    geos-devel \
    proj-devel \
    libxml2-devel
```

### Minimum Build Tool Versions

| Tool | Minimum Version |
|------|-----------------|
| CMake | 3.20 |
| GCC | 9.0 (C++17 support) |
| Clang | 10.0 (C++17 support) |

---

## Network Requirements

### Firewall Ports

ScratchBird uses the following default ports:

| Port | Protocol | Description |
|------|----------|-------------|
| 3092 | TCP | ScratchBird Native protocol |
| 5432 | TCP | PostgreSQL wire protocol |
| 3306 | TCP | MySQL wire protocol |
| 3050 | TCP | Firebird wire protocol |

**Open Firewall (Linux):**
```bash
# firewalld (RHEL/Fedora)
sudo firewall-cmd --add-port=3092/tcp --permanent
sudo firewall-cmd --add-port=5432/tcp --permanent
sudo firewall-cmd --reload

# ufw (Ubuntu)
sudo ufw allow 3092/tcp
sudo ufw allow 5432/tcp
```

### Unix Socket

For local connections, ScratchBird uses:
- Socket path: `/var/run/scratchbird/sb.sock`
- Permissions: 0770
- Group: `scratchbird`

---

## File System Requirements

### Recommended File Systems

| File System | Suitability |
|-------------|-------------|
| ext4 | Excellent |
| XFS | Excellent |
| Btrfs | Good |
| ZFS | Good |
| NTFS | Windows only |

### File System Features

ScratchBird requires:
- POSIX file locking (flock/fcntl)
- Sparse file support (recommended)
- Large file support (>2 GB)

---

## Disk Space

### Installation Size

| Component | Size |
|-----------|------|
| Server binaries | ~15 MB |
| Client tools | ~10 MB |
| Documentation | ~5 MB |
| **Total** | ~30 MB |

### Data Storage

Database size depends on your data. Plan for:
- Database files (.sbdb)
- Write-ahead log (if enabled)
- Backup files
- Log files

---

## Resource Limits

### Linux Kernel Limits

ScratchBird may require increased kernel limits. The systemd service file sets:

```ini
LimitNOFILE=65536    # Open file descriptors
LimitNPROC=4096      # Processes
LimitCORE=infinity   # Core dumps
LimitMEMLOCK=infinity # Memory lock
```

**Manual Configuration (without systemd):**
```bash
# /etc/security/limits.conf
scratchbird soft nofile 65536
scratchbird hard nofile 65536
scratchbird soft nproc 4096
scratchbird hard nproc 4096
```

---

## Virtual Machine Considerations

When running ScratchBird in a VM:

1. **CPU**: Allocate sufficient vCPUs for your workload
2. **Memory**: Don't overcommit; ensure buffer_pool_size fits in RAM
3. **Storage**: Use paravirtualized disk drivers (virtio)
4. **Network**: Use paravirtualized network (virtio-net)

### Recommended VM Settings

| Setting | Recommendation |
|---------|---------------|
| vCPUs | 2 minimum, 4+ for production |
| RAM | 2 GB minimum, 4+ GB recommended |
| Disk | Thick provisioned or SSD-backed |

---

## Cloud Platform Considerations

### AWS
- Instance types: t3.medium or larger
- Storage: gp3 or io1 EBS volumes
- Network: VPC with proper security groups

### Azure
- VM sizes: B2s or larger
- Storage: Premium SSD or Ultra SSD
- Network: Virtual network with NSG rules

### GCP
- Machine types: e2-medium or larger
- Storage: SSD persistent disk
- Network: VPC with firewall rules

---

## Next Steps

Once your system meets these requirements:

1. Choose an [installation method](index.md)
2. Follow the platform-specific guide
3. Proceed to [configuration](../configuration/index.md)
