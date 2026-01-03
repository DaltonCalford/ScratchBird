# SBCLUSTER-03: CA Policy and Rotation

**Document ID**: SBCLUSTER-03
**Version**: 1.0
**Status**: Beta Specification
**Date**: January 2026
**Dependencies**: SBCLUSTER-00, SBCLUSTER-01, SBCLUSTER-02

---

## 1. Introduction

### 1.1 Purpose

This document defines the **Certificate Authority (CA) policy framework** for ScratchBird clusters, including CA rotation with zero downtime using an overlap-then-cutover model, peer observation requirements, and operational procedures for CA lifecycle management.

### 1.2 Scope

This specification covers:
- CA policy structure and versioning
- Overlap model for CA rotation
- Peer observation requirements during rotation
- Cutover procedures and safety gates
- Emergency CA revocation
- Certificate validation rules
- Integration with Cluster Configuration Epochs

### 1.3 Related Documents

| Document ID | Title |
|-------------|-------|
| SBCLUSTER-01 | Cluster Configuration Epoch |
| SBCLUSTER-02 | Membership and Identity |
| SBSEC-04 | Encryption and Key Management |

---

## 2. CA Policy Structure

### 2.1 CA Policy Bundle

A **CA Policy Bundle** defines which Certificate Authorities are trusted by the cluster:

```cpp
struct CAPolicy {
    // Versioning
    uint32_t policy_version;     // Monotonically increasing
    uint64_t epoch_number;       // Which CCE this belongs to
    bytes32 policy_hash;         // SHA-256 of canonical policy

    // Trust anchors
    TrustedCA[] trusted_cas;     // Ordered list of trusted CAs

    // Validation rules
    ValidationRules validation;

    // Metadata
    timestamp_t created_at;
    timestamp_t valid_from;      // When this policy becomes active
    timestamp_t valid_until;     // Optional expiry (for overlap)

    // Phase indicator
    PolicyPhase phase;           // EXCLUSIVE, OVERLAP, CUTOVER
};

enum PolicyPhase : uint8_t {
    EXCLUSIVE = 0,    // Single CA trusted
    OVERLAP = 1,      // Multiple CAs trusted (rotation in progress)
    CUTOVER = 2,      // Transitioning from overlap to new exclusive
};

struct TrustedCA {
    string ca_name;              // Human-readable name
    bytes ca_certificate;        // X.509 CA certificate (DER)
    bytes32 cert_fingerprint;    // SHA-256 of ca_certificate

    // Trust metadata
    bool is_primary;             // Primary CA for new cert issuance
    timestamp_t trusted_from;    // When this CA became trusted
    timestamp_t trusted_until;   // When trust expires (for rotation)

    // Validation
    uint32_t max_path_length;    // Max chain depth (default: 1)
    string[] permitted_dns;      // DNS constraints (optional)
    string[] permitted_ips;      // IP constraints (optional)
};

struct ValidationRules {
    // Certificate requirements
    uint32_t min_key_size_rsa;     // Minimum RSA key size (default: 2048)
    uint32_t min_key_size_ec;      // Minimum EC key size (default: 256)

    // Allowed algorithms
    string[] allowed_sig_algs;     // e.g., ["Ed25519", "ECDSA-P256", "RSA-SHA256"]

    // Validity constraints
    uint32_t max_validity_days;    // Max certificate validity (default: 90)

    // Revocation checking
    bool require_crl_check;        // Check CRL if present
    bool require_ocsp_check;       // Check OCSP if present
    string[] crl_urls;             // CRL distribution points
    string[] ocsp_urls;            // OCSP responder URLs

    // Certificate extensions
    bool require_key_usage;        // Require KeyUsage extension
    bool require_ext_key_usage;    // Require ExtendedKeyUsage
    bool require_uri_san;          // Require URI SAN with sb:// scheme
};
```

### 2.2 Policy Hash Computation

