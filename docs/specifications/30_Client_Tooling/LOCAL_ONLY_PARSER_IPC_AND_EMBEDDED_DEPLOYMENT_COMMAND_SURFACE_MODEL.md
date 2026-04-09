Status: reconstructed_required

# Local Only Parser IPC and Embedded Deployment Command Surface Model

## Purpose

This document defines the operator and tooling command-surface expectations for local-only parser-plus-IPC and embedded deployments.

## Canonical Rule

Tooling shall expose deployment-appropriate commands. Local-only or embedded deployments shall not present listener or manager command surfaces unless those layers are actually present.

## Local-Only Command Classes

For local-only deployments, tooling may expose:

- direct engine attachment inspection
- local IPC endpoint inspection
- parser-library presence and dialect inspection
- database-local configuration and maintenance commands

## Embedded Command Classes

For embedded deployments, tooling may expose:

- engine-library capability inspection
- parser-library capability inspection if present
- local transaction and attachment inspection

## Layer Absence Rule

If no listener, parser-agent pool, or manager exists, tooling shall not fabricate:

- listener topology commands
- parser-pool controls
- manager routing or heartbeat commands

## Non-Guarantees

This file does not require every embedded deployment to have a rich admin CLI. It requires honest command-surface scoping.
