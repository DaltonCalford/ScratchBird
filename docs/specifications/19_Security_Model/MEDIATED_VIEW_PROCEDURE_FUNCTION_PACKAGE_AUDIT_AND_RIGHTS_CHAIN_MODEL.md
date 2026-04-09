Status: reconstructed_required

# Mediated View Procedure Function Package Audit and Rights Chain Model

## Purpose

This document defines the canonical audit and rights-chain model for mediated access through views, procedures, functions, and packages.

## Canonical Rule

Mediated access shall preserve a rights chain explaining:

- why the caller could use the wrapper object
- why the wrapper could touch underlying objects
- why the caller still lacked direct underlying-object rights if applicable

## Required Rights-Chain Elements

The audit chain shall preserve:

- caller principal
- wrapper object identity
- wrapper grant source
- wrapper owner or definer identity
- underlying object identities
- underlying access reason
- caller direct-object rights state
- final allow, deny, or mask outcome

## Covered Object Classes

The canonical mediated classes are:

- views
- procedures
- functions
- packages
- package members

## Direct-Deny Preservation

If the caller has no direct right to an underlying object, the rights chain shall preserve that fact even when the wrapper invocation succeeds.

## Non-Guarantees

This file does not require one log format. It requires the rights chain to be reconstructable from canonical audit evidence.