```cpp
bytes32 compute_ca_policy_hash(const CAPolicy& policy) {
    ByteBuffer canonical;

    // Version
    canonical.append_u32_be(policy.policy_version);
    canonical.append_u64_be(policy.epoch_number);

    // Trusted CAs (sorted by fingerprint for determinism)
    auto sorted_cas = policy.trusted_cas;
    std::sort(sorted_cas.begin(), sorted_cas.end(),
              [](const TrustedCA& a, const TrustedCA& b) {
                  return a.cert_fingerprint < b.cert_fingerprint;
              });

    canonical.append_u32_be(sorted_cas.size());
    for (const auto& ca : sorted_cas) {
        canonical.append_bytes(ca.cert_fingerprint, 32);
        canonical.append_u8(ca.is_primary ? 1 : 0);
        canonical.append_i64_be(ca.trusted_from.nanos_since_epoch());
        canonical.append_i64_be(ca.trusted_until.nanos_since_epoch());
    }

    // Validation rules (canonical serialization)
    canonical.append(serialize_canonical(policy.validation));

    // Phase
    canonical.append_u8(static_cast<uint8_t>(policy.phase));

    return SHA256(canonical);
}
```

---

## 3. CA Rotation Model: Overlap and Cutover

### 3.1 Rotation Phases

The CA rotation process consists of **three phases**:

```
┌────────────────────────────────────────────────────────────┐
│                  CA ROTATION TIMELINE                       │
├────────────────────────────────────────────────────────────┤
│                                                             │
│  Phase 1: EXCLUSIVE (CA_A)                                  │
│  ══════════════════════════════════════                     │
│  │                                                          │
│  │  All nodes use CA_A certificates                        │
│  │  Only CA_A trusted                                      │
│  │                                                          │
│  └──────────────────────────────────────────────────────►  │
│                                                             │
│  Phase 2: OVERLAP (CA_A + CA_B)                            │
│  ══════════════════════════════════════                     │
│  │                                                          │
│  │  Both CA_A and CA_B trusted                             │
│  │  Nodes gradually transition to CA_B                     │
│  │  Peer observation ensures all nodes have CA_B certs     │
│  │                                                          │
│  └──────────────────────────────────────────────────────►  │
│                                                             │
│  Phase 3: EXCLUSIVE (CA_B)                                  │
│  ══════════════════════════════════════                     │
│  │                                                          │
│  │  All nodes use CA_B certificates                        │
│  │  Only CA_B trusted (CA_A no longer accepted)            │
│  │                                                          │
│  └──────────────────────────────────────────────────────►  │
│                                                             │
└────────────────────────────────────────────────────────────┘
```

### 3.2 Overlap Phase: Detailed Steps

**Step 1: Administrator prepares overlap policy**

```
Current State: Epoch N with CA_A (EXCLUSIVE)

Administrator creates Epoch N+1:
  CA Policy:
    - Phase: OVERLAP
    - Trusted CAs:
      - CA_A: is_primary=false, trusted_until=now+48h
      - CA_B: is_primary=true, trusted_until=null (indefinite)
    - All other configs unchanged
```

**Step 2: Publish overlap epoch**

```
1. Administrator publishes Epoch N+1
2. Raft commits epoch to log
3. All nodes receive epoch via replication
4. Nodes activate overlap policy:
   - Trust both CA_A and CA_B
   - Begin accepting certificates from either CA
5. Nodes write ConvergenceAttestation (SBCLUSTER-01)
6. Epoch N+1 becomes EFFECTIVE
```

**Step 3: Certificate reissuance**

```
For each node (can be parallel):
1. Node generates new keypair
2. Node requests certificate from CA_B
3. CA_B issues certificate with:
   - Same node_uuid in CN and URI SAN
   - Same cluster_uuid in URI SAN
   - New public key
   - Validity: 30-90 days
4. Node stores new certificate and private key
5. Node updates mTLS configuration:
   - Still accepts CA_A and CA_B
   - Presents CA_B certificate for outgoing connections
6. Node updates membership record:
   - New cert_fingerprint
7. Node restarts mTLS listeners (graceful reload)
```

**Step 4: Peer observation verification**

