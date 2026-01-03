# SBCLUSTER-04: Security Bundle

## 1. Introduction

### 1.1 Purpose
The Security Bundle ensures that all nodes in a ScratchBird cluster enforce **identical security policies** for Row-Level Security (RLS), Column-Level Security (CLS), data masking, domain validation, and grants. This document specifies the structure, distribution, validation, and enforcement of the cluster-wide security configuration.

### 1.2 Scope
- Security Bundle structure and serialization
- Distribution via Raft control plane
- Hash computation and epoch binding
- Synchronization and convergence protocols
- Enforcement gates for distributed operations
- Version management and updates
- Disaster recovery and rollback

### 1.3 Related Documents
- **SBCLUSTER-01**: Cluster Configuration Epoch (CCE references security_bundle_hash)
- **SBCLUSTER-02**: Membership and Identity (node roles and authentication)
- **SBSEC-01 through SBSEC-09**: Single-node security specifications
- **SBCLUSTER-06**: Distributed Query (execution requires bundle hash match)

### 1.4 Terminology
- **Security Bundle**: Immutable snapshot of all security policies active in a cluster
- **Bundle Hash**: SHA-256 hash of the canonical serialization of a Security Bundle
- **Bundle Version**: Monotonically increasing version number (uint64_t)
- **Enforcement Gate**: Pre-execution check verifying all participating nodes share the same bundle hash
- **Control Plane Integrated**: Security Bundle distributed via Raft log, not external mechanisms

---

## 2. Architecture Overview

### 2.1 Design Principles

1. **Identical Enforcement**: All nodes MUST enforce the exact same security policies
2. **Engine Authority**: Security enforcement is engine-level, not parser-level
3. **Immutable Bundles**: Security Bundles are immutable; updates create new versions
4. **Atomic Transitions**: Cluster transitions atomically from bundle V_n to V_n+1
5. **Hash-Bound Execution**: Distributed queries require matching bundle hashes across nodes
6. **Raft-Delivered**: Bundles distributed via Raft log for strong consistency

### 2.2 Security Bundle Components

A Security Bundle contains:
- **RLS Policies**: Row-level security predicates per table
- **CLS Policies**: Column visibility and redaction rules
- **Masking Policies**: Data masking functions (PII, PCI, HIPAA)
- **Domain Constraints**: CHECK constraints, validation rules
- **Grants Matrix**: USER/AUTHKEY → OBJECT permissions
- **Audit Rules**: Which operations generate audit records

### 2.3 Distribution Model

```
┌─────────────────────────────────────────────────────────────┐
│                      RAFT LEADER                            │
│  1. Administrator executes: ALTER CLUSTER SECURITY BUNDLE   │
│  2. Leader appends SecurityBundleUpdate to Raft log         │
│  3. Leader replicates log entry to all VOTER nodes          │
└───────────────────┬─────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────┐
│                  RAFT LOG REPLICATION                       │
│  LogEntry {                                                 │
│    type: SECURITY_BUNDLE_UPDATE,                            │
│    version: 42,                                             │
│    bundle_hash: 0xABCD...,                                  │
│    bundle_data: <compressed serialized bundle>              │
│  }                                                          │
└───────────────────┬─────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────────┐
│              ALL NODES (VOTER + LEARNER)                    │
│  1. Receive log entry via Raft replication                  │
│  2. Deserialize and validate bundle                         │
│  3. Compute SHA-256 hash, verify match                      │
│  4. Apply bundle to local enforcement engine                │
│  5. Attest convergence to leader                            │
└─────────────────────────────────────────────────────────────┘
```

### 2.4 Enforcement Gates

Before executing a distributed query:
```
FOR each node N_i participating in query Q:
  REQUIRE N_i.current_bundle_hash == Q.initiator.current_bundle_hash

IF any node has mismatched hash:
  ABORT query with ERROR: SECURITY_BUNDLE_MISMATCH
```

---

## 3. Data Structures

### 3.1 SecurityBundle

