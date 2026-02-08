# Cloud & Container Ecosystem

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**[← Back to Beta Requirements](../README.md)** | **[← Back to Specifications Index](../../README.md)**

This directory contains specifications for cloud platforms and container ecosystem integrations.

## Overview

ScratchBird provides first-class support for modern cloud-native deployments with Docker containers, Kubernetes operators, Helm charts, and Infrastructure as Code tools.

## Specifications

### Container Platform

**[docker/](docker/)** - Docker Support
- Official Docker images
- Multi-architecture support (amd64, arm64)
- Optimized layer caching
- Security scanning
- **Market Share:** Standard containerization platform
- Authoritative spec: [docker/SPECIFICATION.md](docker/SPECIFICATION.md)

### Container Orchestration

**[kubernetes/](kubernetes/)** - Kubernetes Integration
- Kubernetes Operators
- StatefulSets for persistence
- Service discovery
- ConfigMaps and Secrets
- **Market Share:** Dominant orchestration platform

### Package Management

**[helm-charts/](helm-charts/)** - Helm Charts
- Official Helm charts
- Customizable deployments
- Upgrade strategies
- **Market Share:** Standard Kubernetes package manager

### Infrastructure as Code

**[terraform/](terraform/)** - Terraform Modules
- AWS module
- GCP module
- Azure module
- **Market Share:** Leading IaC tool

## Cloud Provider Support

- **AWS** - EC2, RDS compatibility, EKS
- **GCP** - Compute Engine, GKE
- **Azure** - Virtual Machines, AKS
- **Self-hosted** - Bare metal, private cloud

## Navigation

- **Parent Directory:** [Beta Requirements](../README.md)
- **Specifications Index:** [Specifications Index](../../README.md)
- **Project Root:** [ScratchBird Home](../../../../README.md)

---

**Last Updated:** January 2026
