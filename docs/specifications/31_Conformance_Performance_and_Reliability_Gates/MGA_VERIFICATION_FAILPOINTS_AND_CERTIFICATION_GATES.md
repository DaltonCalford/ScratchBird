# MGA Verification, Failpoints, and Certification Gates

Status: current_authority_with_reconstructed_expansion

## Purpose
Define the deterministic failpoint, crash-window, and certification gate system
for transaction, savepoint, visibility, sweep, recovery, dormant reattach, and
restart replacement correctness.

## Mandatory Failpoint Families
- begin after txid allocation and before durable ACTIVE state
- savepoint rollback after heap undo and before index-backlog publication
- commit after data-page flush and before terminal inventory publication
- commit after terminal inventory publication and before acknowledgement
- checkpoint after horizon capture and before completion marker
- startup after inventory load and before ACTIVE to ABORTED normalization
- sweep after cursor persistence and before reclaim publication
- writeback `ENOSPC` after reserve-space threshold crossing
- dormant detach after token issue and before session parking publication
- dormant reattach after token admission and before replacement transaction mapping
- remote-management apply after assessment success and before committed target-generation publication

## Certification Suites
### T31-MGA-01 Publication and restart correctness
- no committed-visible split
- no surviving ambiguous ACTIVE transaction
- limbo state remains auditable
- restart replacement reattach never resurrects an uncommitted dead transaction image

### T31-MGA-02 Savepoint and statement restart correctness
- rollback-to-savepoint restores visible heads correctly
- read-consistency restart leaves no abandoned statement-visible versions
- DDL and DML rollback obey the same transaction restart contract

### T31-MGA-03 Sweep and reclaim ordering
- no visible version reclaimed
- no index cleanup published before matching heap proof
- shadow, archive, and derivative sinks remain derivative and do not alter reclaim truth

### T31-MGA-04 Recovery and degraded modes
- repairable classes quarantine correctly
- fatal classes refuse open
- writeback and disk-full degraded states are explicit and observable
- forced-write publication order remains authoritative after crash-window injection

### T31-MGA-05 Dormant detach and reattach correctness
- same-user token verification is enforced
- single-use token semantics are enforced
- session and transaction context replacement remains deterministic after restart
- prepared or limbo transactions remain auditable during reattach refusal or replacement

### T31-MGA-06 Management mutation publication correctness
- remote-management assess and apply boundaries do not bypass transaction publication
- security-policy epoch, permission-cache epoch, and target generation advance only on committed mutation
- management refusal, quarantine, and cancel paths leave prior committed state intact

## Required Evidence
Each applicable suite must emit:
- failpoint seed and trigger map
- visibility outcome diff
- savepoint rollback evidence where applicable
- checkpoint and recovery bundle
- sweep cursor and backlog report
- token issue and reattach evidence where dormant reattach is exercised
- security-policy epoch and permission-cache epoch evidence where security mutation is exercised
- instruction identity, target generation, and apply/refuse state evidence where management mutation is exercised

## Current Code-Backed Entry Points
Current maintained evidence exists through:
- `tests/unit/test_transaction_manager.cpp`
- `tests/unit/test_connection_manager.cpp`
- `tests/integration/test_security_phase3_5_rls_dml.cpp`
- `tests/integration/test_domain_e2e_scenarios.cpp`
- `tests/unit/test_manager_proxy_mcp.cpp`

## Reconstructed Required Expansion
The rebuilt canon additionally requires dedicated failpoint bundles for:
- dormant detach and reattach across server restart
- remote-management apply crash windows
- permission-cache invalidation publication failure windows
- shadow promotion and failback crash windows

## Pass Criteria
1. failpoint families produce deterministic terminal classifications
2. no gate permits WAL-style replay authority claims
3. every passing bundle proves MGA visibility, inventory, and page truth remained authoritative
4. every reattach and management mutation gate proves commit-bound publication order

## Cross-Section References
- `08_Transaction_Core/TRANSACTION_LIFECYCLE.md`
- `08_Transaction_Core/TRANSACTION_CONTEXT_MAPPING.md`
- `35_Durability_Crash_Recovery_and_Checkpoint_Model/DURABILITY_MODEL_AND_CORRECTNESS_BOUNDARY.md`
- `42_Failure_Model_and_Fault_Tolerance/FAILURE_MODEL_AND_FAULT_CLASSIFICATION.md`
