Status: reconstructed_required

# External Identity Home Schema and Shared Right Activation Model

## Purpose

This document defines how external identities map to local principals, home-schema context, and shared-right activation.

## Canonical Rule

External identity mapping does not stop at authentication. It must also determine:

- the bound local principal
- the home-schema or default namespace context
- the shared rights and memberships activated for the session

## Mapping Fields

The canonical mapping record shall preserve:

- external identity source
- external normalized identity
- local principal identity
- home schema or default namespace
- activated roles
- activated groups
- activated shared-right bundles

## Activation Rule

Shared rights are activated only after:

- successful external identity mapping
- local principal bind
- policy validation for the mapped rights bundle

## Home-Schema Rule

The mapped home schema or default namespace is session context only. It does not redefine UUID-based object identity or bypass privilege checks.

## Non-Guarantees

This file does not require every external identity to carry a home-schema concept. It defines the canonical mapping where that feature is admitted.
