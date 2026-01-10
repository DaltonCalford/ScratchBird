# ScratchBird Encryption and Key Management Specification

**Document ID**: SBSEC-04  
**Version**: 1.0  
**Status**: Draft for Review  
**Date**: January 2026  
**Scope**: All deployment modes, Security Levels 2-6  

---

## 1. Introduction

### 1.1 Purpose

This document defines the encryption architecture and key management framework for the ScratchBird database engine. It specifies how data is protected at rest and in transit, how encryption keys are managed throughout their lifecycle, and how the system integrates with external key management systems.

### 1.2 Scope

This specification covers:
- Key hierarchy and derivation
- Transparent Data Encryption (TDE)
- Write-after log (WAL) and transaction log encryption
- Backup encryption
- Wire protocol encryption
- Key lifecycle management
- Hardware Security Module (HSM) integration
- Network Presence Binding (cluster mode)

### 1.3 Related Documents

| Document ID | Title |
|-------------|-------|
| SBSEC-01 | Security Architecture |
| SBSEC-02 | Identity and Authentication |
| SBSEC-06 | Cluster Security |
| SBSEC-07 | Network Presence Binding |

### 1.4 Security Level Applicability

| Feature | L0 | L1 | L2 | L3 | L4 | L5 | L6 |
|---------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| Encryption at rest | | | ● | ● | ● | ● | ● |
| Write-after log (WAL) encryption | | | ● | ● | ● | ● | ● |
| Backup encryption | | | ● | ● | ● | ● | ● |
| Wire encryption (TLS) | | | ○ | ○ | ● | ● | ● |
| HSM support | | | ○ | ○ | ○ | ● | ● |
| Key rotation | | | ○ | ● | ● | ● | ● |
| Network Presence Binding | | | | | | | ● |

● = Required | ○ = Optional | (blank) = Not applicable

---

## 2. Key Hierarchy

### 2.1 Overview

ScratchBird implements a hierarchical key management system where each key protects keys at the next level down:

