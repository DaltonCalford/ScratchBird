# Section 31 Test Contract

Status: current_authority_with_reconstructed_expansion

Section `31` certification requires:

1. Maintained gate runners for every gate family claimed as current release authority.
2. Maintained evidence artifacts with stable schema, retention rules, and replay identifiers.
3. Canonical diff and deterministic replay rules for conformance classifications.
4. Benchmark corpus, environment, and acceptance-threshold definition for every performance claim.
5. Reliability and restart scenarios that exercise the current MGA recovery model.
6. Negative tests or explicit exclusions for unsupported or non-maintained gate families.
7. Security, authorization, and remote-management lanes must distinguish:
   - parser or client acceptance
   - engine authorization
   - metadata publication
   - audit emission
   - operator-visible result contract
8. Derivative-lane gates must prove:
   - local MGA durability health remains separable from derivative shipping health
   - queue-state outputs are emitted with stable schema
   - retry and quarantine preserve source identity and sequence continuity
   - shadow-group readiness and degraded state are observable and deterministic
9. Restore, promotion, and failback boundaries must prove:
   - continuity markers are emitted
   - promoted-shadow and failback procedures remain page-image and reconciliation based
   - no derivative sink is misclassified as restart authority
10. Security and management mutation gates must prove:
   - inspection privilege is distinct from mutation privilege
   - admin-SQL and management result rows are deterministic
   - policy epoch and permission-cache invalidation are commit-bound
   - remote management actions are queued, assessed, applied, refused, quarantined, or cancelled with stable state codes
11. Any Beta 2 optimizer maturity claim must prove the matching optimizer gate family set for:
   - planner unification and physical-property search
   - family-trust and mixed-workload competition
   - plan-store, baseline, and regression governance
   - adaptive processing and memory-grant correction
   - parameter-sensitive multi-plan and workload-feedback governance
12. Any Beta 2 production workload capture and replay claim must prove:
   - stable capture-pack schema
   - privacy/redaction policy validation
   - deterministic replay modes
   - result, plan, and latency divergence reporting
   - restore or rehearsal target provenance
13. Any Beta 2 high-performance OLTP claim must prove:
   - service-class disclosure
   - prepared fast-path hit or miss disclosure
   - latency distribution and contention artifacts
   - hotspot mitigation or refusal behavior
14. Any Beta 2 distributed-query claim must prove:
   - fragment graph disclosure
   - motion-class disclosure
   - remote-fragment failure and retry artifacts
   - exchange spill and backpressure metrics
15. Any Beta 2 shard-management claim must prove:
   - split, merge, or move epoch artifacts
   - cutover-fence publication
   - read-routing-mode disclosure
   - recovery classification for interrupted workflows
16. Any Beta 2 OLAP or cube claim must prove:
   - vector-scan and pruning artifacts
   - acceleration or rewrite disclosure
   - refresh-job lineage and freshness artifacts
   - mixed HTAP interference measurements

A file in this section may document a boundary for an unsupported gate family. That does not create a required current execution lane unless the file explicitly promotes it to maintained current authority.
