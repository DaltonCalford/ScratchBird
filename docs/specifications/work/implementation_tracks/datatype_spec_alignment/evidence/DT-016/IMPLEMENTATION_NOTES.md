# Implementation Notes

- Added `DomainListOptions` and `listDomainsVisible(...)` to provide deterministic visibility filtering.
- Visibility policy reads enabled emulation profiles from catalog and normalizes engine tags.
- System domains remain loaded in engine but are conditionally surfaced per parser dialect/profile.
