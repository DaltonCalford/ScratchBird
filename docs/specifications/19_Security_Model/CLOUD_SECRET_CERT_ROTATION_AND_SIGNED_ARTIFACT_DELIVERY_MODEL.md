# Cloud Secret, Certificate, and Signed-Artifact Delivery Model

Status: reconstructed_required
Section: 19_Security_Model

## Purpose

Define the cloud-facing lifecycle for secrets, certificates, keys, and signed runtime artifacts.

## Beta 1 local-engine scope

For package `06`, the admitted Beta 1 subset is bounded to single-node and
local-engine deployments that:

- source secrets and certificates from explicit externalized inputs
- validate signed binaries, plugins, and UDRs before use
- support certificate reload only on configured runtime channels whose current
  lifecycle already exposes in-place refresh
- support explicit bootstrap-managed database-key rotation as a controlled
  maintenance path

Nothing in this file grants implicit Beta 1 support for clustered secret
distribution, quorum unseal, or distributed trust rollout.

## Governing rules

- secret and certificate material must be externalized
- no deployment may depend on hardcoded secret or certificate payloads
- signed binaries, signed plugins, signed UDRs, and controlled release channels are first-class cloud requirements

## Secret and certificate sources

Supported secret or certificate source classes are policy-driven and may include:

- local protected files
- mounted secret volumes
- cloud or external secret managers
- HSM or KMS-backed material references

The deployment profile must identify which source classes are allowed. Canonical cloud support does not require one specific vendor.

## Rotation model

Rotation must support, by policy:

- manager-facing TLS material
- listener-facing TLS material
- internal service certificates where used
- signing trust roots for binaries, plugins, and UDRs
- remote-management authentication material

Rotation must be coordinated so that:

- readiness reflects generation mismatch or missing material
- existing traffic is drained or revalidated according to the owning layer contract
- stale trust anchors are retired in a controlled order

Current Beta 1 implementation closure is narrower than the full policy matrix:

- TLSContext-backed configured channels may reload certificates in place where
  their runtime lifecycle already supports reload
- in-place reload must refresh cached operator-facing certificate metadata from
  the reloaded TLS context so readiness and diagnostics do not report stale
  certificate identity after a successful reload
- channels without an explicit reload lifecycle require restart rather than
  guessed hot-apply behavior
- protected-store key rotation is an explicit controlled operation, not an
  always-hot reload promise

## Signed artifact delivery

Cloud deployment channels must preserve verification of:

- engine binaries
- server and manager binaries
- plugins
- UDRs
- deployment bundles and update artifacts where signing is required by policy

Unsigned or untrusted artifacts must fail closed.

## Unsupported Beta 1 boundary

Cluster-managed shard rollout, quorum unseal, distributed trust propagation,
and cluster-coordinated certificate rotation remain explicit non-Beta 1
surfaces for package `06` and must fail closed rather than approximating a
cluster runtime.

## Beta 2 cluster extension

Where cluster security is enabled, cluster-managed secret or key material may be distributed as bounded fragments or coordinated secrets according to policy, but the following are mandatory:

- fragment custody rules
- reconstruction quorum rules
- operator audit trail
- no silent promotion of partial secret material into active trust