```
Nodes perform peer observation:
1. Each node observes all other nodes via mTLS
2. Validates peer certificates:
   - Issued by CA_A or CA_B (both accepted in OVERLAP)
   - Tracks which CA issued each node's cert
3. Records observations:
   {
     observer: node_A_uuid,
     subject: node_B_uuid,
     cert_issuer: "CA_B",
     cert_fingerprint: sha256(...),
     observation_time: now
   }
4. Nodes write observations to Raft log

Convergence criteria for cutover:
- ALL enabled nodes have certificates from CA_B
- ALL enabled nodes observe ALL other enabled nodes with CA_B certs
- NO node still using CA_A certificate
```

**Step 5: Stability window**

```
Before cutover, ensure stability:
1. Monitor cluster for configurable duration (default: 1 hour)
2. Verify no certificate validation errors
3. Verify no mTLS connection failures
4. Check all nodes are healthy
5. Administrator confirms readiness for cutover
```

### 3.3 Cutover Phase: Detailed Steps

**Step 1: Administrator prepares cutover policy**

```
Current State: Epoch N+1 with CA_A+CA_B (OVERLAP)

Administrator creates Epoch N+2:
  CA Policy:
    - Phase: EXCLUSIVE
    - Trusted CAs:
      - CA_B: is_primary=true, trusted_until=null
    - CA_A removed from trusted list
    - All other configs unchanged
```

**Step 2: Publish cutover epoch**

```
1. Administrator publishes Epoch N+2
2. Raft commits epoch to log
3. All nodes receive epoch via replication
4. Nodes activate exclusive CA_B policy:
   - Stop trusting CA_A
   - Only accept CA_B certificates
   - Reject any connection with CA_A certificate
5. Nodes write ConvergenceAttestation
6. Epoch N+2 becomes EFFECTIVE
```

**Step 3: Verify cutover**

```
1. Check all nodes are at Epoch N+2
2. Verify all mTLS connections successful
3. Test new node join (should use CA_B)
4. Audit log review for any CA_A certificate rejections
```

**Result: CA rotation complete with zero downtime**

---

## 4. Peer Observation Requirements

### 4.1 Purpose

During CA rotation, **peer observation** provides cryptographic proof that all nodes have successfully transitioned to new certificates before the old CA is removed from trust.

### 4.2 Observation Protocol for CA Rotation

**Frequency**: Increased during OVERLAP phase (every 30 seconds vs. normal 60 seconds)

**Observation record**:

```cpp
struct CertificateObservation {
    UUID observer_node_uuid;
    UUID subject_node_uuid;

    // Certificate details
    bytes32 cert_fingerprint;       // SHA-256 of observed cert
    bytes32 issuer_ca_fingerprint;  // Which CA issued this cert
    string issuer_ca_name;          // e.g., "CA_B"

    // Validation results
    bool cert_valid;                // Certificate passed validation
    bool chain_valid;               // Chain to trusted CA valid
    timestamp_t cert_not_before;
    timestamp_t cert_not_after;

    // Context
    uint64_t epoch_number;          // Epoch when observed
    timestamp_t observed_at;

    // Signature
    Signature observer_signature;   // Ed25519 signature by observer
};
```

### 4.3 Convergence Gates for Cutover

**Cutover MUST NOT proceed unless**:

```
For all enabled nodes N_i and N_j (i ≠ j):
  ∃ observation O where:
    - O.observer = N_i
    - O.subject = N_j
    - O.issuer_ca_name = "CA_B" (new CA)
    - O.cert_valid = true
    - O.observed_at within last 5 minutes
```

**In plain English**:
- Every enabled node has observed every other enabled node
- All observed certificates are from the new CA (CA_B)
- All observations are recent (within 5 minutes)
- All certificates validated successfully

### 4.4 Observation Quorum

For large clusters (>10 nodes), full N×N observation may be expensive. **Quorum-based observation** is acceptable:

```
Instead of requiring all N×N observations, require:
- Each node observes ≥ quorum_size nodes (N/2 + 1)
- The union of all observations covers all nodes
- No node reports CA_A certificates

This reduces observation overhead while maintaining safety.
```