```cpp
struct SecurityBundle {
    uint64_t version;                    // Monotonic version number
    UUID cluster_uuid;                   // Must match cluster identity
    timestamp_t created_at;              // Bundle creation timestamp

    // Component hashes (for incremental verification)
    bytes32 rls_policies_hash;
    bytes32 cls_policies_hash;
    bytes32 masking_policies_hash;
    bytes32 domain_constraints_hash;
    bytes32 grants_matrix_hash;
    bytes32 audit_rules_hash;

    // Security policy components
    RLSPolicySet rls_policies;           // Row-Level Security policies
    CLSPolicySet cls_policies;           // Column-Level Security policies
    MaskingPolicySet masking_policies;   // Data masking functions
    DomainConstraintSet domain_constraints; // Domain validation rules
    GrantsMatrix grants_matrix;          // Permission matrix
    AuditRuleSet audit_rules;            // Audit logging rules

    // Metadata
    UUID created_by_user_uuid;           // Administrator who created bundle
    string description;                  // Human-readable description

    // Signature (Ed25519)
    Signature bundle_signature;          // Signed by cluster administrators
};
```

### 3.2 RLSPolicySet

```cpp
struct RLSPolicy {
    UUID policy_uuid;                    // Unique policy identifier (v7)
    UUID table_uuid;                     // Target table
    string policy_name;                  // Human-readable name

    PolicyType type;                     // PERMISSIVE | RESTRICTIVE
    AppliesTo applies_to;                // SELECT | INSERT | UPDATE | DELETE | ALL

    string using_clause;                 // SQL predicate for SELECT/UPDATE/DELETE
    string with_check_clause;            // SQL predicate for INSERT/UPDATE

    UUID[] role_uuids;                   // Roles this policy applies to
    bool applies_to_table_owner;         // Does policy apply to table owner?
};

struct RLSPolicySet {
    RLSPolicy[] policies;                // Ordered by table_uuid, then policy_uuid
};
```

### 3.3 CLSPolicySet

```cpp
struct CLSPolicy {
    UUID policy_uuid;
    UUID table_uuid;
    UUID column_uuid;                    // Target column

    CLSAction action;                    // DENY | REDACT | MASK
    UUID[] granted_role_uuids;           // Roles that can see this column

    // For MASK action
    UUID masking_function_uuid;          // Optional: masking function to apply
};

struct CLSPolicySet {
    CLSPolicy[] policies;                // Ordered by table_uuid, column_uuid
};
```

### 3.4 MaskingPolicySet

```cpp
struct MaskingFunction {
    UUID function_uuid;
    string function_name;                // e.g., "mask_ssn", "mask_credit_card"

    DataType input_type;                 // Must match column type
    DataType output_type;                // Usually VARCHAR

    string function_body;                // SBLR bytecode or SQL expression
    MaskingClass classification;         // PII | PCI | HIPAA | GDPR | CUSTOM
};

enum MaskingClass {
    PII,        // Personally Identifiable Information
    PCI,        // Payment Card Industry
    HIPAA,      // Health Insurance Portability and Accountability Act
    GDPR,       // General Data Protection Regulation
    CUSTOM      // Custom masking
};

struct MaskingPolicySet {
    MaskingFunction[] functions;         // Ordered by function_uuid
};
```

### 3.5 DomainConstraintSet

```cpp
struct DomainConstraint {
    UUID domain_uuid;
    string domain_name;
    DataType base_type;

    string check_expression;             // SQL CHECK constraint
    bool nullable;
    string default_expression;           // Optional DEFAULT value

    CollationInfo collation;             // For string types
};

struct DomainConstraintSet {
    DomainConstraint[] constraints;      // Ordered by domain_uuid
};
```

### 3.6 GrantsMatrix

```cpp
struct Grant {
    UUID grant_uuid;                     // Unique grant identifier (v7)

    // Grantee (subject)
    GranteeType grantee_type;            // USER | AUTHKEY | ROLE
    UUID grantee_uuid;

    // Object (target)
    ObjectType object_type;              // TABLE | VIEW | PROCEDURE | DOMAIN | SCHEMA
    UUID object_uuid;

    // Permissions (bitfield)
    Permissions permissions;             // SELECT | INSERT | UPDATE | DELETE | EXECUTE | etc.

    // Grant options
    bool with_grant_option;              // Can grantee grant to others?
    UUID granted_by_uuid;                // Who granted this permission?
    timestamp_t granted_at;
};

enum Permissions : uint32_t {
    SELECT   = 0x0001,
    INSERT   = 0x0002,
    UPDATE   = 0x0004,
    DELETE   = 0x0008,
    EXECUTE  = 0x0010,
    USAGE    = 0x0020,
    CREATE   = 0x0040,
    DROP     = 0x0080,
    ALTER    = 0x0100,
    GRANT    = 0x0200,
    ALL      = 0xFFFF,
};

struct GrantsMatrix {
    Grant[] grants;                      // Ordered by grantee_uuid, object_uuid
};
```

