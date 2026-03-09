# Platform Support Matrix

[Prerequisites README](../README.md) | [Installation README](../../README.md)

## Synopsis

Supported platforms and system requirements for ScratchBird installation.

## Operating Systems

### Linux (Tier 1 - Fully Supported)

| Distribution | Versions | Architectures |
|--------------|----------|---------------|
| Ubuntu | 22.04 LTS, 24.04 LTS | x86_64, ARM64 |
| Debian | 11, 12 | x86_64, ARM64 |
| RHEL/CentOS | 8, 9 | x86_64, ARM64 |
| Rocky Linux | 8, 9 | x86_64, ARM64 |
| AlmaLinux | 8, 9 | x86_64, ARM64 |
| Fedora | 39, 40 | x86_64, ARM64 |
| openSUSE | Leap 15.5, Tumbleweed | x86_64, ARM64 |

### macOS (Tier 1)

| Version | Architectures |
|---------|---------------|
| macOS 13 (Ventura) | Intel, Apple Silicon |
| macOS 14 (Sonoma) | Intel, Apple Silicon |
| macOS 15 (Sequoia) | Intel, Apple Silicon |

### Windows (Tier 2)

| Version | Architectures | Notes |
|---------|---------------|-------|
| Windows Server 2022 | x86_64 | Via WSL2 recommended |
| Windows 11 | x86_64, ARM64 | WSL2 recommended |
| Windows 10 | x86_64 | WSL2 only |

### FreeBSD (Tier 2)

| Version | Architectures |
|---------|---------------|
| FreeBSD 14.x | x86_64, ARM64 |

## System Requirements

### Minimum Requirements

| Component | Minimum |
|-----------|---------|
| CPU | 2 cores, x86_64 or ARM64 |
| RAM | 2 GB |
| Disk | 10 GB free space |
| Network | TCP/IP connectivity |

### Recommended for Production

| Component | Recommended |
|-----------|-------------|
| CPU | 8+ cores |
| RAM | 16+ GB |
| Disk | SSD, 100+ GB |
| Network | Gigabit Ethernet |

### Large Deployments

| Component | Specification |
|-----------|---------------|
| CPU | 32+ cores |
| RAM | 128+ GB |
| Disk | NVMe SSD, 1+ TB |
| Network | 10GbE or better |

## Architecture Support

| Architecture | Status | Notes |
|--------------|--------|-------|
| x86_64 (amd64) | ✅ Production | Primary platform |
| ARM64 (aarch64) | ✅ Production | Apple Silicon, AWS Graviton |
| RISC-V 64 | 🧪 Experimental | Future support |

## Docker Support

| Platform | Status |
|----------|--------|
| Linux containers | ✅ Supported |
| Windows containers | ⚠️ Limited |
| macOS (Docker Desktop) | ✅ Supported |

## Kubernetes Support

| Platform | Status |
|----------|--------|
| vanilla Kubernetes | ✅ 1.25+ |
| OpenShift | ✅ 4.12+ |
| EKS | ✅ Supported |
| GKE | ✅ Supported |
| AKS | ✅ Supported |

## Compatibility Matrix

### LLVM JIT Support

| Platform | LLVM Version | JIT Support |
|----------|--------------|-------------|
| Linux x86_64 | 15+ | ✅ Full |
| Linux ARM64 | 15+ | ✅ Full |
| macOS Intel | 15+ | ✅ Full |
| macOS Apple Silicon | 15+ | ✅ Full |

### Emulation Support

| Platform | PostgreSQL | MySQL | Firebird |
|----------|------------|-------|----------|
| Linux x86_64 | ✅ | ✅ | ✅ |
| Linux ARM64 | ✅ | ✅ | ✅ |
| macOS | ✅ | ✅ | ✅ |

## See Also

- [Choose Installation Path](02_choose_installation_path.md)
- [Verify System Requirements](03_verify_system_requirements.md)