---

## 5. Safety Gates and Validation

### 5.1 Pre-Overlap Validation

Before publishing overlap epoch:

```
Validate:
1. New CA certificate is valid X.509 certificate
2. New CA public key differs from old CA
3. New CA has appropriate CA:TRUE basic constraint
4. All existing nodes can reach new CA (if external)
5. Test certificate issuance from new CA
6. Verify new CA respects cluster URI SAN requirements
```

### 5.2 Pre-Cutover Validation

Before publishing cutover epoch:

```
Validate:
1. All enabled nodes have CA_B certificates (peer observation)
2. No nodes still using CA_A certificates
3. Stability window elapsed (default: 1 hour)
4. No recent mTLS errors in cluster
5. All nodes at overlap epoch (N+1)
6. Administrator explicit confirmation
```

### 5.3 Validation Failures

**If validation fails**:

```
Option 1: Extend overlap period
- Increase trusted_until for CA_A
- Publish new epoch with extended overlap
- Continue monitoring

Option 2: Rollback (if no nodes transitioned yet)
- Publish new epoch removing CA_B
- Return to CA_A exclusive
- Investigate failure cause

Option 3: Force cutover (DANGEROUS)
- Administrator explicitly acknowledges risk
- Disable nodes that haven't transitioned
- Proceed with cutover
- Requires quorum without disabled nodes
```

---

## 6. Emergency CA Revocation

### 6.1 Scenario: CA Compromise

If a CA is compromised:

```
IMMEDIATE ACTIONS:
1. Administrator publishes emergency epoch:
   - Remove compromised CA from trusted list
   - Add replacement CA
   - Phase: OVERLAP (to allow transition)
   - Short stability window (10 minutes)

2. All nodes immediately:
   - Stop trusting compromised CA
   - Reject certificates from compromised CA
   - Request new certificates from replacement CA

3. Rapid certificate reissuance:
   - Automated if possible
   - Nodes self-service request new certs
   - Priority: VOTER nodes first

4. Forced cutover after minimum overlap:
   - Disable any nodes that failed to transition
   - Proceed with cutover
   - Manual recovery for disabled nodes
```

### 6.2 Certificate Revocation Lists (CRL)

```cpp
struct CRLPolicy {
    bool enable_crl_checking;       // Check CRLs during validation
    string[] crl_urls;              // CRL distribution points
    uint32_t crl_cache_ttl_seconds; // How long to cache CRLs
    bool fail_closed;               // Reject if CRL unavailable
};
```

**CRL Checking Process**:

```
When validating certificate:
1. Extract CRL distribution points from cert
2. If CRL checking enabled:
   a. Fetch CRL from distribution point (cache-aware)
   b. Verify CRL signature
   c. Check if cert serial number is in CRL
   d. If revoked: REJECT certificate
   e. If CRL unavailable and fail_closed=true: REJECT
3. Continue with rest of validation
```

### 6.3 OCSP Stapling

```cpp
struct OCSPPolicy {
    bool enable_ocsp_checking;      // Check OCSP during validation
    bool require_stapling;          // Require OCSP stapling in TLS
    string[] ocsp_responders;       // OCSP responder URLs
    uint32_t ocsp_cache_ttl_seconds;
    bool fail_closed;
};
```

---

## 7. Certificate Validation Algorithm

### 7.1 Full Validation Process

