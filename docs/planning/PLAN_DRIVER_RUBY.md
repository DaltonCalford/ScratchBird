# Driver Plan: Ruby

Status: Draft

Spec references:
- docs/specifications/beta_requirements/00_DRIVERS_AND_INTEGRATIONS_INDEX.md
- docs/specifications/drivers/ALPHA_DRIVER_BOOTSTRAP.md
- docs/specifications/beta_requirements/drivers/DRIVER_BASELINE_SPEC.md

Goal
- Deliver a Ruby driver suitable for Rails ActiveRecord.

Scope
- Native SBWP via C extension or FFI.

Plan
1) Driver API
- Implement Ruby DB driver interfaces (Sequel/ActiveRecord adapter).
- Connection string parsing and config mapping.

2) Protocol integration
- Use libscratchbird via native extension.
- TLS/auth handling and autocommit mapping.

3) Types + streaming
- Implement type conversions and enumerable row streaming.

4) Packaging
- RubyGems package with native extension.

Testing
- Unit tests for adapter behavior.
- Integration tests with listener.
- Rails ActiveRecord smoke tests.

Dependencies
- libscratchbird client core.
