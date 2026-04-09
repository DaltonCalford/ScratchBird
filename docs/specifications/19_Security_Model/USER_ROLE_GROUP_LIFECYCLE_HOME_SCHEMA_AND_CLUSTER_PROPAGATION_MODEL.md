Status: reconstructed_required

# User Role Group Lifecycle Home Schema and Cluster Propagation Model

## Purpose

This document defines the canonical lifecycle for users, roles, groups, their home-schema context, and their cluster propagation state.

## Canonical Rule

Users, roles, and groups are first-class security objects with UUID-backed identity, explicit lifecycle state, and optional cluster propagation state. Their home-schema or default namespace context is part of the bound identity model, not an afterthought.

## Lifecycle States

The canonical lifecycle states are:

- `CREATED`
- `ACTIVE`
- `SUSPENDED`
- `REVOKED`
- `RETIRED`

These states apply independently to users, roles, and groups according to object class.

## Required Object Fields

Each user, role, or group object shall preserve:

- system UUID
- user-visible name
- object class
- lifecycle state
- home schema or default namespace where applicable
- cluster propagation generation where applicable

## Home-Schema Rule

Home schema is session context attached to the bound principal. It influences default object lookup and user-facing context, but it does not replace UUID-bound object identity or privilege checks.

## Cluster Propagation Rule

If users, roles, or groups are propagated through the cluster layer:

- the propagated object keeps the same system UUID
- the propagated home-schema context keeps the same canonical meaning
- lifecycle state changes propagate explicitly and audibly

## Non-Guarantees

This file does not require every deployment to use cluster-propagated users, roles, or groups. It defines the canonical model where those features are admitted.
