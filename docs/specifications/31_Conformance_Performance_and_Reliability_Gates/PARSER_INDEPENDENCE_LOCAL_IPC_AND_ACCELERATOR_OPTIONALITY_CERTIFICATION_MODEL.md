Status: reconstructed_required

# Parser Independence Local IPC and Accelerator Optionality Certification Model

## Purpose

This document defines the certification evidence required for parser independence, local-only IPC deployment, and optional accelerator packaging.

## Required Certification Classes

Certification shall cover:

- single-parser deployment with no other parser present
- multiple independent parser deployments with no cross-parser dependency
- direct embedded engine deployment with no parser
- local-only IPC deployment with no IP listener
- package variants with and without LLVM or GPU support

## Required Evidence

Each certification case shall preserve:

- deployment variant
- parser set present or absent
- local IPC endpoint state if applicable
- accelerator package class
- admitted or refused accelerator state if applicable
- outcome classification

## Failure Criteria

Certification fails when:

- one parser requires another parser to function
- local-only IPC deployment depends on IP listener presence
- package optionality for LLVM or GPU changes semantic correctness
- tooling or runtime claims a layer exists when the package omits it

## Non-Guarantees

This file does not require all current packages to expose all variants yet. It defines the certification target for the recovered architecture.
