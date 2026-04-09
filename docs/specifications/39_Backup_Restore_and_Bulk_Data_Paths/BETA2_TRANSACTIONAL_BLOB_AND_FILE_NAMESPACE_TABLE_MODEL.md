# Beta 2 Transactional Blob And File Namespace Table Model

## Purpose

Define transactional blob objects and file-namespace tables over ScratchBird
object storage and catalog truth.

## Governing rules

1. Namespace metadata is transactional.
2. Blob object bytes may live in object storage or filespace storage, but
   catalog truth remains local.
3. Rename and move operations are atomic at namespace scope.
4. Path visibility follows ordinary transaction isolation and security rules.

## Canonical metadata

- `sb_blob_namespace`
  - `namespace_uuid`
  - `namespace_name`
  - `root_policy`
  - `storage_policy`
- `sb_blob_object`
  - `object_uuid`
  - `namespace_uuid`
  - `content_locator`
  - `byte_length`
  - `checksum`
  - `version_no`
- `sb_blob_path`
  - `path_uuid`
  - `namespace_uuid`
  - `path_text`
  - `object_uuid`
  - `path_state`

## Operation flow

1. Insert creates object metadata and one path binding.
2. Rename updates path bindings atomically.
3. Delete marks path state and garbage-collection eligibility.
4. Versioned object retention follows namespace policy.

## Refusal rules

- `BLOB_NAMESPACE_UNKNOWN`
- `BLOB_PATH_CONFLICT`
- `BLOB_STORAGE_POLICY_REFUSED`
- `BLOB_OBJECT_ORPHANED`

## Example

```sql
create blob namespace documents;
insert into documents.paths(path_text, content_bytes) values ('/legal/a.txt', :payload);
update documents.paths set path_text = '/legal/archive/a.txt' where path_text = '/legal/a.txt';
```

## Cross-section requirements

- section `39` owns object publication and retention
- section `24` owns namespace catalog rows
- section `19` owns path and blob security policy
