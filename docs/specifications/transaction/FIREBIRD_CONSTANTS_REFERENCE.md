# Firebird Constants Reference (Transactions, GC, Lock Manager)


**Authoritative MGA/Lock/GC References:**
- [TRANSACTION_MGA_CORE.md](TRANSACTION_MGA_CORE.md)
- [TRANSACTION_LOCK_MANAGER.md](TRANSACTION_LOCK_MANAGER.md)
- [MGA_IMPLEMENTATION.md](../storage/MGA_IMPLEMENTATION.md)
- [FIREBIRD_GC_SWEEP_GLOSSARY.md](FIREBIRD_GC_SWEEP_GLOSSARY.md)
- [FIREBIRD_CONSTANTS_REFERENCE.md](FIREBIRD_CONSTANTS_REFERENCE.md)


## Purpose
This file captures Firebird constants used by transaction management, garbage
collection, and lock manager subsystems. It is a Firebird-derived reference for
ScratchBird implementation. Values are taken from Firebird sources and should be
kept in sync with the Firebird version used as the model.

---

## 1. Lock Manager Constants

Source: `firebird/src/lock/lock_proto.h`

Lock series aggregation:
- `LCK_MAX_SERIES = 7`
- `LCK_MIN = 1`
- `LCK_MAX = 2`
- `LCK_CNT = 3`
- `LCK_SUM = 4`
- `LCK_AVG = 5`
- `LCK_ANY = 6`

Lock levels:
- `LCK_none = 0`
- `LCK_null = 1`
- `LCK_SR = 2`
- `LCK_PR = 3`
- `LCK_SW = 4`
- `LCK_PW = 5`
- `LCK_EX = 6`
- `LCK_max = 7`
- `LCK_read = LCK_PR`
- `LCK_write = LCK_EX`

Wait modes:
- `LCK_WAIT = 1`
- `LCK_NO_WAIT = 0`

Lock block types:
- `type_null = 0`
- `type_lhb = 1`
- `type_lrq = 2`
- `type_lbl = 3`
- `type_his = 4`
- `type_shb = 5`
- `type_own = 6`
- `type_lpr = 7`

Lock header versioning:
- `BASE_LHB_VERSION = 19`
- `PLATFORM_LHB_VERSION = 128`
- `LHB_VERSION = PLATFORM_LHB_VERSION | BASE_LHB_VERSION` (64-bit)
- `LHB_VERSION = BASE_LHB_VERSION` (32-bit)

Request flags:
- `LRQ_blocking = 1`
- `LRQ_pending = 2`
- `LRQ_rejected = 4`
- `LRQ_deadlock = 8`
- `LRQ_repost = 16`
- `LRQ_scanned = 32`
- `LRQ_blocking_seen = 64`
- `LRQ_just_granted = 128`
- `LRQ_wait_timeout = 256`

Owner flags:
- `OWN_scanned = 1`
- `OWN_wakeup = 2`
- `OWN_signaled = 4`

Source: `firebird/src/jrd/lck.h`

Lock types (`lck_t`):
- `LCK_database`
- `LCK_relation`
- `LCK_bdb`
- `LCK_tra`
- `LCK_rel_exist`
- `LCK_idx_exist`
- `LCK_attachment`
- `LCK_shadow`
- `LCK_sweep`
- `LCK_expression`
- `LCK_prc_exist`
- `LCK_update_shadow`
- `LCK_backup_alloc`
- `LCK_backup_database`
- `LCK_backup_end`
- `LCK_rel_partners`
- `LCK_page_space`
- `LCK_dsql_cache`
- `LCK_monitor`
- `LCK_tt_exist`
- `LCK_cancel`
- `LCK_btr_dont_gc`
- `LCK_rel_gc`
- `LCK_tpc_init`
- `LCK_tpc_block`
- `LCK_fun_exist`
- `LCK_rel_rescan`
- `LCK_crypt`
- `LCK_crypt_status`
- `LCK_record_gc`
- `LCK_alter_database`
- `LCK_repl_state`
- `LCK_repl_tables`
- `LCK_dsql_statement_cache`
- `LCK_profiler_listener`

Lock owner types:
- `LCK_OWNER_database = 1`
- `LCK_OWNER_attachment`

Source: `firebird/src/common/config/config.h`

Lock manager defaults:
- `LockMemSize = 1048576` bytes
- `LockHashSlots = 8191`
- `LockAcquireSpins = 0`
- `DeadlockTimeout = 10` seconds

Source: `firebird/src/common/file_params.h`

Lock manager file name:
- `LOCK_FILE = "fb_lock_%s"`

---

## 2. Transaction Management Constants

Source: `firebird/src/jrd/tra.h`

Transaction defaults:
- `DEFAULT_LOCK_TIMEOUT = -1` (infinite)
- `TRA_BLOB_SPACE = "fb_blob_"`
- `TRA_UNDO_SPACE = "fb_undo_"`
- `MAX_TEMP_BLOBS = 1000`
- `TRA_AUTONOMOUS_PER_POOL = 64`
- `TRA_system_transaction = 0`

