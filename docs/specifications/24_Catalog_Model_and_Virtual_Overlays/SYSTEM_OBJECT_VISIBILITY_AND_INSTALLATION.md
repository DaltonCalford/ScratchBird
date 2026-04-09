# System Object Visibility and Installation

Status: current_authority

## Current authority

System-object visibility is handler-driven.

Current source proves:
- virtual system catalogs are exposed by registered handlers
- `information_schema` and `sys_catalog` are first-class runtime overlays
- engine-specific overlays are conditionally visible when the emulation profile permits registration

## Current installation boundary

Current section authority covers registration and visibility of those system objects through startup and handler registration.

## Non-claims

This file does not claim universal parity for:
- every donor-engine visibility rule
- every installation-time package or listener surface
- every system object narrative outside the current handler registration layer
