Status: reconstructed_required

# Embedded Local Only and Layered Deployment Operator Surface Model

## Purpose

This document defines the operator-facing expectations for local-only embedded deployment and layered scaled deployment.

## Canonical Rule

Operator surfaces shall describe which deployment variant is active, which layers are present, and which control operations are meaningful in that variant.

## Required Deployment Surface Fields

Every operator-facing deployment inspection surface shall be able to report:

- deployment variant
- engine embedding mode
- parser presence or absence
- IPC server presence or absence
- listener presence or absence
- manager presence or absence
- remote-management availability

## Local-Only Rule

For local-only embedded deployments, operator tooling shall not pretend listener or manager controls exist if those layers are absent.

## Layered Rule

For layered deployments, operator tooling shall expose:

- listener topology state
- parser-pool state
- manager routing state where present
- deployment generation or configuration state

## Non-Guarantees

This file does not require one single UI or CLI. It requires honest exposure of which layers exist in the active deployment form.