Transaction flags:
- `TRA_system = 0x00000001`
- `TRA_prepared = 0x00000002`
- `TRA_reconnected = 0x00000004`
- `TRA_degree3 = 0x00000008`
- `TRA_write = 0x00000010`
- `TRA_readonly = 0x00000020`
- `TRA_prepare2 = 0x00000040`
- `TRA_ignore_limbo = 0x00000080`
- `TRA_invalidated = 0x00000100`
- `TRA_deferred_meta = 0x00000200`
- `TRA_read_committed = 0x00000400`
- `TRA_autocommit = 0x00000800`
- `TRA_perform_autocommit = 0x00001000`
- `TRA_rec_version = 0x00002000`
- `TRA_restart_requests = 0x00004000`
- `TRA_no_auto_undo = 0x00008000`
- `TRA_precommitted = 0x00010000`
- `TRA_own_interface = 0x00020000`
- `TRA_read_consistency = 0x00040000`
- `TRA_ex_restart = 0x00080000`
- `TRA_replicating = 0x00100000`
- `TRA_no_blob_check = 0x00200000`
- `TRA_auto_release_temp_blobid = 0x00400000`

Transaction options mask:
- `TRA_OPTIONS_MASK = TRA_degree3 | TRA_readonly | TRA_ignore_limbo | TRA_read_committed |
  TRA_autocommit | TRA_rec_version | TRA_read_consistency | TRA_no_auto_undo |
  TRA_restart_requests | TRA_auto_release_temp_blobid`

TIP bit math:
- `TRA_MASK = 3`
- `TRA_SHIFT = 2`
- `TRANS_SHIFT(number) = ((number & TRA_MASK) << 1)`
- `TRANS_OFFSET(number) = (number >> TRA_SHIFT)`

Misc:
- `TRA_ACTIVE_CLEANUP = 100`

Source: `firebird/src/jrd/tpc_proto.h`

Commit number (CN) reserved values:
- `CN_ACTIVE = 0`
- `CN_PREHISTORIC = 1`
- `CN_LIMBO = -1` (cast to CommitNumber)
- `CN_DEAD = -2` (cast to CommitNumber)
- `CN_MAX_NUMBER = -3` (cast to CommitNumber)

Source: `firebird/src/common/config/config.h`

Transaction/TIP cache defaults:
- `TipCacheBlockSize = 4194304` bytes
- `SnapshotsMemSize = 65536` bytes
- `ReadConsistency = true`

Source: `firebird/src/common/file_params.h`

Transaction/TIP cache file names:
- `TPC_HDR_FILE = "fb_tpc_%s"`
- `TPC_BLOCK_FILE = "fb_tpc_%s_%" UQUADFORMAT`
- `SNAPSHOTS_FILE = "fb_snap_%s"`

---

## 3. Garbage Collection Constants

Source: `firebird/src/jrd/jrd.h`

Window flags (GC relevant):
- `WIN_large_scan = 1`
- `WIN_secondary = 2`
- `WIN_garbage_collector = 4`
- `WIN_garbage_collect = 8`

Thread flags (GC/sweep relevant):
- `TDBB_sweeper = 1`

Sweep pacing:
- `SWEEP_QUANTUM = 10`

Source: `firebird/src/jrd/Database.h`

Database flags (GC/sweep relevant):
- `DBB_garbage_collector = 0x00000008`
- `DBB_gc_active = 0x00000010`
- `DBB_gc_pending = 0x00000020`
- `DBB_sweep_in_progress = 0x00001000`
- `DBB_gc_starting = 0x00002000`
- `DBB_suspend_bgio = 0x00004000`
- `DBB_gc_cooperative = 0x00010000`
- `DBB_gc_background = 0x00020000`
- `DBB_sweep_starting = 0x00040000`

Source: `firebird/src/jrd/Attachment.h`

Attachment flags (GC relevant):
- `ATT_no_cleanup = 0x00001`
- `ATT_notify_gc = 0x00040`
- `ATT_garbage_collector = 0x00080`
- `ATT_from_thread = 0x80000`
- `ATT_NO_CLEANUP = (ATT_no_cleanup | ATT_notify_gc)`

---

## 4. Transaction/GC File Naming Constants

Source: `firebird/src/common/file_params.h`

- `TPC_HDR_FILE = "fb_tpc_%s"`
- `TPC_BLOCK_FILE = "fb_tpc_%s_%" UQUADFORMAT`
- `SNAPSHOTS_FILE = "fb_snap_%s"`
- `EVENT_FILE = "fb_event_%s"`
- `MONITOR_FILE = "fb_monitor_%s"`
- `REPL_FILE = "fb_repl_%s"`

---

## Notes
- This list is intentionally focused on transaction, GC, and lock manager scopes.
- If Firebird updates these values, ScratchBird must update this reference.