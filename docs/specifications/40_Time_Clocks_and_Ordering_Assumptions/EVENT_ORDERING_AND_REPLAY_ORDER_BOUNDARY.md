# Event Ordering and Replay Order Boundary

## Purpose

This file defines how event order may be interpreted in the current system.

## Current rules

Core correctness order for transactions remains anchored to MGA publication and visibility semantics.

Replay order claims must be narrower than transaction correctness claims and must be tied to explicit replay evidence such as transaction lineage, committed schema epochs, forensic snapshot capsules, and restore or replay contracts owned by sections `08`, `35`, `24`, `37`, and adjacent recovery sections.

Artifact timestamps, log timestamps, or capture order do not by themselves imply total historical runtime order.

Cross-surface chronology is not unified by default. A control-plane log, diagnostic artifact, audit record, and runtime event stream do not automatically share one total order.

## Explicit exclusions

There is no universal event-log ordering contract.

There is no event-sourcing architecture claim.

There is no guaranteed total replay chronology across all subsystems or artifact families.
