Status: reconstructed_required

# Wrapper Object Security Definer and Sandbox Catalog Model

## Purpose

This document defines the canonical catalog-backed representation of wrapper-object security-definer and sandbox rules.

## Canonical Rule

Wrapper-object mediation shall be backed by explicit catalog state. The runtime must be able to determine from committed metadata whether a wrapper is invoker-context, security-definer, or sandbox-mediated.

## Required Catalog Families

The canonical families are:

- wrapper security-mode rows
- wrapper owner or definer rows
- wrapper-to-underlying dependency rows
- wrapper mediation policy rows
- wrapper audit-policy rows

## Required Fields

These rows shall preserve:

- wrapper object identity
- wrapper object class
- security mode
- definer or owner identity
- underlying object identities
- mediation scope
- active generation

## Commit Rule

Changes to wrapper mediation rules become effective only through committed catalog publication under ordinary MGA transaction rules.

## Non-Guarantees

This file does not require all wrapper-object metadata to live in one physical table. It requires explicit committed row families and committed publication semantics.