### 3.7 AuditRuleSet

```cpp
struct AuditRule {
    UUID rule_uuid;
    string rule_name;

    AuditLevel level;                    // STATEMENT | OBJECT | COLUMN
    AuditScope scope;                    // DDL | DML | DCL | ALL

    UUID[] target_object_uuids;          // Empty = applies to all objects
    UUID[] target_user_uuids;            // Empty = applies to all users

    bool log_success;                    // Log successful operations?
    bool log_failure;                    // Log failed operations?
    bool log_parameters;                 // Log query parameters?
    bool log_result_count;               // Log number of rows affected?
};

enum AuditLevel {
    STATEMENT,   // Log entire statement
    OBJECT,      // Log per-object access
    COLUMN       // Log per-column access (for CLS)
};

enum AuditScope {
    DDL,         // CREATE, ALTER, DROP
    DML,         // SELECT, INSERT, UPDATE, DELETE
    DCL,         // GRANT, REVOKE
    ALL
};

struct AuditRuleSet {
    AuditRule[] rules;                   // Ordered by rule_uuid
};
```

---

## 4. Canonical Serialization and Hashing

### 4.1 Canonical Serialization Algorithm

```cpp
ByteBuffer serialize_canonical(const SecurityBundle& bundle) {
    ByteBuffer buf;

    // Header
    buf.append_u64_be(bundle.version);
    buf.append_bytes(bundle.cluster_uuid.bytes(), 16);
    buf.append_u64_be(bundle.created_at.micros_since_epoch());

    // Component sets (each serialized in deterministic order)
    buf.append_bytes(serialize_rls_policy_set(bundle.rls_policies));
    buf.append_bytes(serialize_cls_policy_set(bundle.cls_policies));
    buf.append_bytes(serialize_masking_policy_set(bundle.masking_policies));
    buf.append_bytes(serialize_domain_constraint_set(bundle.domain_constraints));
    buf.append_bytes(serialize_grants_matrix(bundle.grants_matrix));
    buf.append_bytes(serialize_audit_rule_set(bundle.audit_rules));

    // Metadata (description is UTF-8, length-prefixed)
    buf.append_bytes(bundle.created_by_user_uuid.bytes(), 16);
    buf.append_u32_be(bundle.description.length());
    buf.append_bytes(bundle.description.data(), bundle.description.length());

    // NOTE: Signature is NOT included in canonical serialization
    //       (signature is computed OVER the canonical bytes)

    return buf;
}
```

**Deterministic Ordering Rules**:
- All arrays sorted by UUID (lexicographic byte order)
- Strings encoded as UTF-8, length-prefixed (u32 big-endian)
- Booleans: 0x00 (false) or 0x01 (true)
- Timestamps: microseconds since Unix epoch (u64 big-endian)

### 4.2 Hash Computation

```cpp
bytes32 compute_bundle_hash(const SecurityBundle& bundle) {
    ByteBuffer canonical = serialize_canonical(bundle);
    return SHA256(canonical.data(), canonical.size());
}
```

### 4.3 Component Hash Computation

For incremental verification, each component has its own hash:

```cpp
bytes32 compute_rls_policies_hash(const RLSPolicySet& policies) {
    ByteBuffer buf = serialize_rls_policy_set(policies);
    return SHA256(buf.data(), buf.size());
}

// Similar for cls_policies_hash, masking_policies_hash, etc.
```

**Verification**:
```cpp
bool verify_component_hashes(const SecurityBundle& bundle) {
    return (bundle.rls_policies_hash == compute_rls_policies_hash(bundle.rls_policies))
        && (bundle.cls_policies_hash == compute_cls_policies_hash(bundle.cls_policies))
        && (bundle.masking_policies_hash == compute_masking_policies_hash(bundle.masking_policies))
        && (bundle.domain_constraints_hash == compute_domain_constraints_hash(bundle.domain_constraints))
        && (bundle.grants_matrix_hash == compute_grants_matrix_hash(bundle.grants_matrix))
        && (bundle.audit_rules_hash == compute_audit_rules_hash(bundle.audit_rules));
}
```