```
┌─────────────────────────────────────────────────────────────────┐
│                      KEY HIERARCHY                               │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│                    ┌─────────────────┐                          │
│                    │       CMK       │                          │
│                    │ Cluster Master  │                          │
│                    │      Key        │                          │
│                    └────────┬────────┘                          │
│                             │                                    │
│         ┌───────────────────┼───────────────────┐               │
│         │                   │                   │               │
│         ▼                   ▼                   ▼               │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐         │
│  │     MEK     │    │     SDK     │    │     DBK     │         │
│  │   Memory    │    │  Security   │    │  Database   │         │
│  │ Encryption  │    │   DB Key    │    │    Key      │         │
│  │    Key      │    │             │    │             │         │
│  └─────────────┘    └──────┬──────┘    └──────┬──────┘         │
│        │                   │                  │                 │
│        │                   ▼                  │                 │
│        │           ┌─────────────┐            │                 │
│        │           │     CSK     │            │                 │
│        │           │ Credential  │            │                 │
│        │           │ Store Key   │            │                 │
│        │           └─────────────┘            │                 │
│        │                                      │                 │
│        │           ┌──────────────────────────┤                 │
│        │           │              │           │                 │
│        │           ▼              ▼           ▼                 │
│        │    ┌─────────────┐ ┌─────────────┐ ┌─────────────┐    │
│        │    │     TSK     │ │     LEK     │ │     BKK     │    │
│        │    │ Tablespace  │ │     Log     │ │   Backup    │    │
│        │    │    Key      │ │ Encryption  │ │    Key      │    │
│        │    │             │ │    Key      │ │             │    │
│        │    └──────┬──────┘ └─────────────┘ └─────────────┘    │
│        │           │                                            │
│        │           ▼                                            │
│        │    ┌─────────────┐                                     │
│        │    │     DEK     │                                     │
│        │    │    Data     │                                     │
│        │    │ Encryption  │                                     │
│        │    │    Keys     │                                     │
│        │    └─────────────┘                                     │
│        │                                                        │
│        └───────────────────────────────────────────────────────►│
│          Network Presence Binding (Level 6 only)                │
│          MEK protected by Shamir's Secret Sharing               │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 Key Definitions

| Key | Scope | Protected By | Protects | Lifetime |
|-----|-------|--------------|----------|----------|
| CMK | Cluster | Passphrase/HSM/KMS | All other keys | Until rotation |
| MEK | Runtime | CMK + Network Presence | Memory encryption | Session |
| SDK | Security DB | CMK | Credential Store Key | Until rotation |
| CSK | Credential Store | SDK | Stored passwords/tokens | Until rotation |
| DBK | Database | CMK | TSK, LEK, BKK | Until rotation |
| TSK | Tablespace | DBK | Data Encryption Keys | Until rotation |
| DEK | Page/Extent | TSK | Actual data pages | Until rotation |
| LEK | Transaction Log | DBK | Write-after log (WAL) entries | Until rotation |
| BKK | Backup | CMK + passphrase | Backup data | Per backup |

### 2.3 Key Storage

```sql
CREATE TABLE sys.sec_key_store (
    key_uuid            UUID PRIMARY KEY,
    key_type            ENUM('CMK', 'MEK', 'SDK', 'CSK', 'DBK', 'TSK', 'DEK', 'LEK', 'BKK'),
    key_version         INTEGER NOT NULL,
    
    -- Key data (encrypted by parent key)
    encrypted_key       BYTEA NOT NULL,
    key_algorithm       VARCHAR(32) NOT NULL,   -- 'AES-256-GCM', etc.
    
    -- Key wrapping
    wrapped_by_uuid     UUID,                   -- Parent key UUID
    wrapping_algorithm  VARCHAR(32),            -- 'AES-256-KWP', etc.
    
    -- Scope
    database_uuid       UUID,                   -- For DBK, TSK, DEK, LEK
    tablespace_uuid     UUID,                   -- For TSK, DEK
    
    -- Lifecycle
    status              ENUM('ACTIVE', 'ROTATING', 'RETIRED', 'DESTROYED'),
    created_at          TIMESTAMP NOT NULL,
    activated_at        TIMESTAMP,
    deactivated_at      TIMESTAMP,
    destroyed_at        TIMESTAMP,
    
    -- Rotation
    previous_version    UUID,
    next_version        UUID,
    
    -- Metadata
    created_by          UUID,
    key_purpose         VARCHAR(256),
    
    UNIQUE (key_type, key_version, database_uuid, tablespace_uuid)
);
```

---

## 3. Cluster Master Key (CMK)

### 3.1 Overview

The CMK is the root of the key hierarchy. All other keys are ultimately protected by the CMK.

### 3.2 CMK Derivation Sources

| Source | Description | Security Level |
|--------|-------------|----------------|
| PASSPHRASE | KDF from admin-provided passphrase | 2+ |
| HSM | Hardware Security Module via PKCS#11 | 4+ (5+ required) |
| EXTERNAL_KMS | AWS KMS, HashiCorp Vault, Azure Key Vault | 4+ |
| TPM | Trusted Platform Module binding | 4+ |
| SPLIT_KNOWLEDGE | N-of-M administrators at startup | 5+ |

### 3.3 Passphrase-Based CMK

```python
def derive_cmk_from_passphrase(passphrase, salt, config):
    """Derive CMK from passphrase using Argon2id."""
    
    # Argon2id parameters for key derivation
    params = {
        'memory_cost': 1048576,    # 1 GB (configurable)
        'time_cost': 4,            # 4 iterations
        'parallelism': 8,
        'hash_len': 32             # 256-bit key
    }
    
    # Derive key material
    key_material = argon2id(
        passphrase.encode('utf-8'),
        salt,
        **params
    )
    
    return CMK(
        key_data=key_material,
        algorithm='AES-256-GCM',
        derivation_source='PASSPHRASE',
        derivation_params=params
    )
