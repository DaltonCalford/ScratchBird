# Section 19 Test Contract

Status: current_authority

## Certification lanes

1. Fail-closed bootstrap sequencing.
2. Rejection of invalid or unreadable key and certificate material.
3. Authentication handshake success and refusal across the admitted Beta 1
   method families: password or compatibility password, MD5 compatibility where
   a shipped protocol still requires it, SCRAM, token or authkey, peer,
   certificate mTLS, manager-control token flow, and MFA overlays on top of
   those primary methods.
4. Signed-module admission and unsigned-module rejection.
5. Channel hardening and downgrade refusal where protection is required.
6. Audit evidence production, redaction, and privileged forensic access gating.
7. Rotation and reload behavior for the currently supported refresh paths:
   TLSContext-backed certificate reload on explicitly configured current
   channels and explicit bootstrap-managed database-key rotation.
8. Security-definer and `WITH CHECK OPTION` paths fail closed whenever a
   stronger integrated privilege or predicate backend is unavailable.
9. Bootstrap-authority coverage for local `SysArch`, named `sysadmin`, and
   cluster-admin supersession semantics where those surfaces are admitted by the
   release program.
10. Beta 2 at-rest encryption proves:
   - live page, index, overflow, backup, and archive encryption policy
   - key-generation publication and refusal behavior
   - rewrap-only versus page-rewrite rekey classification
11. Beta 2 protected-query encryption proves:
   - protected profile classification
   - deterministic equality admission and refusal of unsupported operators
   - enclave attestation gating and refusal behavior
   - protected rotation and searchable-token policy metadata
12. Beta 2 enterprise identity federation proves:
   - provider metadata publication
   - token-validation refusal behavior
   - claim-to-role mapping determinism
   - preserved local recovery path
13. Beta 2 RLS and masking closure proves:
   - policy-order determinism
   - mixed RLS and masking cache invalidation
   - privileged bypass and force-RLS interaction disclosure

## Refusal rules

- A build that exposes listeners before required security readiness fails certification.
- A build that accepts unsigned authentication extensions fails certification.
- A build that treats bootstrap secrets as catalog-owned mutable data fails certification.
- A build that silently treats non-admitted enterprise or cluster security
  surfaces as supported Beta 1 runtime behavior fails certification.
- A Beta 2 build that claims transparent at-rest encryption or protected-query
  execution without the matching metadata, refusal, and observability evidence
  fails certification.
