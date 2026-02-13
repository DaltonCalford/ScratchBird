# archive/alpha_phase_2/11a-Connection-Pool-Implementation.md - Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/archive/alpha_phase_2/11a-Connection-Pool-Implementation.md`

Status notes:
- The document explicitly states it is **Non-Authoritative**, but describes the canonical UDR pool behavior.

Implementation notes (UDR pool in this repo):
- Implementation lives in `src/udr/connection_pool.cpp` with config in `include/scratchbird/udr/connection_pool.h`.
- Config fields differ from spec (e.g., `min_size/max_size`, `validation_query`, `test_on_borrow/return`, `max_retries`), and spec fields like `max_idle`, `reset_on_release`, `max_in_flight_per_conn`, and backoff settings are not present.
- Pool keying/isolation (server definition + user mapping + TLS + session options) is not represented in the pool interface; pool is keyed only by name via `ConnectionPoolManager`.
- Acquire logic supports `test_on_borrow` validation, but does not explicitly check idle timeout vs health interval on borrow.
- Release logic does not perform rollback/reset sequence; it only drops closed/expired connections and optionally validates on return.
- Health/maintenance thread only evicts idle/expired connections and ensures `min_size`; no periodic validation query/ping loop or exponential backoff.
- Metrics tracked differ from required spec set (no last_error_* fields; no wait_queue_depth metric; naming differs).
- Cancellation semantics are not integrated at the pool layer.

Verification:
- Partial code-level verification only (UDR pool behavior; no conformance tests run).
