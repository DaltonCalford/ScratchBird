# Beta 2 Transparent At-Rest Encryption And Rekey Model

## Purpose

Define transparent at-rest encryption for live pages, derived physical artifacts,
and rekey behavior without weakening MGA truth or turning derivative logs into
recovery authority.

## Governing rules

1. Encryption changes bytes on storage media, not logical visibility rules.
2. Committed MGA page and transaction state remain authoritative after decrypt.
3. Rekey is a cataloged publication workflow, not an operator-only filesystem
   procedure.
4. Domain-level encryption intent remains valid, but this file owns storage,
   filespace, backup, and artifact encryption.

## Protected surfaces

The Beta 2 at-rest model shall cover:

- heap pages
- exact and deferred index pages
- overflow or oversized-value pages
- buffer spill files and durable temp artifacts when marked protected
- backup images
- shadow or promotion images
- PITR capsules and archive payloads when policy requires encryption

## Crypto hierarchy

ScratchBird shall use a four-level hierarchy:

1. root protector
   - external keystore, HSM, TPM-backed protector, or explicitly admitted local
     keystore
2. key-encryption-key generation
   - database or tenant-scoped wrapping key
3. data-encryption-key generation
   - filespace, object-family, or artifact-scoped working key
4. page or artifact nonce/tweak material
   - page-identity or artifact-identity bound uniqueness input

No page may store plaintext key material.

## Durable metadata

The canonical metadata rows are:

- `sb_security_keyring`
  - `key_uuid`
  - `key_class`
  - `generation`
  - `protector_id`
  - `status`
  - `created_at`
  - `retire_after`
- `sb_encryption_domain`
  - `domain_uuid`
  - `scope_class`
  - `scope_uuid`
  - `kek_uuid`
  - `dek_uuid`
  - `algorithm_family`
  - `nonce_family`
  - `rekey_policy`
  - `status`
- `sb_encryption_rekey_job`
  - `job_uuid`
  - `domain_uuid`
  - `from_generation`
  - `to_generation`
  - `mode`
  - `progress_state`
  - `page_cursor`
  - `started_at`
  - `completed_at`
  - `failure_code`

## Page-header contract

Every encrypted page family shall carry:

- encrypted flag
- crypto domain uuid
- crypto generation
- algorithm family enum
- tweak or nonce family enum
- header authentication or checksum mode indicator

Header crypto metadata must be readable before full-page decrypt so startup,
backup validation, and rekey workers can classify the page.

## Write path

1. Executor publishes committed logical changes through normal MGA rules.
2. Buffer manager selects the active crypto domain for the target filespace or
   object family.
3. Page image is serialized in canonical plaintext form in memory.
4. Encryption is applied immediately before durable writeback.
5. Page checksum or authenticated tag is computed over the encrypted payload and
   canonical header fields according to the page-family policy.
6. Disk write persists the encrypted page image.

Plaintext page images may exist only in admitted in-memory domains.

## Read path

1. Read page header.
2. Resolve crypto domain and generation from catalog-backed keyring state.
3. Refuse service if the domain is unknown, retired without compatibility, or
   unreadable.
4. Decrypt the page payload.
5. Validate checksum or authentication.
6. Publish the page to ordinary buffer and executor consumers.

## Rekey modes

Two rekey modes are admitted:

1. `REWRAP_ONLY`
   - allowed when algorithm family, tweak family, and page payload rules do not
     change
   - re-encrypts the DEK under the next KEK generation without rewriting every
     page
2. `PAGE_REWRITE`
   - required when page DEK changes, algorithm family changes, tweak policy
     changes, or the scope is crossing an encryption-domain boundary

`REWRAP_ONLY` may complete online without page rewriting. `PAGE_REWRITE` shall
run as a resumable maintenance job.

## Rekey workflow

1. Create next key generation rows in `sb_security_keyring`.
2. Create `sb_encryption_rekey_job`.
3. Publish `rekey_pending` in the target encryption domain.
4. New writes use the next generation immediately after publication.
5. Background worker rewrites old-generation pages when `PAGE_REWRITE` is
   required.
6. Completion requires: zero old-generation live-page count, verified backup or
   shadow compatibility update, and committed job finalization.
7. Retire the prior generation only after all dependent artifacts are outside
   the compatibility window.

## Backup and restore rules

1. Backup policy shall declare one of:
   - `INHERIT_LIVE_DOMAIN`
   - `DEDICATED_BACKUP_DOMAIN`
   - `UNENCRYPTED_REFUSED`
2. Encrypted backups carry backup-domain metadata and generation markers in the
   backup manifest.
3. Restore requires compatible protector access before page import begins.
4. Restore may rewrap imported DEKs into a new target protector during import,
   but it may not skip metadata validation.

## Failure rules

- Missing or unreadable protector state is `ENCRYPTION_PROTECTOR_UNAVAILABLE`.
- Unknown crypto generation is `ENCRYPTION_GENERATION_UNKNOWN`.
- Page decrypt failure is `ENCRYPTION_PAGE_DECRYPT_FAILED`.
- Attempted open of a required protected filespace without key access is
  `ENCRYPTION_OPEN_REFUSED`.
- Rekey promotion without verified page or artifact completion is
  `ENCRYPTION_REKEY_INCOMPLETE`.

## Metrics and observability

The runtime shall expose:

- encrypted page read and write counts by family
- decrypt failures
- active domains and generations
- rekey job progress
- old-generation live page count
- encrypted backup and restore job counts

## Implementation closure requirements

Beta 2 implementation is not complete until all of the following exist:

- startup scan that classifies encrypted versus unencrypted filespaces before
  ordinary page open
- page-family helpers for encrypted heap, index, overflow, temp, backup, and
  archive artifacts
- rekey worker checkpoint and resume markers
- operator workflow for pause, resume, and abort of rekey jobs
- backup manifest validation of encryption domain and generation compatibility
- restore-time refusal path when protector access or domain metadata is missing

## Operator examples

```sql
create encryption domain live_default on filespace primary;
alter encryption domain live_default rotate key generation;
select * from sb_encryption.rekey_status(domain_name => 'live_default');
```

## Sample worker skeleton

```cpp
for (;;) {
    RekeyJob job = catalog.load_next_rekey_job();
    if (!job.valid()) break;
    PageRef page = page_manager.load_page(job.page_cursor);
    PlainPage plain = crypto.decrypt(page, job.from_generation);
    EncryptedPage next = crypto.encrypt(plain, job.to_generation);
    page_manager.write_rekeyed_page(next);
    catalog.advance_rekey_cursor(job.job_uuid, page.page_id);
}
```

## Cross-section requirements

- section 05 owns page checksum and page-family binary layout details
- section 19 owns key admission, privilege, and keystore security rules
- section 33 owns protected memory domains used during decrypt and encrypt
- section 39 owns encrypted backup and archive artifact handling
