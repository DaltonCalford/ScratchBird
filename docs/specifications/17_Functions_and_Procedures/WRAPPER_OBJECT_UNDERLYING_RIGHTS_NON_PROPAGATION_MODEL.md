Status: reconstructed_required

# Wrapper Object Underlying Rights Non Propagation Model

## Purpose

This document defines the canonical non-propagation rule for wrapper objects such as views, procedures, functions, and packages.

## Canonical Rule

Rights to invoke or read through a wrapper object do not propagate into general-purpose direct rights on the wrapper’s underlying objects.

## Covered Wrapper Objects

The canonical wrapper-object classes are:

- views
- procedures
- functions
- packages
- package members

## Non-Propagation Rule

If a caller receives access through a wrapper object:

- that access is limited to the wrapper object contract
- the caller does not automatically receive direct `SELECT`, `UPDATE`, `DELETE`, `REFERENCES`, or `EXECUTE` rights on underlying objects
- direct access checks on underlying objects remain unchanged unless separately granted

## Audit Rule

The runtime shall be able to demonstrate:

- wrapper object invoked
- underlying object set consulted
- direct underlying-object deny state retained for the caller
- final mediated result

## Non-Guarantees

This file does not define the SQL grant syntax for wrapper objects. It defines the canonical no-propagation rule.