```cpp
enum CertValidationResult {
    VALID,
    EXPIRED,
    NOT_YET_VALID,
    REVOKED,
    UNTRUSTED_CA,
    CHAIN_INVALID,
    WRONG_CLUSTER,
    MISSING_URI_SAN,
    WEAK_KEY,
    ALGORITHM_DISALLOWED,
};

CertValidationResult validate_certificate(
    const X509Certificate& cert,
    const CAPolicy& policy,
    const UUID& expected_cluster_uuid
) {
    // 1. Time validity
    timestamp_t now = current_timestamp();
    if (now < cert.not_before) {
        return NOT_YET_VALID;
    }
    if (now > cert.not_after) {
        return EXPIRED;
    }

    // 2. Chain validation
    TrustedCA* issuing_ca = nullptr;
    for (auto& ca : policy.trusted_cas) {
        if (verify_signature(cert, ca.ca_certificate)) {
            issuing_ca = &ca;
            break;
        }
    }
    if (!issuing_ca) {
        return UNTRUSTED_CA;
    }

    // 3. Check if CA is still trusted (for OVERLAP phase)
    if (issuing_ca->trusted_until != timestamp_t::max()) {
        if (now > issuing_ca->trusted_until) {
            return UNTRUSTED_CA;  // CA trust expired
        }
    }

    // 4. URI SAN validation
    string uri_san = cert.get_uri_san();
    if (uri_san.empty() && policy.validation.require_uri_san) {
        return MISSING_URI_SAN;
    }

    // Parse: sb://cluster/<cluster_uuid>/node/<node_uuid>
    auto parsed = parse_scratchbird_uri(uri_san);
    if (!parsed.valid) {
        return MISSING_URI_SAN;
    }
    if (parsed.cluster_uuid != expected_cluster_uuid) {
        return WRONG_CLUSTER;
    }

    // 5. Key strength validation
    if (cert.public_key.algorithm == "RSA") {
        if (cert.public_key.key_size < policy.validation.min_key_size_rsa) {
            return WEAK_KEY;
        }
    } else if (cert.public_key.algorithm == "EC") {
        if (cert.public_key.key_size < policy.validation.min_key_size_ec) {
            return WEAK_KEY;
        }
    }

    // 6. Signature algorithm validation
    if (!policy.validation.allowed_sig_algs.contains(cert.signature_algorithm)) {
        return ALGORITHM_DISALLOWED;
    }

    // 7. Revocation checking
    if (policy.validation.require_crl_check || policy.validation.require_ocsp_check) {
        RevocationStatus status = check_revocation(cert, policy);
        if (status == REVOKED) {
            return REVOKED;
        }
    }

    // 8. Full chain validation (path length, constraints, etc.)
    if (!verify_certificate_chain_full(cert, issuing_ca->ca_certificate, policy)) {
        return CHAIN_INVALID;
    }

    return VALID;
}
```

---

## 8. Operational Procedures

### 8.1 Standard CA Rotation (Planned)

**Timeline**: 7-14 days total

```
Day 0: Preparation
- Generate new CA keypair (offline, HSM recommended)
- Create CA certificate
- Test certificate issuance
- Prepare overlap epoch configuration

Day 1: Publish overlap epoch
- Epoch N+1 published with CA_A + CA_B
- All nodes activate overlap policy
- Monitor for issues

Day 2-5: Certificate reissuance
- Automated or manual certificate requests
- Nodes transition to CA_B certificates
- Continuous peer observation monitoring

Day 6: Verify convergence
- Check all nodes have CA_B certificates
- Review peer observations
- Run validation checks

Day 7: Cutover
- Publish cutover epoch (CA_B exclusive)
- Verify all connections successful
- Monitor for 24 hours

Day 8+: Post-cutover monitoring
- Audit log review
- Performance monitoring
- Document lessons learned
```

### 8.2 Emergency CA Rotation (Compromise)

**Timeline**: 4-8 hours total

```
Hour 0: Detection and response
- CA compromise detected
- Emergency procedures activated
- Generate new CA immediately (may use temporary CA)

Hour 1: Publish emergency overlap
- Epoch N+1 published with CA_NEW only or CA_A + CA_NEW
- Compromised CA removed from trust
- Short stability window (15 minutes)

Hour 2-4: Rapid certificate reissuance
- Automated certificate requests
- Priority: VOTER nodes, then SHARD_OWNER nodes
- Disable nodes that cannot transition

Hour 4-6: Forced cutover
- Publish cutover epoch
- Accept that some nodes may be disabled
- Manual recovery procedures for disabled nodes

Hour 6-8: Recovery and verification
- Bring disabled nodes back online
- Full cluster health check
- Incident post-mortem
```

