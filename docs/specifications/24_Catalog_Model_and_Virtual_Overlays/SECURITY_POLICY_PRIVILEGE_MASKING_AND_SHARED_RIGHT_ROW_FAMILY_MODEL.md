Status: reconstructed_required

# Security Policy Privilege Masking and Shared Right Row Family Model

## Purpose

This document defines the canonical catalog row families needed to persist privilege classes, masking policy, shared-right bundles, and their activation context.

## Canonical Rule

Security and masking behavior shall be backed by explicit catalog row families. Effective security state is not an undocumented blend of hard-coded flags and ephemeral memory only.

## Canonical Row Families

The canonical families are:

- privilege grant rows
- privilege revoke rows
- shared-right bundle rows
- shared-right membership rows
- row-policy rows
- column-policy rows
- domain-policy rows
- masking-policy rows
- wrapper-object mediation rows

## Required Fields

These rows shall preserve enough identity to explain:

- grantor
- grantee
- privilege class
- object or domain target
- bundle identity where relevant
- policy precedence where relevant
- active or retired generation

## Activation Rule

Shared-right bundles and policy rows become effective only through committed catalog state under ordinary MGA transaction rules. They are not published out of band.

## Non-Guarantees

This file does not require every future security feature to use a new row family. It defines the canonical row-family substrate for the recovered security model.