---

## 5. Bundle Distribution Protocol

### 5.1 Creating a New Security Bundle

**Administrative Command**:
```sql
-- Step 1: Draft a new security bundle (does not activate it)
CREATE SECURITY BUNDLE VERSION 43
  DESCRIPTION 'Add PII masking for customer table'
  FROM CURRENT;  -- Copy current bundle as starting point

-- Step 2: Make modifications (these update the draft)
CREATE RLS POLICY customer_region_filter
  ON customers
  FOR SELECT
  USING (region_id = current_user_region_id());

CREATE MASKING FUNCTION mask_ssn(VARCHAR(11)) RETURNS VARCHAR(11)
  AS 'XXXX-XX-' || SUBSTRING($1, 8, 4)
  CLASSIFICATION PII;

-- Step 3: Publish the bundle to the cluster
PUBLISH SECURITY BUNDLE VERSION 43;
```

### 5.2 Raft Log Entry

When `PUBLISH SECURITY BUNDLE` is executed:

```cpp
struct RaftLogEntry_SecurityBundleUpdate {
    RaftLogEntryType type = SECURITY_BUNDLE_UPDATE;
    uint64_t log_index;
    uint64_t term;

    SecurityBundle bundle;               // Full bundle payload
    bytes32 bundle_hash;                 // SHA-256 hash for quick verification

    UUID initiator_node_uuid;            // Node that initiated the update
    UUID initiator_user_uuid;            // Administrator who executed PUBLISH
    timestamp_t initiated_at;
};
```

**Raft Replication**:
1. Leader appends log entry to local Raft log
2. Leader replicates to all VOTER nodes
3. Once majority acknowledges, entry is **committed**
4. Leader broadcasts commit notification to all nodes (VOTER + LEARNER)

### 5.3 Applying the Bundle

Each node, upon receiving a committed SECURITY_BUNDLE_UPDATE log entry:

```cpp
void apply_security_bundle_update(const RaftLogEntry_SecurityBundleUpdate& entry) {
    // 1. Deserialize bundle
    SecurityBundle bundle = entry.bundle;

    // 2. Validate bundle structure
    if (!validate_bundle_structure(bundle)) {
        log_fatal("Invalid security bundle structure in Raft log - cluster inconsistent!");
        abort();  // This should NEVER happen (leader validates before appending)
    }

    // 3. Verify hash
    bytes32 computed_hash = compute_bundle_hash(bundle);
    if (computed_hash != entry.bundle_hash) {
        log_fatal("Security bundle hash mismatch - cluster inconsistent!");
        abort();
    }

    // 4. Verify component hashes
    if (!verify_component_hashes(bundle)) {
        log_fatal("Security bundle component hash mismatch!");
        abort();
    }

    // 5. Apply to local enforcement engine
    //    This is an ATOMIC operation - either all policies apply or none
    transaction_guard txn(catalog_db);

    // Clear old policies
    enforcement_engine.clear_all_policies();

    // Load new policies
    enforcement_engine.load_rls_policies(bundle.rls_policies);
    enforcement_engine.load_cls_policies(bundle.cls_policies);
    enforcement_engine.load_masking_policies(bundle.masking_policies);
    enforcement_engine.load_domain_constraints(bundle.domain_constraints);
    enforcement_engine.load_grants_matrix(bundle.grants_matrix);
    enforcement_engine.load_audit_rules(bundle.audit_rules);

    // Update current bundle metadata
    current_bundle_version = bundle.version;
    current_bundle_hash = computed_hash;

    txn.commit();

    // 6. Attest convergence to leader
    send_bundle_convergence_attestation(entry.bundle_hash, bundle.version);
}
```

### 5.4 Convergence Attestation

After applying a bundle, each node sends an attestation to the leader:

```cpp
struct BundleConvergenceAttestation {
    UUID node_uuid;
    UUID cluster_uuid;
    uint64_t bundle_version;             // Version node has applied
    bytes32 bundle_hash;                 // Hash of applied bundle
    timestamp_t applied_at;
    Signature node_signature;            // Ed25519 signature
};
```