```

### 3.4 HSM-Based CMK

When using an HSM, the CMK never leaves the HSM:

```
┌─────────────────────────────────────────────────────────────────┐
│                    HSM CMK ARCHITECTURE                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                    HSM (PKCS#11)                         │    │
│  │                                                          │    │
│  │  ┌───────────────────────────────────────────────────┐  │    │
│  │  │                 CMK (AES-256)                      │  │    │
│  │  │                 Never exported                     │  │    │
│  │  └───────────────────────────────────────────────────┘  │    │
│  │                         │                               │    │
│  │       Wrap/Unwrap operations via PKCS#11 API           │    │
│  │                         │                               │    │
│  └─────────────────────────┼───────────────────────────────┘    │
│                            │                                     │
│                            ▼                                     │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                 ScratchBird Engine                       │    │
│  │                                                          │    │
│  │  DBK, TSK wrapped by CMK inside HSM                     │    │
│  │  Wrapped keys stored in database                        │    │
│  │  Unwrap operation requires HSM access                    │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 3.5 External KMS Integration

```python
def derive_cmk_from_kms(kms_config):
    """Derive CMK from external KMS."""
    
    if kms_config.provider == 'aws':
        # AWS KMS
        client = boto3.client('kms')
        response = client.generate_data_key(
            KeyId=kms_config.key_id,
            KeySpec='AES_256'
        )
        plaintext_key = response['Plaintext']
        encrypted_key = response['CiphertextBlob']
        
    elif kms_config.provider == 'vault':
        # HashiCorp Vault
        client = hvac.Client(url=kms_config.vault_url)
        response = client.secrets.transit.generate_data_key(
            name=kms_config.key_name,
            key_type='aes256-gcm96'
        )
        plaintext_key = base64.b64decode(response['data']['plaintext'])
        encrypted_key = response['data']['ciphertext']
    
    # Store encrypted key for restart
    store_wrapped_cmk(encrypted_key, kms_config)
    
    return CMK(
        key_data=plaintext_key,
        algorithm='AES-256-GCM',
        derivation_source='EXTERNAL_KMS',
        kms_provider=kms_config.provider
    )
```

### 3.6 Split Knowledge CMK

For high-security deployments, the CMK can be split among multiple administrators:

```
┌─────────────────────────────────────────────────────────────────┐
│                  SPLIT KNOWLEDGE CMK                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Configuration: 3-of-5 split (threshold = 3, total = 5)         │
│                                                                  │
│  Initial Setup:                                                  │
│  1. Generate random CMK                                          │
│  2. Split using Shamir's Secret Sharing                         │
│  3. Distribute shares to 5 administrators                       │
│  4. Destroy original CMK                                         │
│                                                                  │
│  Startup:                                                        │
│  1. Prompt for shares (need 3)                                   │
│  2. Administrator 1 enters share                                 │
│  3. Administrator 2 enters share                                 │
│  4. Administrator 3 enters share                                 │
│  5. Reconstruct CMK using Lagrange interpolation                │
│  6. CMK exists only in memory                                    │
│                                                                  │
│  Security Properties:                                            │
│  • No single administrator knows CMK                             │
│  • 2 compromised shares reveal nothing                          │
│  • 3+ compromised shares = CMK compromise                        │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 4. Memory Encryption Key (MEK)

### 4.1 Overview

The MEK is a runtime key used to encrypt sensitive data in memory. It provides an additional layer of protection against memory scraping attacks.

### 4.2 MEK Derivation

The MEK is derived from the CMK at startup:

```python
def derive_mek(cmk):
    """Derive MEK from CMK for memory encryption."""
    
    # Use HKDF for key derivation
    hkdf = HKDF(
        algorithm=hashes.SHA256(),
        length=32,
        salt=get_system_salt(),
        info=b'scratchbird-mek-v1'
    )
    
    mek_key = hkdf.derive(cmk.key_data)
    
    return MEK(
        key_data=mek_key,
        algorithm='AES-256-GCM',
        session_id=generate_session_id()
    )
```

### 4.3 Network Presence Binding (Level 6)

In Level 6 cluster mode, the MEK is protected by Network Presence Binding using Shamir's Secret Sharing across cluster nodes:

```
┌─────────────────────────────────────────────────────────────────┐
│               NETWORK PRESENCE BINDING                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Cluster: 5 nodes, threshold = 3                                │
│                                                                  │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌────────┐│
│  │ Node A  │  │ Node B  │  │ Node C  │  │ Node D  │  │ Node E ││
│  │         │  │         │  │         │  │         │  │        ││
│  │ Share 1 │  │ Share 2 │  │ Share 3 │  │ Share 4 │  │ Share 5││
│  │ (memory)│  │ (memory)│  │ (memory)│  │ (memory)│  │(memory)││
│  └────┬────┘  └────┬────┘  └────┬────┘  └────┬────┘  └────┬───┘│
│       │            │            │            │            │     │
│       └────────────┴─────┬──────┴────────────┴────────────┘     │
│                          │                                       │
│                          ▼                                       │
│                 ┌─────────────────┐                             │
│                 │  MEK Assembly   │                             │
│                 │                 │                             │
│                 │  Requires 3+    │                             │
│                 │  nodes online   │                             │
│                 │  to reconstruct │                             │
│                 └─────────────────┘                             │
│                                                                  │
│  If < 3 nodes reachable:                                        │
│  • MEK cannot be reconstructed                                   │
│  • Database becomes read-only (cached data)                     │
│  • New encrypted writes fail                                     │
│  • "Dead Man's Switch" protection                               │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

See SBSEC-07 for detailed Network Presence Binding specification.

---

## 5. Database Encryption Keys

### 5.1 Database Key (DBK)

Each database has its own DBK, wrapped by the CMK:

```python
def create_database_key(database_uuid, cmk):
    """Create a new database encryption key."""
    
    # Generate random key
    dbk_key = secure_random_bytes(32)
    
    # Wrap with CMK
    wrapped_key = aes_key_wrap(
        wrapping_key=cmk.key_data,
        key_to_wrap=dbk_key,
        algorithm='AES-256-KWP'
    )
    
    # Store wrapped key
    store_key(
        key_uuid=generate_uuid_v7(),
        key_type='DBK',
        database_uuid=database_uuid,
        encrypted_key=wrapped_key,
        key_algorithm='AES-256-GCM',
        wrapping_algorithm='AES-256-KWP'
    )
    
    return DBK(
        key_data=dbk_key,
        database_uuid=database_uuid
    )
```

### 5.2 Tablespace Key (TSK)

Each tablespace has its own TSK, wrapped by the DBK:

```sql
-- Create encrypted tablespace
CREATE TABLESPACE sensitive_data
LOCATION '/data/sensitive'
ENCRYPTION = 'AES-256-GCM'
KEY_ROTATION_INTERVAL = '90 days';
```

### 5.3 Data Encryption Key (DEK)

DEKs encrypt individual pages or extents within a tablespace:

```
┌─────────────────────────────────────────────────────────────────┐
│                    DEK ARCHITECTURE                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Tablespace: sensitive_data                                      │
│  TSK: wrapped by DBK                                             │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                    Data File                             │    │
│  ├─────────────────────────────────────────────────────────┤    │
│  │                                                          │    │
│  │  Page 0    Page 1    Page 2    Page 3    ...            │    │
│  │  ┌──────┐  ┌──────┐  ┌──────┐  ┌──────┐                │    │
│  │  │DEK-0 │  │DEK-1 │  │DEK-2 │  │DEK-3 │                │    │
│  │  │      │  │      │  │      │  │      │                │    │
│  │  │ Data │  │ Data │  │ Data │  │ Data │                │    │
│  │  │      │  │      │  │      │  │      │                │    │
│  │  └──────┘  └──────┘  └──────┘  └──────┘                │    │
│  │                                                          │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  Options:                                                        │
│  1. Per-page DEK: Maximum security, higher overhead             │
│  2. Per-extent DEK: Balance of security and performance         │
│  3. Per-tablespace DEK: Single TSK=DEK, lower overhead          │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 6. Transparent Data Encryption (TDE)

### 6.1 Overview

TDE encrypts data transparently at the page level. Applications do not need modification.

### 6.2 Encryption Process

```
┌─────────────────────────────────────────────────────────────────┐
│                    TDE WRITE PATH                                │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Application                                                     │
│       │                                                          │
│       │ INSERT INTO sensitive_table VALUES (...)                │
│       ▼                                                          │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  SBLR Execution                                          │    │
│  │  • Data in plaintext in memory                          │    │
│  │  • Row assembled for storage                            │    │
│  └─────────────────────────────────────────────────────────┘    │
│       │                                                          │
│       ▼                                                          │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  Buffer Manager                                          │    │
│  │  • Page modified in buffer pool (plaintext)             │    │
│  │  • Page marked dirty                                    │    │
│  └─────────────────────────────────────────────────────────┘    │
│       │                                                          │
│       │ Page eviction / checkpoint                              │
│       ▼                                                          │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  Encryption Layer                                        │    │
│  │  • Get DEK for page (or derive from TSK)                │    │
│  │  • Generate unique IV/nonce                             │    │
│  │  • Encrypt page data with AES-256-GCM                   │    │
│  │  • Append authentication tag                            │    │
│  └─────────────────────────────────────────────────────────┘    │
│       │                                                          │
│       ▼                                                          │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  Storage                                                 │    │
│  │  • Encrypted page written to disk                       │    │
│  │  • IV stored with page (or derived from page number)    │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 6.3 Page Format

```
┌─────────────────────────────────────────────────────────────────┐
│                  ENCRYPTED PAGE FORMAT                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Bytes 0-31:     Page Header (unencrypted)                      │
│  ├─ Page number (8 bytes)                                       │
│  ├─ Page type (2 bytes)                                         │
│  ├─ Encryption indicator (2 bytes)                              │
│  ├─ Key version (4 bytes)                                       │
│  ├─ IV/Nonce (12 bytes)                                         │
│  └─ Reserved (4 bytes)                                          │
│                                                                  │
│  Bytes 32-(N-16):  Encrypted Data                               │
│  └─ AES-256-GCM ciphertext                                      │
│                                                                  │
│  Bytes (N-16)-N:   Authentication Tag (16 bytes)                │
│  └─ GCM authentication tag                                      │
│                                                                  │
│  AAD (Additional Authenticated Data):                           │
│  └─ Page header is AAD (authenticated but not encrypted)        │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 6.4 IV/Nonce Generation

```python
def generate_page_iv(page_number, key_version, write_counter):
    """Generate unique IV for page encryption."""
    
    # Option 1: Counter-based (deterministic)
    # IV = page_number || key_version || write_counter
    iv = struct.pack('>QHH', page_number, key_version, write_counter)
    
    # Option 2: Random (non-deterministic)
    # iv = secure_random_bytes(12)
    
    return iv
```

**Critical**: IV reuse with the same key is catastrophic for AES-GCM. Counter-based IVs prevent reuse but require careful management across restarts and replicas.

### 6.5 Encryption Algorithms

| Algorithm | Key Size | IV Size | Tag Size | Use Case |
|-----------|----------|---------|----------|----------|
| AES-256-GCM | 256 bits | 96 bits | 128 bits | Default, AEAD |
| AES-256-XTS | 256 bits | 128 bits | N/A | Disk encryption fallback |
| ChaCha20-Poly1305 | 256 bits | 96 bits | 128 bits | Software-only environments |

AES-256-GCM is the default due to hardware acceleration (AES-NI) availability.

---

## 7. Write-after Log (WAL) and Log Encryption

### 7.1 Log Encryption Key (LEK)

The LEK encrypts Write-Ahead Log entries:

```python
def encrypt_wal_record(record, lek):
    """Encrypt a write-after log (WAL) record."""
    
    # Generate unique IV from LSN
    iv = derive_wal_iv(record.lsn, lek.version)
    
    # Serialize record
    plaintext = serialize_wal_record(record)
    
    # Encrypt
    cipher = AES_GCM(lek.key_data, iv)
    ciphertext, tag = cipher.encrypt(
        plaintext,
        associated_data=record.lsn.to_bytes()
    )
    
    return EncryptedWALRecord(
        lsn=record.lsn,
        iv=iv,
        ciphertext=ciphertext,
        tag=tag,
        key_version=lek.version
    )
```

### 7.2 Write-after Log (WAL) Record Format

```
┌─────────────────────────────────────────────────────────────────┐
│              ENCRYPTED WRITE-AFTER LOG (WAL) RECORD              │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Header (unencrypted, authenticated):                           │
│  ├─ LSN (8 bytes)                                               │
│  ├─ Record length (4 bytes)                                     │
│  ├─ Record type (2 bytes)                                       │
│  ├─ Encryption flag (1 byte)                                    │
│  ├─ Key version (2 bytes)                                       │
│  └─ IV (12 bytes)                                               │
│                                                                  │
│  Encrypted payload:                                              │
│  ├─ Transaction ID                                              │
│  ├─ Page references                                             │
│  ├─ Before/after images                                         │
│  └─ Operation data                                              │
│                                                                  │
│  Authentication tag (16 bytes)                                  │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 7.3 Write-after Log (WAL) Key Rotation

LEK rotation must be coordinated with write-after log (WAL) archival:

1. Generate new LEK version
2. Mark rotation point in write-after log (WAL)
3. New records use new LEK
4. Old LEK retained until all old write-after log (WAL) archived
5. Old LEK destroyed after retention period

---

## 8. Backup Encryption

### 8.1 Backup Key (BKK)

Backups are encrypted with a dedicated key:

```python
def create_backup_key(backup_id, cmk, passphrase=None):
    """Create backup encryption key."""
    
    # Generate random backup key
    bkk_key = secure_random_bytes(32)
    
    # Wrap with CMK
    wrapped_by_cmk = aes_key_wrap(cmk.key_data, bkk_key)
    
    # Optionally also wrap with passphrase for portability
    if passphrase:
        passphrase_key = derive_key_from_passphrase(passphrase)
        wrapped_by_passphrase = aes_key_wrap(passphrase_key, bkk_key)
    else:
        wrapped_by_passphrase = None
    
    return BKK(
        key_data=bkk_key,
        backup_id=backup_id,
        wrapped_by_cmk=wrapped_by_cmk,
        wrapped_by_passphrase=wrapped_by_passphrase
    )
```

### 8.2 Backup Encryption Modes

| Mode | Description | Restore Requires |
|------|-------------|------------------|
| CMK_ONLY | Wrapped by CMK only | CMK access |
| CMK_PASSPHRASE | Wrapped by both | CMK or passphrase |
| PASSPHRASE_ONLY | Wrapped by passphrase | Passphrase only |

### 8.3 Backup Format

```
┌─────────────────────────────────────────────────────────────────┐
│                  ENCRYPTED BACKUP FORMAT                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Backup Header (unencrypted):                                   │
│  ├─ Magic number: "SBDB-BK\x01"                                │
│  ├─ Format version (4 bytes)                                    │
│  ├─ Backup UUID                                                 │
│  ├─ Source database UUID                                        │
│  ├─ Timestamp                                                   │
│  ├─ Encryption mode                                             │
│  ├─ Wrapped BKK (CMK)                                          │
│  ├─ Wrapped BKK (passphrase) - optional                        │
│  └─ Header checksum                                             │
│                                                                  │
│  Encrypted Segments:                                             │
│  ├─ Segment 1: Catalog metadata                                 │
│  ├─ Segment 2: Schema definitions                               │
│  ├─ Segment 3: Table data (per table)                          │
│  ├─ Segment 4: Indexes                                          │
│  └─ Segment N: ...                                              │
│                                                                  │
│  Each segment:                                                   │
│  ├─ Segment header (type, size, IV)                            │
│  ├─ Encrypted data                                              │
│  └─ Authentication tag                                          │
│                                                                  │
│  Backup Footer:                                                  │
│  ├─ Segment count                                               │
│  ├─ Total size                                                  │
│  ├─ Integrity hash (over all segments)                         │
│  └─ Footer checksum                                             │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 9. Wire Protocol Encryption

### 9.1 TLS Configuration

```sql
-- Global TLS configuration
ALTER SYSTEM SET wire.tls_mode = 'REQUIRED';  -- DISABLED, PREFERRED, REQUIRED
ALTER SYSTEM SET wire.min_tls_version = 'TLS13';
ALTER SYSTEM SET wire.cipher_suites = 
    'TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256';

-- Per-listener configuration
ALTER LISTENER postgresql SET tls_certificate = '/path/to/cert.pem';
ALTER LISTENER postgresql SET tls_private_key = '/path/to/key.pem';
ALTER LISTENER postgresql SET tls_client_cert_mode = 'REQUIRED';
```

### 9.2 Supported TLS Versions

| Version | Security Level | Notes |
|---------|----------------|-------|
| TLS 1.2 | 2+ | Minimum for compatibility |
| TLS 1.3 | 4+ (required 5+) | Preferred, required for Level 5+ |

### 9.3 Cipher Suites

For TLS 1.3 (Level 5+):
- TLS_AES_256_GCM_SHA384
- TLS_CHACHA20_POLY1305_SHA256
- TLS_AES_128_GCM_SHA256

For TLS 1.2 (Level 2-4):
- TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384
- TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384
- TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256

### 9.4 Client Certificate Authentication

```
┌─────────────────────────────────────────────────────────────────┐
│                 mTLS AUTHENTICATION                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  1. Client connects with TLS                                     │
│  2. Server presents certificate                                  │
│  3. Client validates server certificate                          │
│  4. Server requests client certificate                           │
│  5. Client presents certificate                                  │
│  6. Server validates:                                            │
│     a. Certificate chain to trusted CA                          │
│     b. Certificate not revoked (OCSP/CRL)                       │
│     c. Certificate attributes match allowed patterns            │
│  7. Extract client identity from certificate:                    │
│     • CN (Common Name)                                          │
│     • SAN (Subject Alternative Names)                           │
│     • Custom OID fields                                          │
│  8. Map certificate identity to ScratchBird user                │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 10. Key Lifecycle Management

### 10.1 Key States

```
┌─────────────────────────────────────────────────────────────────┐
│                    KEY LIFECYCLE                                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────────┐                                               │
│  │   PENDING    │ Key generated but not yet activated           │
│  └──────┬───────┘                                               │
│         │ activate()                                             │
│         ▼                                                        │
│  ┌──────────────┐                                               │
│  │    ACTIVE    │ Current key for encryption/decryption         │
│  └──────┬───────┘                                               │
│         │ rotate()                                               │
│         ▼                                                        │
│  ┌──────────────┐                                               │
│  │  ROTATING    │ New key active, old key still valid for read  │
│  └──────┬───────┘                                               │
│         │ complete_rotation()                                    │
│         ▼                                                        │
│  ┌──────────────┐                                               │
│  │   RETIRED    │ Only valid for decryption, not encryption     │
│  └──────┬───────┘                                               │
│         │ destroy() (after retention period)                     │
│         ▼                                                        │
│  ┌──────────────┐                                               │
│  │  DESTROYED   │ Key material securely erased                  │
│  └──────────────┘                                               │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 10.2 Key Rotation

```python
def rotate_key(key_uuid):
    """Rotate an encryption key."""
    
    old_key = get_key(key_uuid)
    
    # 1. Generate new key
    new_key = generate_key(
        key_type=old_key.key_type,
        algorithm=old_key.algorithm
    )
    new_key.version = old_key.version + 1
    new_key.previous_version = old_key.key_uuid
    
    # 2. Update old key status
    old_key.status = 'ROTATING'
    old_key.next_version = new_key.key_uuid
    
    # 3. Activate new key
    new_key.status = 'ACTIVE'
    store_key(new_key)
    update_key(old_key)
    
    # 4. Start background re-encryption
    if old_key.key_type in ('TSK', 'DEK'):
        schedule_reencryption(old_key, new_key)
    
    # 5. Log rotation event
    audit_log('KEY_ROTATED', old_key, new_key)
    
    return new_key
```

### 10.3 Re-encryption Process

When keys are rotated, existing data must be re-encrypted:

```
┌─────────────────────────────────────────────────────────────────┐
│                  RE-ENCRYPTION PROCESS                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Background worker process:                                      │
│                                                                  │
│  1. Scan for pages encrypted with old key version               │
│                                                                  │
│  2. For each page (throttled):                                   │
│     a. Read page                                                │
│     b. Decrypt with old key                                     │
│     c. Re-encrypt with new key                                  │
│     d. Write page                                               │
│     e. Update page metadata                                     │
│                                                                  │
│  3. Track progress:                                              │
│     • Pages remaining                                           │
│     • Estimated completion time                                 │
│     • Throttle rate                                             │
│                                                                  │
│  4. When complete:                                               │
│     a. Mark old key as RETIRED                                  │
│     b. Schedule destruction after retention period              │
│                                                                  │
│  Throttling:                                                     │
│  • Max IOPS for re-encryption: configurable                     │
│  • Pause during peak hours: optional                            │
│  • Priority: lower than user operations                         │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 10.4 Key Destruction

```python
def destroy_key(key_uuid):
    """Securely destroy a key."""
    
    key = get_key(key_uuid)
    
    # Validation
    if key.status != 'RETIRED':
        raise KeyError("Can only destroy retired keys")
    
    if has_data_encrypted_with_key(key):
        raise KeyError("Data still encrypted with this key")
    
    # Secure destruction
    # 1. Overwrite key material with random data
    random_overwrite(key.encrypted_key, 3)  # 3 passes
    
    # 2. Update status
    key.status = 'DESTROYED'
    key.destroyed_at = now()
    key.encrypted_key = None
    update_key(key)
    
    # 3. Audit log
    audit_log('KEY_DESTROYED', key)
    
    # 4. If HSM-backed, destroy in HSM
    if key.hsm_handle:
        hsm_destroy_key(key.hsm_handle)
```

### 10.5 Rotation Policies

```sql
-- Set rotation policy
ALTER TABLESPACE sensitive_data 
SET KEY_ROTATION_INTERVAL = '90 days';

-- Policy table
CREATE TABLE sys.sec_key_policies (
    policy_uuid         UUID PRIMARY KEY,
    key_type            VARCHAR(32),
    
    -- Rotation
    rotation_interval   INTERVAL,           -- Auto-rotate after
    max_key_age         INTERVAL,           -- Force rotate after
    
    -- Retention
    retired_retention   INTERVAL,           -- Keep retired key
    
    -- Destruction
    destruction_delay   INTERVAL,           -- Wait before destroy
    secure_erase_passes INTEGER DEFAULT 3,
    
    -- Alerts
    alert_days_before   INTEGER DEFAULT 14  -- Alert before expiry
);
```

---

## 11. Key Management Operations

### 11.1 SQL Commands

```sql
-- Generate new master key (initial setup)
ALTER SYSTEM INITIALIZE ENCRYPTION 
WITH PASSPHRASE 'secret'
ALGORITHM 'AES-256-GCM';

-- Rotate master key
ALTER SYSTEM ROTATE MASTER KEY;

-- Change passphrase
ALTER SYSTEM REKEY MASTER KEY 
WITH PASSPHRASE 'new_secret';

-- Enable encryption on database
ALTER DATABASE mydb ENABLE ENCRYPTION;

-- Enable encryption on tablespace
ALTER TABLESPACE myts ENABLE ENCRYPTION
WITH ALGORITHM 'AES-256-GCM';

-- Rotate tablespace key
ALTER TABLESPACE myts ROTATE KEY;

-- Export wrapped key (for backup)
SELECT sys.export_wrapped_key('DBK', database_uuid);

-- Import wrapped key (for restore)
SELECT sys.import_wrapped_key(wrapped_key_data);
```

### 11.2 Key Export/Import

For disaster recovery, wrapped keys can be exported:

```python
def export_wrapped_key(key_uuid, export_passphrase):
    """Export a key wrapped with export passphrase."""
    
    key = get_key(key_uuid)
    
    # Unwrap with current parent
    unwrapped = unwrap_key(key)
    
    # Re-wrap with export passphrase
    export_key = derive_key_from_passphrase(export_passphrase)
    exported = aes_key_wrap(export_key, unwrapped)
    
    # Secure clear unwrapped key
    secure_memzero(unwrapped)
    
    return ExportedKey(
        key_uuid=key.key_uuid,
        key_type=key.key_type,
        wrapped_key=exported,
        export_timestamp=now()
    )
```

---

## 12. Security Considerations

### 12.1 Key Protection

| Threat | Mitigation |
|--------|------------|
| Memory scraping | MEK, secure memory allocation, optional memory encryption |
| Key file theft | Keys wrapped by CMK, CMK in HSM or passphrase-derived |
| Cold boot attack | MEK volatile, cleared on shutdown |
| Side-channel | Constant-time operations, cache-timing resistant |
| Quantum threats | AES-256 remains secure, monitor PQC developments |

### 12.2 Cryptographic Requirements

| Requirement | Implementation |
|-------------|----------------|
| Key generation | CSPRNG (OS-provided) |
| Key derivation | Argon2id, HKDF-SHA256 |
| Encryption | AES-256-GCM (AEAD) |
| Key wrapping | AES-256-KWP (RFC 5649) |
| Hashing | SHA-256, SHA-384 |
| Random bytes | /dev/urandom, BCryptGenRandom |

### 12.3 Compliance Considerations

| Standard | Requirement | ScratchBird Compliance |
|----------|-------------|------------------------|
| PCI-DSS 3.5 | Protect encryption keys | CMK in HSM or split knowledge |
| PCI-DSS 3.6 | Key management procedures | Documented rotation, destruction |
| HIPAA | Encryption at rest | TDE with AES-256 |
| GDPR Art 32 | Appropriate technical measures | Encryption, access control |
| SOC 2 CC6.1 | Encryption of data | TDE, wire encryption |

---

## 13. Audit Events

### 13.1 Key Management Events

| Event | Logged Data |
|-------|-------------|
| KEY_GENERATED | key_uuid, key_type, algorithm, generated_by |
| KEY_ACTIVATED | key_uuid, activated_by |
| KEY_ROTATED | old_key_uuid, new_key_uuid, rotated_by |
| KEY_RETIRED | key_uuid, retired_by, reason |
| KEY_DESTROYED | key_uuid, destroyed_by |
| KEY_EXPORTED | key_uuid, exported_by, export_format |
| KEY_IMPORTED | key_uuid, imported_by, source |

### 13.2 Encryption Events

| Event | Logged Data |
|-------|-------------|
| ENCRYPTION_ENABLED | database/tablespace, algorithm, enabled_by |
| ENCRYPTION_DISABLED | database/tablespace, disabled_by, reason |
| REENCRYPTION_STARTED | tablespace, old_key, new_key |
| REENCRYPTION_COMPLETED | tablespace, pages_processed, duration |
| DECRYPTION_FAILURE | object, reason (no key details) |

---

## Appendix A: Configuration Reference

### A.1 Encryption Policies

| Policy Key | Type | Default | Description |
|------------|------|---------|-------------|
| `encryption.enabled` | boolean | false | Global encryption enable |
| `encryption.algorithm` | string | AES-256-GCM | Default algorithm |
| `encryption.key_source` | enum | PASSPHRASE | CMK source |
| `encryption.hsm.provider` | string | | PKCS#11 provider |
| `encryption.kms.provider` | string | | External KMS |

### A.2 Key Rotation Policies

| Policy Key | Type | Default | Description |
|------------|------|---------|-------------|
| `key.rotation.auto` | boolean | false | Automatic rotation |
| `key.rotation.interval` | interval | 90d | Rotation interval |
| `key.retention.retired` | interval | 365d | Retired key retention |
| `key.destruction.delay` | interval | 30d | Destruction delay |

### A.3 TLS Policies

| Policy Key | Type | Default | Description |
|------------|------|---------|-------------|
| `wire.tls_mode` | enum | PREFERRED | DISABLED, PREFERRED, REQUIRED |
| `wire.min_tls_version` | enum | TLS12 | TLS12, TLS13 |
| `wire.client_cert_mode` | enum | DISABLED | DISABLED, OPTIONAL, REQUIRED |

---

*End of Document*
