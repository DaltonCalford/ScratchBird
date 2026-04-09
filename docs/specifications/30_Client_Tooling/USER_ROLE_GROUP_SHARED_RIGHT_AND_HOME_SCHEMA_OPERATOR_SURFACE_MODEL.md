Status: reconstructed_required

# User Role Group Shared Right and Home Schema Operator Surface Model

## Purpose

This document defines the operator-facing inspection surface for user, role, group, shared-right, and home-schema state.

## Canonical Rule

Operators shall be able to inspect identity and authorization state directly rather than inferring it from access failures alone.

## Required Inspection Fields

The operator surface shall preserve:

- object UUID
- object class
- user-visible name
- lifecycle state
- home schema or default namespace where applicable
- active role or group memberships
- shared-right bundle associations
- cluster propagation generation and drift state where applicable

## Mutation Boundary

Inspection privilege remains distinct from mutation privilege. The ability to inspect shared-right and home-schema state does not imply the ability to alter grants, memberships, or propagation state.

## Non-Guarantees

This file does not require one single command grammar. It defines the canonical operator information contract.