**Leader tracks convergence**:
```cpp
struct ConvergenceTracker {
    uint64_t target_bundle_version;
    bytes32 target_bundle_hash;

    map<UUID, BundleConvergenceAttestation> attestations;  // node_uuid -> attestation

    bool is_converged() const {
        // All ENABLED nodes must have matching bundle version and hash
        for (const Node& node : cluster.get_enabled_nodes()) {
            auto it = attestations.find(node.node_uuid);
            if (it == attestations.end()) return false;  // Missing attestation

            const auto& att = it->second;
            if (att.bundle_version != target_bundle_version) return false;
            if (att.bundle_hash != target_bundle_hash) return false;
        }
        return true;
    }
};
```

**Convergence Timeout**: If convergence is not achieved within 60 seconds, the cluster enters a degraded state where distributed queries are rejected.

---

## 6. Enforcement Gates

### 6.1 Pre-Query Validation

Before executing a distributed query, the coordinator MUST verify all participating nodes have the same security bundle:

```cpp
Status validate_security_bundle_consistency(
    const DistributedQuery& query,
    const vector<UUID>& participating_node_uuids
) {
    bytes32 coordinator_bundle_hash = get_current_bundle_hash();

    for (const UUID& node_uuid : participating_node_uuids) {
        // Query node for its current bundle hash
        BundleHashResponse response = rpc_client.get_bundle_hash(node_uuid);

        if (response.bundle_hash != coordinator_bundle_hash) {
            return Status::SECURITY_BUNDLE_MISMATCH(
                fmt::format("Node {} has bundle hash {}, coordinator has {}",
                    node_uuid.to_string(),
                    hex(response.bundle_hash),
                    hex(coordinator_bundle_hash))
            );
        }
    }

    return Status::OK();
}
```

**Error Handling**: If bundle hashes don't match:
- Query is **immediately aborted**
- Error code: `SECURITY_BUNDLE_MISMATCH`
- User receives error message with details
- Administrator is alerted (cluster may be in inconsistent state)

### 6.2 Query Annotation

All distributed queries carry the initiator's bundle hash:

```cpp
struct DistributedQueryRequest {
    UUID query_uuid;
    UUID initiator_node_uuid;
    bytes32 security_bundle_hash;        // Initiator's bundle hash

    QueryPlan query_plan;
    // ... other fields
};
```

Each participating node validates:
```cpp
if (request.security_bundle_hash != local_bundle_hash) {
    return ERROR_SECURITY_BUNDLE_MISMATCH;
}
```

---

## 7. Bundle Updates and Versioning

### 7.1 Version Numbering

- Bundle versions are **monotonically increasing** uint64_t values
- Version 0 is reserved (never used)
- Initial cluster bootstrap creates version 1 (empty bundle)
- Each `PUBLISH SECURITY BUNDLE` increments version by 1

### 7.2 Bundle History

The cluster maintains a **history of Security Bundles** in the catalog:

```sql
CREATE TABLE sys.security_bundle_history (
    bundle_version BIGINT PRIMARY KEY,
    bundle_hash BYTES32 NOT NULL,
    bundle_data BLOB NOT NULL,           -- Compressed serialized bundle
    created_at TIMESTAMP NOT NULL,
    created_by_user_uuid UUID NOT NULL,
    description VARCHAR(1000),
    applied_at TIMESTAMP,                -- When bundle became active
    superseded_at TIMESTAMP              -- When bundle was replaced (NULL if current)
);
```

**Retention Policy**:
- Current bundle: retained forever
- Last 10 versions: retained forever (for rollback)
- Older versions: retained for 90 days, then archived/deleted

### 7.3 Rollback Procedure

To roll back to a previous bundle version:

```sql
-- Rollback to version 41
ROLLBACK SECURITY BUNDLE TO VERSION 41;
```

**Implementation**:
1. Leader retrieves bundle version 41 from `sys.security_bundle_history`
2. Creates a new bundle version (e.g., 44) with the same content as version 41
3. Publishes the new bundle via Raft log
4. All nodes apply the rolled-back policies

