Status: reconstructed_required

# Donor Without Force Transaction Capture and Operator Risk Acceptance Model

## Purpose

This document defines the canonical treatment of donors that do not support forced transactions, durable commit fencing, or equivalent guarantees.

## Canonical Rule

If a donor cannot force or fence transaction durability, ScratchBird shall classify that donor as risk-bearing and require explicit operator-visible acceptance of the residual uncertainty before final promotion or cutover.

## Required Donor Classification

The capture layer shall preserve:

- donor identity
- absence of forced-transaction capability
- extraction mode
- replay ordering basis if any
- residual uncertainty class

## Risk Acceptance Rule

Final cutover or migration completion for such a donor shall require either:

- stronger external quiesce proof
- stronger divergence evidence
- explicit operator risk acceptance

## Risk Acceptance Record

The risk-acceptance record shall preserve:

- donor identity
- uncertainty class
- missing donor guarantees
- operator or policy actor accepting the risk
- affected objects or target scope
- timestamp of acceptance

## Non-Guarantees

This file does not claim donors without forced transactions can be elevated to zero-risk migration sources. It defines how to handle the remaining risk honestly.
