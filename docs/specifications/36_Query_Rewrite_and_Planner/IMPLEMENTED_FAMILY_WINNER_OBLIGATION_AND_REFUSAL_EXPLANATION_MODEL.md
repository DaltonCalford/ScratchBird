Status: reconstructed_required

# Implemented Family Winner Obligation and Refusal Explanation Model

## Purpose

This document defines the optimizer obligation to provide either winning coverage or explicit refusal explanation for every implemented index family.

## Canonical Rule

For every implemented family, the optimizer owes one of two things on a relevant workload:

- a path by which that family can win when conditions justify it
- an explicit refusal explanation when it cannot participate

Silent non-participation is non-conforming.

## Winner Obligation

If a family is implemented and semantically legal for a workload, the optimizer framework shall be capable of producing a winning outcome for that family under some documented admissible conditions.

## Refusal Explanation Rule

If a family does not participate on a given workload, the trace shall preserve:

- whether it was illegal, unsupported, or refused
- the exact refusal class
- whether metrics, memory, accelerator, or policy conditions caused the refusal

## Certification Link

Section 31 certification shall prove both:

- winner coverage for each implemented family
- refusal explanation coverage for cases where a family is absent

## Non-Guarantees

This file does not require every family to be best for every workload. It requires no implemented family to disappear without accountable explanation.
