# Code Truth Audit Maintenance Rules

## Search-Key Rule

All implementation references recorded by this package must use:

- implementation path
- unique search key

They must not use line numbers.

## Canonical Evidence Rule

When a ticket claims an existing ScratchBird substrate exists, it must record:

- the current canonical spec path
- the current code path when implementation exists
- the exact bounded search key proving the claim

## Research Packet Rule

Every research ticket must leave behind a stable packet under
`evidence/<ticket>/` describing:

- current ScratchBird truth
- external sources gathered
- best-fit design choice
- rejected donor or open-source options
- implementation-ready process flow

## Closeout Rule

No ticket is complete until:

- its evidence packet exists
- any new canonical spec file is created or expanded
- section `README.md` indexes are synced where needed
- the package trackers are updated

## MGA Rule

Every ticket must state how the resulting design preserves:

- MGA truth
- UUID identity authority
- parser boundary rules
- non-authoritative status of derivative logs and donor journals
