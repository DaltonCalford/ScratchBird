# Recovery Markers and Repairable Page Fields

Status: current_authority

## 1. Current marker authority

The canonical `PageHeader` carries the current page-local recovery and repair
markers.

Relevant fields are:

- `page_generation`
- `flush_generation`
- `checkpoint_generation`
- `repair_epoch`
- `repair_state_raw`

These are page-image evidence fields.
They do not replace MGA transaction inventory or transaction-manager startup
reconciliation.

## 2. Generation-order rules

Shared validation requires:

1. `flush_generation <= page_generation`
2. `checkpoint_generation <= flush_generation`

Any violation is page corruption.

Interpretation:

- `page_generation` is the current published generation for the page image
- `flush_generation` is the last durably flushed generation
- `checkpoint_generation` is the last completed checkpoint generation covering
  the page

## 3. Repair-state rules

`repair_state_raw` is validated against the current compiled `PageRepairState`
enum range.

Rules:

1. values below the minimum enum value are illegal
2. values above `REPAIR_FATAL` are illegal
3. illegal repair-state values are page corruption
4. repair-state values classify what repair or quarantine logic may do with a
   page image

## 4. Repair-epoch rules

`repair_epoch` is the page-local repair publication epoch.

Rules:

1. repair publication increments page-local evidence without replacing
   transaction truth
2. repair epoch participates in audit, repair, and startup classification
3. repair epoch is not a substitute for committed transaction visibility

## 5. Shared consumer rule

These page-local markers are available to:

- startup validation
- page audit
- repair consumers
- corruption classification

They do not by themselves authorize:

- transaction visibility decisions
- MGA horizon decisions
- restart truth without transaction-manager reconciliation

## 6. Negative requirements

The following are prohibited:

1. using page-local markers as a replacement for `TIP`, `CLOG`, or transaction
   reconciliation
2. describing generation markers as WAL or replay sequence authority
3. treating repair-state markers as proof that family-local payload is valid

## 7. Implementation contract

Any implementation or audit against this file must prove:

1. generation-order validation is enforced
2. repair-state legality is enforced
3. repair markers are exposed to audit and repair consumers
4. page-local markers remain evidence only and do not replace MGA recovery truth
