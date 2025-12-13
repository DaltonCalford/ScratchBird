# Installation Guide

This section covers installing ScratchBird on various platforms.

[Back to Documentation Index](../index.md)

---

## Choose Your Installation Method

### Linux

| Method | Best For | Guide |
|--------|----------|-------|
| **DEB Package** | Ubuntu, Debian, Linux Mint | [Linux (DEB)](linux-deb.md) |
| **RPM Package** | RHEL, Fedora, CentOS, Rocky | [Linux (RPM)](linux-rpm.md) |
| **Tarball** | Any Linux distribution | [Linux (Tarball)](linux-tarball.md) |

### Windows

| Method | Best For | Guide |
|--------|----------|-------|
| **Installer** | Standard Windows installation | [Windows (Installer)](windows-installer.md) |
| **Portable** | USB drive, no admin rights | [Windows (Portable)](windows-portable.md) |

### Container

| Method | Best For | Guide |
|--------|----------|-------|
| **Docker** | Development, testing, microservices | [Docker](docker.md) |

### From Source

| Method | Best For | Guide |
|--------|----------|-------|
| **Build** | Developers, custom builds | [Building from Source](building-from-source.md) |

---

## Prerequisites

Before installing, ensure your system meets the [System Requirements](system-requirements.md).

---

## Quick Install Commands

### Debian/Ubuntu
```bash
sudo dpkg -i scratchbird_0.9.0-beta0_amd64.deb
sudo systemctl enable scratchbird
sudo systemctl start scratchbird
```

### RHEL/Fedora
```bash
sudo rpm -i scratchbird-0.9.0-beta0-1.x86_64.rpm
sudo systemctl enable scratchbird
sudo systemctl start scratchbird
```

### Docker
```bash
docker pull scratchbird/scratchbird:0.9.0-beta0
docker run -d -p 3092:3092 -p 5432:5432 scratchbird/scratchbird:0.9.0-beta0
```

---

## Post-Installation

After installing, proceed to:

1. [Configuration](../configuration/index.md) - Configure the server
2. [First Database](../getting-started/first-database.md) - Create your first database
3. [First Connection](../getting-started/first-connection.md) - Connect with a client

---

## Verify Installation

Test that ScratchBird is running:

```bash
# Check service status (Linux)
sudo systemctl status scratchbird

# Connect using sb_isql
sb_isql -H localhost -P 3092

# Connect using psql (PostgreSQL protocol)
psql -h localhost -p 5432 -U admin
```

---

## Uninstalling

### Debian/Ubuntu
```bash
sudo systemctl stop scratchbird
sudo apt remove scratchbird
```

### RHEL/Fedora
```bash
sudo systemctl stop scratchbird
sudo rpm -e scratchbird
```

### Docker
```bash
docker stop scratchbird
docker rm scratchbird
```

---

## Getting Help

If you encounter installation issues:

1. Check [Troubleshooting](../admin/troubleshooting.md)
2. Review system logs: `journalctl -u scratchbird`
3. Report issues: [GitHub Issues](https://github.com/DaltonCalford/ScratchBird/issues)
