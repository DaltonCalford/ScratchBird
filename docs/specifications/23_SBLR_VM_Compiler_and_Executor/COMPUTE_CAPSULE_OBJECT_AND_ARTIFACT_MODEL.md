# Compute Capsule Object and Artifact Model

Status: reconstructed_required_with_current_substrate

## Purpose

This file defines the current object model actually proven by code for native-compilation artifacts and the stricter recovered capsule model that future promotion must build on top of that current truth.

## Current code-backed object model

Current code does not prove a separate first-class persisted `compute capsule` catalog object.
Current code instead proves a routine-native artifact model built from the following object families.

### Runtime request object

`JitRuntimeRequest` is the current execution-time request carrier. It includes:
- routine surface kind
- object UUID
- module id
- plan id
- canonical SBLR bytes
- compatibility key
- policy envelope

### Compatibility key object

`ArtifactCompatibilityKey` is the current native identity contract. It includes:
- object UUID
- canonical SBLR hash
- target triple
- CPU feature profile
- native ABI version
- compiler identity
- compiler version
- optimization profile
- security policy version

### Persisted artifact object

`JitArtifact` is the current persisted native-artifact carrier. It includes:
- artifact id
- module id
- plan id
- binary blob id
- optional signature blob id
- optional native hash
- native blob
- optional signature blob payload
- compatibility key
- artifact state
- created transaction id
- created timestamp

### Queue object

`JitQueueEntry` is the current deferred-compile object. It includes:
- queue id
- object UUID
- module id
- plan id
- compile request
- dedupe key
- reserved priority field

### Stats objects

Current code also proves two stats families:
- global runtime performance snapshot
- per-object performance snapshot

## Current persistence model

Current persistence is catalog-plus-TOAST based, not anonymous filesystem artifact storage.

The persisted artifact model stores:
- compatibility metadata in catalog rows
- native blob payload in TOAST-backed storage
- optional signature payload in TOAST-backed storage
- artifact stats in a separate artifact-stats catalog family

## Current state model

Current artifact state vocabulary currently includes at least:
- `QUEUED`
- `READY`
- `RETIRED`

Runtime selection only admits `READY` artifacts.
`RETIRED` artifacts are non-selectable.

## Current authority rule

Until a true compute-capsule object is proven in code, the native-artifact bundle described here is the authoritative compute-adjacent object model.
The specification must not pretend a richer catalog object already exists when it does not.

## Required reconstructed capsule model

A future promoted compute capsule may exist, but it must be layered on top of the current artifact truth rather than replacing it.

If promoted later, a compute capsule must explicitly bind:
- object UUID
- module id
- plan id
- canonical SBLR hash
- compatibility envelope
- compile policy envelope
- artifact generation set
- invalidation dependencies
- stats handles

## Non-negotiable layering rule

A future compute capsule must not weaken the current compatibility key or artifact verification model.
It may group artifacts, queue entries, and stats, but it must not redefine the runtime identity of native material.

## Current non-claims

Current code does not prove:
- a standalone compute-capsule DDL or catalog family
- one capsule containing multiple backends with negotiated runtime preference
- direct durable persistence of queue objects across restarts
- a separate capsule-level trust model independent of the artifact verification and compatibility key