### 8.3 Administrative Commands

**Initiate CA rotation**:

```sql
ALTER CLUSTER CA POLICY
BEGIN OVERLAP
WITH NEW CA CERTIFICATE '<pem_encoded_ca_cert>'
STABILITY WINDOW '1 hour';
```

**Force cutover** (after overlap):

```sql
ALTER CLUSTER CA POLICY
CUTOVER TO NEW CA
REQUIRE CONVERGENCE TRUE;  -- Waits for peer observation convergence
```

**Emergency revocation**:

```sql
ALTER CLUSTER CA POLICY
EMERGENCY REVOKE CA '<ca_fingerprint>'
REPLACE WITH '<new_ca_cert>'
FORCE IMMEDIATE;  -- Dangerous: no overlap period
```

---

## 9. Monitoring and Alerting

### 9.1 Key Metrics

```
CA Rotation Metrics:
- ca_rotation_phase (gauge: EXCLUSIVE=0, OVERLAP=1, CUTOVER=2)
- ca_certificates_issued_by_ca (counter per CA)
- ca_overlap_duration_seconds (gauge)
- ca_peer_observations_completed (gauge: 0-100%)
- ca_nodes_with_old_certs (gauge: count)
- ca_validation_failures (counter, labeled by reason)
```

### 9.2 Alerts

```
CRITICAL Alerts:
- CA certificate expires in <30 days
- CA rotation stuck in OVERLAP >7 days
- Node failed to obtain new certificate
- Peer observation convergence <100% for >6 hours during rotation
- CA validation failures >1% of attempts

WARNING Alerts:
- CA certificate expires in <90 days
- CA rotation in OVERLAP phase >3 days
- Peer observation coverage <95%
- CRL/OCSP lookup failures >5%
```

---

## 10. Testing and Validation

### 10.1 Pre-Production Testing

Before production CA rotation:

```
Test in staging environment:
1. Full CA rotation cycle (overlap → cutover)
2. Certificate validation edge cases:
   - Expired certificates
   - Wrong cluster UUID
   - Invalid signatures
3. Failure scenarios:
   - Node offline during rotation
   - Network partition during overlap
   - CA unavailable during certificate request
4. Performance impact:
   - mTLS handshake latency
   - Certificate validation overhead
   - Peer observation load
```

### 10.2 Rotation Rehearsal

```
Quarterly rotation rehearsals:
1. Generate test CA
2. Perform rotation in non-production cluster
3. Time each phase
4. Document issues encountered
5. Update runbooks
```

---

## 11. Security Considerations

### 11.1 CA Private Key Protection

**Requirements**:
- CA private keys MUST be stored in HSM or offline
- CA private keys MUST NEVER be on cluster nodes
- CA certificate signing SHOULD be manual approval process
- CA private keys SHOULD use ceremony with multiple parties for access

### 11.2 Certificate Pinning

**Node certificates are pinned via membership record**:

```
When node joins cluster:
1. Certificate fingerprint recorded in membership
2. Future connections MUST match this fingerprint
3. Certificate rotation updates fingerprint
4. Old fingerprint retained in audit log
```

This prevents MITM attacks even with compromised CA.

### 11.3 Entropy for Key Generation

**Requirements**:
- Use cryptographically secure random number generator
- Minimum 256 bits of entropy for Ed25519 keys
- Minimum 2048 bits for RSA keys (if used)
- Verify entropy quality before key generation

---

## 12. References

- [Consul CA Certificate Rotation with No Downtime](https://support.hashicorp.com/hc/en-us/articles/18178232029843)
- [Zero Downtime Certificate Rotation Best Practices](https://expiring.at/blog/zero-downtime-certificate-rotation-strategies-tools-best-practices/)
- [X.509 Certificate Path Validation](https://datatracker.ietf.org/doc/html/rfc5280)
- SBCLUSTER-01: Cluster Configuration Epoch
- SBCLUSTER-02: Membership and Identity

---

**End of SBCLUSTER-03**