**Why not reuse version 41?** Because version numbers must be monotonic for Raft ordering.

---

## 8. Security Considerations

### 8.1 Bundle Signing

Security Bundles are signed by cluster administrators using Ed25519:

```cpp
Signature sign_bundle(const SecurityBundle& bundle, const Ed25519PrivateKey& admin_key) {
    bytes32 bundle_hash = compute_bundle_hash(bundle);
    return admin_key.sign(bundle_hash);
}

bool verify_bundle_signature(
    const SecurityBundle& bundle,
    const Signature& signature,
    const Ed25519PublicKey& admin_pubkey
) {
    bytes32 bundle_hash = compute_bundle_hash(bundle);
    return admin_pubkey.verify(bundle_hash, signature);
}
```

**Multi-Administrator Signing** (for high-security clusters):
- Bundles can require M-of-N administrator signatures
- Similar to CCE multi-admin signing (SBCLUSTER-01)

### 8.2 Unauthorized Modification Prevention

- Security Bundles are distributed **only via Raft log**
- No direct file-based or network-based bundle injection
- Raft's cryptographic integrity (via leader's signature) prevents tampering
- Each node validates hash independently

### 8.3 Denial of Service Prevention

**Large Bundle Attack**:
- Bundles have a maximum size limit (default: 100 MB compressed)
- Bundles exceeding the limit are rejected before Raft replication

**Bundle Churn Attack**:
- Rate limit on `PUBLISH SECURITY BUNDLE` (max 1 per minute)
- Only cluster administrators can publish bundles

---

## 9. Operational Procedures

### 9.1 Initial Cluster Bootstrap

When creating a new cluster, an empty Security Bundle (version 1) is created:

```cpp
SecurityBundle create_initial_bundle(const UUID& cluster_uuid) {
    SecurityBundle bundle;
    bundle.version = 1;
    bundle.cluster_uuid = cluster_uuid;
    bundle.created_at = now();

    // All policy sets are empty
    bundle.rls_policies = RLSPolicySet{};
    bundle.cls_policies = CLSPolicySet{};
    bundle.masking_policies = MaskingPolicySet{};
    bundle.domain_constraints = DomainConstraintSet{};
    bundle.grants_matrix = GrantsMatrix{};
    bundle.audit_rules = AuditRuleSet{};

    bundle.created_by_user_uuid = SYSTEM_UUID;
    bundle.description = "Initial empty security bundle";

    return bundle;
}
```

### 9.2 Adding a New Node

When a new node joins the cluster:
1. Node receives full Raft log replay (including all SECURITY_BUNDLE_UPDATE entries)
2. Node applies bundles in order (version 1 → version N)
3. Node ends up with the current bundle installed
4. Node sends convergence attestation
5. Node becomes ENABLED only after attesting convergence

### 9.3 Monitoring Bundle Consistency

**Cluster-wide hash check** (periodic, every 5 minutes):
```cpp
void check_bundle_consistency() {
    map<bytes32, vector<UUID>> hash_to_nodes;

    for (const Node& node : cluster.get_enabled_nodes()) {
        BundleHashResponse resp = rpc_client.get_bundle_hash(node.node_uuid);
        hash_to_nodes[resp.bundle_hash].push_back(node.node_uuid);
    }

    if (hash_to_nodes.size() > 1) {
        // CRITICAL: Cluster has inconsistent security bundles!
        log_critical("Security bundle inconsistency detected:");
        for (const auto& [hash, nodes] : hash_to_nodes) {
            log_critical("  Hash {}: nodes {}", hex(hash), join(nodes, ", "));
        }

        alert_administrators("SECURITY_BUNDLE_INCONSISTENCY");

        // Enter degraded mode: reject distributed queries
        cluster_state.set_degraded(DegradedReason::SECURITY_BUNDLE_INCONSISTENCY);
    }
}
```

### 9.4 Emergency Bundle Replacement

If a bundle is found to have a security flaw:

```sql
-- 1. Create new bundle with fix
CREATE SECURITY BUNDLE VERSION 45
  DESCRIPTION 'EMERGENCY: Fix RLS bypass vulnerability CVE-2026-XXXX'
  FROM CURRENT;

-- 2. Apply fix (e.g., correct RLS policy)
ALTER RLS POLICY customer_region_filter
  ON customers
  SET USING (region_id = current_user_region_id() AND is_active = TRUE);

-- 3. Publish immediately
PUBLISH SECURITY BUNDLE VERSION 45;

-- 4. Verify convergence
SELECT * FROM sys.security_bundle_convergence;
```

