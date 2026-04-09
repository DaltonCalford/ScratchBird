# Cloud Packaging, Support Matrix, and Deployment Automation Model

Status: reconstructed_required
Section: 41_Platform_Interface_and_Lifecycle_Management

## Purpose

Define how ScratchBird is packaged and automated for cloud deployment and how the support matrix is expressed.

## Packaging classes

1. Linux runtime package
- supports non-interactive install, configure, start, stop, and recover procedures
- supports persistent volume attachment and service identity retention

2. Windows runtime package
- supports non-interactive install, configure, start, stop, and recover procedures
- supports persistent volume attachment and service identity retention

3. Auxiliary deployment asset
- may include VM image, container image, install bundle, or IaC profile
- must declare storage, secret, and network prerequisites explicitly
- does not become a supported first-class Beta 1 package profile unless the support matrix says so explicitly

## Support matrix dimensions

The support matrix must identify, at minimum:

- operating system or distro class
- packaging class
- supported storage backend classes
- supported network and load-balancer assumptions
- supported secret and certificate source classes
- supported orchestrator classes, if any
- supported upgrade channel and signing model

Cloud support must never be implied without a declared support matrix entry.

For Beta 1 package `07`, the declared first-class support-matrix entries are:

- Linux runtime package
- Windows runtime package

All other deployment assets are auxiliary only unless explicitly promoted later.

## Deployment automation requirements

Cloud-ready deployment automation must support:

- non-interactive bootstrap
- non-interactive restart and recovery invocation
- compatibility validation before rollout
- explicit rollout refusal on unsupported topology or missing prerequisites
- deterministic identity and storage mapping

## Rollout classes

1. Initial deploy
2. Rolling front-door refresh
3. Stateful engine restart
4. Backup-driven restore deploy
5. Promotion or failback deployment event

Each rollout class must bind to the compatibility manifest and the section 31 certification gates.
