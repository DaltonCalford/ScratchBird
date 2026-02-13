# Deployment Specifications

**[← Back to Specifications Index](../README.md)**

This directory contains deployment and installation specifications for ScratchBird.

## Overview

ScratchBird supports modern deployment models including systemd service integration, containerization, and cloud deployment.

## Specifications in this Directory

- **[SYSTEMD_SERVICE_SPECIFICATION.md](SYSTEMD_SERVICE_SPECIFICATION.md)** (2,127 lines) - systemd service integration specification
- **[INSTALLATION_AND_BUILD_SPECIFICATION.md](INSTALLATION_AND_BUILD_SPECIFICATION.md)** - install/build methods and dependency matrix
- **[WINDOWS_CROSS_COMPILE_SPECIFICATION.md](WINDOWS_CROSS_COMPILE_SPECIFICATION.md)** - Windows cross-compile requirements
- **[INSTALLER_FEATURES_AND_CONFIG_GENERATOR.md](INSTALLER_FEATURES_AND_CONFIG_GENERATOR.md)** - installer feature matrix + config wizard

## Key Features

### systemd Integration

- **Service management** - Start, stop, restart, reload via systemd
- **Automatic restart** - Restart on failure with backoff
- **Resource limits** - CPU, memory, file descriptor limits
- **Logging** - Integration with systemd journal
- **Socket activation** - systemd socket activation support
- **Security hardening** - systemd security directives

### Deployment Models

- **Single-node** - Standalone server deployment
- **Cluster** - Multi-node cluster deployment (Beta)
- **Container** - Docker/Kubernetes deployment (see [Beta Requirements](../beta_requirements/cloud-container/))
- **Cloud** - AWS, GCP, Azure deployment

## Related Specifications

- [Beta Requirements - Cloud/Container](../beta_requirements/cloud-container/) - Container deployment specifications
- [Admin](../admin/) - Administration tools
- [Operations](../operations/) - Operational monitoring
- [Cluster](../Cluster%20Specification%20Work/) - Cluster deployment (Beta)

## Navigation

- **Parent Directory:** [Specifications Index](../README.md)
- **Project Root:** [ScratchBird Home](../../../README.md)

---

**Last Updated:** January 2026
