Status: reconstructed_required

# Shared Right Bundle Propagation Package View Procedure and Function Model

## Purpose

This document defines how shared-right bundles propagate through executable and view-like surfaces while preserving underlying-object deny boundaries.

## Canonical Rule

Shared-right bundles may activate rights to invoke admitted wrapper objects, but they do not automatically propagate direct rights to every underlying object those wrappers use.

## Covered Surface Classes

The canonical wrapper or mediated surface classes are:

- views
- procedures
- functions
- packages
- other admitted security-definer or sandboxed callable objects

## Propagation Rule

A shared-right bundle may grant:

- visibility of the wrapper object
- execute rights on callable wrapper objects
- select rights on mediated view surfaces where admitted

The same bundle shall not silently grant:

- direct table rights
- direct underlying function rights
- direct underlying package member rights

unless those are separately granted.

## Audit Rule

The engine shall preserve:

- the shared-right bundle identity
- the wrapper object invoked
- the mediated underlying object set
- the remaining direct-object deny state

## Non-Guarantees

This file does not require every wrapper surface to participate in shared-right bundles. It defines the canonical propagation rule where they do.
