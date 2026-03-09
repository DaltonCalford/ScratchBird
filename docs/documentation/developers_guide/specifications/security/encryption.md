# Specification: Encryption

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | security/encryption |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.1.0 |
| **Authors** | ScratchBird Security Team |

## Synopsis

This specification defines encryption capabilities in ScratchBird, including data at rest encryption, Transparent Data Encryption (TDE) architecture, column-level encryption, and key management.

## Scope

### In Scope

- Encryption architecture overview
- TDE (Transparent Data Encryption) design
- Column-level encryption
- Key management hierarchy
- Encryption algorithms

### Out of Scope

- SSL/TLS transport encryption (see `ssl_tls.md`)
- Authentication encryption (SCRAM - see `auth_plugins.md`)
- Application-level encryption (client-side)

## Background

ScratchBird's encryption strategy focuses on protecting data at rest. The architecture supports multiple encryption layers from storage-level TDE to column-level encryption for sensitive fields.

## Specification

### Encryption Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    Encryption Layers                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌───────────────────────────────────────────────────────────┐ │
│  │ Layer 3: Column-Level Encryption                           │ │
│  │ ┌─────────┐ ┌─────────┐ ┌─────────┐                      │ │
│  │ │  SSN    │ │  CC#    │ │Password │  Per-column AES-256  │ │
│  │ │(encrypted│ │(encrypted│ │(encrypted│                      │ │
│  │ └─────────┘ └─────────┘ └─────────┘                      │ │
│  └───────────────────────────────────────────────────────────┘ │
│                           │                                     │
│  ┌────────────────────────┴──────────────────────────────────┐ │
│  │ Layer 2: Transparent Data Encryption (TDE)               │ │
│  │ ┌──────────────────────────────────────────────────────┐ │ │
│  │ │ Encrypted Data Files (AES-256-XTS)                   │ │ │
│  │ │ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐     │ │ │
│  │ │ │ Page 1  │ │ Page 2  │ │ Page 3  │ │ Page N  │ ... │ │ │
│  │ │ │[encrypted│ │[encrypted│ │[encrypted│ │[encrypted│     │ │ │
│  │ │ └─────────┘ └─────────┘ └─────────┘ └─────────┘     │ │ │
│  │ └──────────────────────────────────────────────────────┘ │ │
│  └───────────────────────────────────────────────────────────┘ │
│                           │                                     │
│  ┌────────────────────────┴──────────────────────────────────┐ │
│  │ Layer 1: Storage Encryption (OS/Filesystem)              │ │
│  │ ┌──────────────────────────────────────────────────────┐ │ │
│  │ │ LUKS / BitLocker / EFS / Hardware FDE              │ │ │
│  │ └──────────────────────────────────────────────────────┘ │ │
│  └───────────────────────────────────────────────────────────┘ │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Key Management Hierarchy

```
Key Hierarchy:

┌─────────────────────────────────────────────────────────────┐
│ Master Key (MK)                                             │
│ ├─ Stored in: HSM / Key Vault / Password-protected file    │
│ ├─ Algorithm: AES-256                                       │
│ └─ Rotation: Manual, scheduled                              │
└──────────────────┬──────────────────────────────────────────┘
                   │ Encrypts
                   ▼
┌─────────────────────────────────────────────────────────────┐
│ Key Encryption Key (KEK)                                    │
│ ├─ Stored in: Database header / Separate key file          │
│ ├─ Algorithm: AES-256-GCM                                   │
│ └─ Rotation: On master key rotation                         │
└──────────────────┬──────────────────────────────────────────┘
                   │ Encrypts
                   ▼
┌─────────────────────────────────────────────────────────────┐
│ Data Encryption Key (DEK) / Tablespace Key                  │
│ ├─ Stored in: Encrypted in database header                  │
│ ├─ Algorithm: AES-256-XTS (TDE) / AES-256-GCM (column)     │
│ └─ Rotation: Re-encryption required                         │
└──────────────────┬──────────────────────────────────────────┘
                   │ Encrypts
                   ▼
┌─────────────────────────────────────────────────────────────┐
│ Data                                                        │
│ └─ Database pages / Column values                           │
└─────────────────────────────────────────────────────────────┘
```

### Transparent Data Encryption (TDE)

#### TDE Configuration

```cpp
struct TDEConfig {
    bool enabled = false;
    
    // Master key source
    enum class MasterKeySource {
        FILE,           // Password-protected key file
        HSM,            // Hardware Security Module
        KMIP,           // KMIP-compatible key server
        ENV_VAR         // Environment variable
    };
    MasterKeySource key_source = MasterKeySource::FILE;
    
    // Key file configuration
    std::string master_key_file;
    std::string master_key_password;  // Or use key wrapping
    
    // HSM configuration
    std::string hsm_library_path;
    std::string hsm_slot;
    std::string hsm_key_label;
    
    // Encryption algorithm
    std::string cipher = "AES-256-XTS";
    
    // Tablespace-specific encryption
    std::map<std::string, std::string> tablespace_keys;
};
```

#### TDE Page Structure

```
Encrypted Page Layout (8KB):

┌─────────────────────────────────────────────────────────────────┐
│ Page Header (unencrypted)                                       │
│ ├─ Page ID          : 4 bytes                                   │
│ ├─ Page LSN         : 8 bytes                                   │
│ ├─ Checksum         : 4 bytes                                   │
│ ├─ Encryption IV    : 16 bytes                                  │
│ └─ Flags            : 4 bytes                                   │
├─────────────────────────────────────────────────────────────────┤
│ Encrypted Data (8128 bytes)                                     │
│ ├─ Original page data encrypted with AES-256-XTS               │
│ └─ Authentication tag (if using AEAD mode)                     │
├─────────────────────────────────────────────────────────────────┤
│ Padding to 8KB boundary                                         │
└─────────────────────────────────────────────────────────────────┘

Total: 8192 bytes (standard page size)
```

#### TDE Encryption/Decryption

```cpp
class TDEEngine {
public:
    // Initialize with master key
    Status initialize(const TDEConfig& config, ErrorContext* ctx);
    
    // Encrypt page before writing
    Status encryptPage(
        const void* plaintext_page,
        size_t page_size,
        PageId page_id,
        void* encrypted_page,
        ErrorContext* ctx
    );
    
    // Decrypt page after reading
    Status decryptPage(
        const void* encrypted_page,
        size_t page_size,
        PageId page_id,
        void* plaintext_page,
        ErrorContext* ctx
    );
    
    // Rotate data encryption key
    Status rotateDEK(ErrorContext* ctx);
    
private:
    std::vector<uint8_t> master_key_;
    std::vector<uint8_t> dek_;
    EVP_CIPHER_CTX* cipher_ctx_;
};
```

### Column-Level Encryption

```sql
-- Column encryption functions
CREATE EXTENSION IF NOT EXISTS pgcrypto;

-- Encrypt column value
SELECT encrypt(
    data := 'sensitive data'::bytea,
    key := 'my-secret-key-32-bytes-long!!'::bytea,
    type := 'aes-256-cbc'
);

-- Decrypt column value
SELECT decrypt(
    data := encrypted_value,
    key := 'my-secret-key-32-bytes-long!!'::bytea,
    type := 'aes-256-cbc'
);
```

#### Encrypted Column Type

```cpp
// Encrypted data storage format
struct EncryptedColumnValue {
    uint8_t version;           // Format version
    uint8_t algorithm;         // Encryption algorithm used
    uint8_t key_id[16];        // Key identifier (UUID)
    uint8_t iv[16];            // Initialization vector
    uint32_t ciphertext_len;   // Length of encrypted data
    uint8_t auth_tag[16];      // Authentication tag (GCM mode)
    uint8_t ciphertext[];      // Variable-length ciphertext
};
```

### Supported Encryption Algorithms

| Algorithm | Mode | Use Case | Security Level |
|-----------|------|----------|----------------|
| AES-128 | CBC | Column-level | Medium |
| AES-256 | CBC | Column-level | High |
| AES-256 | GCM | Column-level (AEAD) | High |
| AES-256 | XTS | TDE (full disk) | High |
| ChaCha20 | Poly1305 | Alternative to AES-GCM | High |

### Key Rotation

```
Key Rotation Process:

1. Generate new DEK
   new_dek = random(32 bytes)

2. Re-encrypt data
   FOR each encrypted_page IN tablespace:
     plaintext = decrypt(page, old_dek)
     ciphertext = encrypt(plaintext, new_dek)
     write(ciphertext)

3. Update key metadata
   encrypt_and_store(new_dek, KEK)
   mark_old_dek_as_deprecated()

4. Verify
   spot_check_pages()

5. Cleanup
   secure_erase(old_dek)
```

### Encryption and Compression

Order of operations:
1. **Compress then Encrypt**: Better security (compressing encrypted data is inefficient)
2. For TDE: Transparent - happens at storage layer
3. For column-level: Application decides

```
Recommended: Plaintext → Compress → Encrypt → Store
Alternative:  Plaintext → Encrypt → Store (no compression)
```

### Backup Encryption

```sql
-- Encrypted backup
BACKUP DATABASE mydb 
    TO '/backup/mydb.bak'
    WITH ENCRYPTION (
        ALGORITHM = 'AES-256-CBC',
        KEY = 'backup-encryption-key'
    );

-- Restore from encrypted backup
RESTORE DATABASE mydb 
    FROM '/backup/mydb.bak'
    WITH DECRYPTION (
        KEY = 'backup-encryption-key'
    );
```

### SQL Interface

```sql
-- Enable TDE on database
ALTER DATABASE mydb SET ENCRYPTION ON;

-- Create encrypted tablespace
CREATE TABLESPACE encrypted_ts
    LOCATION '/data/encrypted'
    WITH (ENCRYPTION = ON);

-- Create table in encrypted tablespace
CREATE TABLE sensitive_data (
    id UUID PRIMARY KEY,
    ssn VARCHAR(11) ENCRYPTED,
    credit_card VARCHAR(16) ENCRYPTED
) TABLESPACE encrypted_ts;

-- Encrypt existing column
ALTER TABLE employees 
    ALTER COLUMN salary TYPE ENCRYPTED 
    USING encrypt(salary::text, 'key');

-- Rotate encryption key
ALTER DATABASE mydb ROTATE ENCRYPTION KEY;
```

## Security Considerations

| Threat | Mitigation |
|--------|------------|
| Stolen storage | TDE encrypts data files |
| Memory dump | Keys in secure memory, cleared after use |
| Key exposure | HSM storage, key rotation |
| Backup theft | Encrypted backups |
| Insider threat | Separate key custodians |

## Performance Impact

| Encryption Type | CPU Overhead | I/O Impact | Memory Impact |
|-----------------|--------------|------------|---------------|
| TDE (AES-XTS) | 3-5% | Minimal | +32 bytes/page |
| Column AES-GCM | 5-10% | +16 bytes/row | Minimal |
| Column AES-CBC | 2-5% | +16 bytes/row | Minimal |

## Invariants

1. **Key Separation**: DEK never stored unencrypted
   - Verification: KEK encrypts DEK at all times

2. **Authenticated Encryption**: Detect tampering
   - Verification: Use GCM or HMAC modes

3. **Secure Key Storage**: Master key in HSM or secure storage
   - Verification: Key never in plain text files

## Related Specifications

- `ssl_tls.md` - Transport encryption
- `cls_column_masking.md` - Data masking (alternative)
- `storage/page_layout.md` - Page structure

## Appendix

### Encryption Key Generation

```bash
# Generate 256-bit master key
openssl rand -base64 32 > master.key

# Generate DEK wrapped with KEK
openssl enc -aes-256-cbc -salt -in dek.bin -out dek.enc -pass file:master.key
```

### Compliance Mappings

| Regulation | Requirement | Implementation |
|------------|-------------|----------------|
| GDPR | Data protection | TDE + Column encryption |
| PCI-DSS | Cardholder data | Column encryption for PAN |
| HIPAA | PHI protection | Full database encryption |
| SOC 2 | Access controls | Encryption at rest |

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
