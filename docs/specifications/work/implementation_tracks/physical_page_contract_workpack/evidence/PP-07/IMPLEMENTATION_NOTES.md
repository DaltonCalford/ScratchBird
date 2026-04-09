# Implementation Notes

- Primary page-manager FSM now reserves canonical fixed pages `0..5`.
- Added hard guard against freeing bootstrap pages.
- FSM page buffer writes checksum-valid headers for corruption detection.
