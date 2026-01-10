# Reference Documentation

**[← Back to Specifications Index](../README.md)**

This directory contains reference documentation from other database systems that inform ScratchBird's design and compatibility.

## Overview

Reference materials from Firebird, PostgreSQL, MySQL, and other databases that ScratchBird emulates or draws inspiration from.

## Contents

### Firebird Reference

- **[firebird/](firebird/)** - Complete Firebird reference documentation
  - Firebird 5.0 Language Reference
  - Firebird SQL Reference Document
  - ISQL Reference
  - Transaction Features

### Replication Research

- **[UUIDv7 Replication System Design.md](UUIDv7%20Replication%20System%20Design.md)** - UUIDv7-based replication research (superseded by [Beta Requirements - Replication](../beta_requirements/replication/))

## Purpose

These reference documents serve as:

- **Compatibility targets** - Define behavior to emulate
- **Design inspiration** - Inform ScratchBird's design decisions
- **Feature parity** - Ensure equivalent functionality
- **Testing baselines** - Compare behavior for compatibility testing

## Related Specifications

- [Parser](../parser/) - SQL dialect parsing
- [Transaction System](../transaction/) - MGA implementation based on Firebird
- [Beta Requirements](../beta_requirements/) - Compatibility specifications

## Navigation

- **Parent Directory:** [Specifications Index](../README.md)
- **Child Directories:**
  - [Firebird Reference](firebird/README.md)
- **Project Root:** [ScratchBird Home](../../../README.md)

---

**Last Updated:** January 2026