**Expected convergence time**: < 30 seconds for a 10-node cluster.

---

## 10. Testing Procedures

### 10.1 Functional Tests

**Test: Bundle Serialization Determinism**
```cpp
TEST(SecurityBundle, SerializationDeterminism) {
    SecurityBundle bundle = create_test_bundle();

    bytes32 hash1 = compute_bundle_hash(bundle);
    bytes32 hash2 = compute_bundle_hash(bundle);

    EXPECT_EQ(hash1, hash2);  // Same bundle = same hash
}
```

**Test: Component Hash Verification**
```cpp
TEST(SecurityBundle, ComponentHashVerification) {
    SecurityBundle bundle = create_test_bundle();

    // Tamper with RLS policies
    bundle.rls_policies.policies.push_back(create_rogue_policy());

    // Component hash should NOT match
    EXPECT_FALSE(verify_component_hashes(bundle));
}
```

**Test: Bundle Distribution**
```cpp
TEST(SecurityBundle, RaftDistribution) {
    auto cluster = create_test_cluster(3);  // 3-node cluster

    // Leader publishes new bundle
    SecurityBundle bundle = create_test_bundle();
    cluster.leader().publish_security_bundle(bundle);

    // Wait for convergence
    ASSERT_TRUE(cluster.wait_for_bundle_convergence(bundle.version, 30s));

    // Verify all nodes have same bundle hash
    bytes32 expected_hash = compute_bundle_hash(bundle);
    for (Node& node : cluster.nodes()) {
        EXPECT_EQ(node.get_current_bundle_hash(), expected_hash);
    }
}
```

### 10.2 Security Tests

**Test: Enforcement Gate Blocks Mismatched Bundles**
```cpp
TEST(SecurityBundle, EnforcementGateMismatch) {
    auto cluster = create_test_cluster(3);

    // Artificially create inconsistency (node 2 has old bundle)
    cluster.node(1).apply_bundle(create_test_bundle_v1());
    cluster.node(2).apply_bundle(create_test_bundle_v2());  // Different bundle!
    cluster.node(3).apply_bundle(create_test_bundle_v1());

    // Attempt distributed query involving all 3 nodes
    DistributedQuery query = create_cross_shard_query({node1, node2, node3});

    Status result = cluster.coordinator().execute_distributed_query(query);

    EXPECT_EQ(result.code(), StatusCode::SECURITY_BUNDLE_MISMATCH);
    EXPECT_THAT(result.message(), HasSubstr("Node"));
    EXPECT_THAT(result.message(), HasSubstr("has bundle hash"));
}
```

**Test: Unauthorized Bundle Modification**
```cpp
TEST(SecurityBundle, UnauthorizedModification) {
    auto cluster = create_test_cluster(1);

    // Attempt to directly modify bundle file (bypassing Raft)
    SecurityBundle rogue_bundle = create_rogue_bundle();

    // This should FAIL (bundles only accepted via Raft)
    EXPECT_THROW(
        cluster.node(0).force_apply_bundle(rogue_bundle),
        UnauthorizedOperationException
    );
}
```

### 10.3 Performance Tests

**Test: Bundle Convergence Time**
```cpp
TEST(SecurityBundle, ConvergenceTime) {
    auto cluster = create_test_cluster(10);  // 10-node cluster

    SecurityBundle bundle = create_large_bundle(10000);  // 10k policies

    auto start = now();
    cluster.leader().publish_security_bundle(bundle);

    ASSERT_TRUE(cluster.wait_for_bundle_convergence(bundle.version, 60s));

    auto elapsed = now() - start;
    EXPECT_LT(elapsed, 30s);  // Should converge in < 30 seconds
}
```

---

## 11. Examples

### 11.1 Complete Bundle Update Workflow

**Scenario**: Add RLS policy for multi-tenant SaaS application.

**Step 1: Administrator drafts bundle**
```sql
-- Create new draft bundle (version 10)
CREATE SECURITY BUNDLE VERSION 10
  DESCRIPTION 'Add tenant isolation for orders table'
  FROM CURRENT;
```

