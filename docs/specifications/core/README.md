# Core System Specifications

**[← Back to Specifications Index](../README.md)**

This directory contains core system specifications for ScratchBird's internal architecture and infrastructure components.

## Overview

The core system specifications define the fundamental architecture, internal APIs, thread safety models, and critical infrastructure that underlies all other ScratchBird components.

**MGA Reference:** See `MGA_RULES.md` for Multi-Generational Architecture semantics (visibility, TIP usage, recovery).

## Specifications in this Directory

### Architecture & Infrastructure

- **[Y_VALVE_ARCHITECTURE.md](Y_VALVE_ARCHITECTURE.md)** - Listener/pool architecture (legacy Y-Valve spec)
- **[THREAD_SAFETY.md](THREAD_SAFETY.md)** - Thread safety models and concurrency control
- **[IMPLEMENTATION_RECOMMENDATIONS.md](IMPLEMENTATION_RECOMMENDATIONS.md)** - Strategic implementation guidance and recommendations
- **[CACHE_AND_BUFFER_ARCHITECTURE.md](CACHE_AND_BUFFER_ARCHITECTURE.md)** - Cache/buffer architecture (current state + Beta target)

### Core Features

- **[GIT_METADATA_INTEGRATION_SPECIFICATION.md](GIT_METADATA_INTEGRATION_SPECIFICATION.md)** - Git version control integration for database objects
- **[LIVE_MIGRATION_PASSTHROUGH_SPECIFICATION.md](LIVE_MIGRATION_PASSTHROUGH_SPECIFICATION.md)** - Live migration support and passthrough mechanisms
- **[INTERNAL_FUNCTIONS.md](INTERNAL_FUNCTIONS.md)** - Internal function implementations and APIs

### System Summaries

- **[ENGINE_CORE_UNIFIED_SPEC.md](ENGINE_CORE_UNIFIED_SPEC.md)** - Unified Alpha core engine specification (consolidated)
- **[CORE_IMPLEMENTATION_SPECS_SUMMARY.md](CORE_IMPLEMENTATION_SPECS_SUMMARY.md)** - Summary of core implementation specifications
- **[design_limits.md](design_limits.md)** - System design limits and constraints

## Related Specifications

- [Storage Engine](../storage/) - Storage layer implementation
- [Transaction System](../transaction/) - MGA transaction management
- [Catalog System](../catalog/) - System catalog structure
- [Network Layer](../network/) - Network protocols and listener/pool integration

## Navigation

- **Parent Directory:** [Specifications Index](../README.md)
- **Project Root:** [ScratchBird Home](../../../README.md)

---

**Last Updated:** January 2026
