Status: reconstructed_required

# Transaction Reattach Common Handle and Restart Survivability Certification Model

## Purpose

This document defines the certification evidence required for dormant reattach, common-transaction handles, and restart-time rebind.

## Required Certification Classes

Certification shall cover:

- dormant detach then same-session reattach
- dormant detach then new-session reattach
- restart replacement reattach
- common transaction multi-attachment where admitted
- principal-mismatch refusal
- retired-handle refusal

## Required Evidence

Each certification case shall preserve:

- transaction identity
- handle generation
- principal identity
- database identity
- bind count
- reattach or bind outcome
- final commit or rollback outcome

## Failure Criteria

Certification fails when:

- a successful reattach changes transaction truth silently
- commit or rollback on a common handle leaves other bound attachments in ambiguous state
- principal mismatch does not cause refusal
- restart replacement rebind cannot explain whether the transaction was reconstructed or refused

## Non-Guarantees

This file does not require every current surface to pass every case today. It defines the certification target for the recovered design.