**Step 2: Add RLS policy to draft**
```sql
CREATE RLS POLICY tenant_isolation_orders
  ON orders
  FOR ALL
  USING (tenant_id = current_setting('app.current_tenant_id')::UUID)
  WITH CHECK (tenant_id = current_setting('app.current_tenant_id')::UUID);
```

**Step 3: Test draft locally (optional)**
```sql
-- Simulate enforcement with draft bundle
SET scratchbird.test_bundle_version = 10;

-- Run test queries to verify RLS works correctly
SELECT * FROM orders WHERE order_id = 12345;  -- Should only see own tenant's orders
```

**Step 4: Publish bundle to cluster**
```sql
PUBLISH SECURITY BUNDLE VERSION 10;
```

**Step 5: Monitor convergence**
```sql
-- Check convergence status
SELECT
    node_uuid,
    bundle_version,
    applied_at,
    CASE
        WHEN bundle_version = 10 THEN 'CONVERGED'
        ELSE 'PENDING'
    END as status
FROM sys.security_bundle_convergence
ORDER BY node_uuid;
```

**Expected output**:
```
node_uuid                             | bundle_version | applied_at           | status
--------------------------------------|----------------|----------------------|----------
01234567-89ab-cdef-0123-456789abcdef | 10             | 2026-01-02 14:30:12  | CONVERGED
12345678-9abc-def0-1234-56789abcdef0 | 10             | 2026-01-02 14:30:13  | CONVERGED
23456789-abcd-ef01-2345-6789abcdef01 | 10             | 2026-01-02 14:30:14  | CONVERGED
```

**Step 6: Verify enforcement**
```sql
-- Run query as tenant A
SET app.current_tenant_id = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa';
SELECT COUNT(*) FROM orders;  -- Returns only tenant A's orders

-- Run same query as tenant B
SET app.current_tenant_id = 'bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb';
SELECT COUNT(*) FROM orders;  -- Returns only tenant B's orders
```

### 11.2 Rollback Example

**Scenario**: Bundle version 15 introduced a bug; rollback to version 14.

```sql
-- Rollback (creates new version 16 with content of version 14)
ROLLBACK SECURITY BUNDLE TO VERSION 14;

-- Verify rollback
SELECT
    bundle_version,
    description,
    applied_at
FROM sys.security_bundle_history
ORDER BY bundle_version DESC
LIMIT 3;
```

**Output**:
```
bundle_version | description                        | applied_at
---------------|------------------------------------|-----------------------
16             | ROLLBACK to version 14             | 2026-01-02 15:45:00
15             | Add column masking (BUGGY)         | 2026-01-02 15:30:00
14             | Add audit rules for compliance     | 2026-01-02 14:00:00
```

---

## 12. References

### 12.1 Internal References
- **SBCLUSTER-01**: Cluster Configuration Epoch
- **SBCLUSTER-02**: Membership and Identity
- **SBSEC-05**: Row-Level Security (RLS) Specification
- **SBSEC-06**: Column-Level Security (CLS) Specification
- **SBSEC-07**: Data Masking Specification

### 12.2 External References
- **PostgreSQL Row Security Policies**: https://www.postgresql.org/docs/current/ddl-rowsecurity.html
- **Oracle Virtual Private Database (VPD)**: Oracle security whitepapers
- **SQL Server Row-Level Security**: https://learn.microsoft.com/en-us/sql/relational-databases/security/row-level-security
- **NIST SP 800-53**: Security and Privacy Controls (AC-3: Access Enforcement)

### 12.3 Research Papers
- *"Granular Access Control in Database Management Systems"* - Chen et al., VLDB 2018
- *"Scalable Policy Distribution in Distributed Databases"* - Zhang et al., SIGMOD 2020

---

## 13. Revision History

| Version | Date       | Author       | Changes                                    |
|---------|------------|--------------|--------------------------------------------|
| 1.0     | 2026-01-02 | D. Calford   | Initial comprehensive specification        |

---

**Document Status**: DRAFT (Beta Specification Phase)
**Next Review**: Before Beta Implementation Phase
**Approval Required**: Chief Architect, Security Lead, Cluster Engineering Lead
