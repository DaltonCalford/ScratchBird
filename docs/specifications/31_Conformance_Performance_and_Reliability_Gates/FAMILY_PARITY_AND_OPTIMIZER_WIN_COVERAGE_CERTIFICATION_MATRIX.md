Status: reconstructed_required

# Family Parity and Optimizer Win Coverage Certification Matrix

## Purpose

This document defines the certification matrix required to prove that all implemented index families are primary-class optimizer candidates and can win where appropriate.

## Canonical Rule

Certification is incomplete until every implemented family has explicit win-coverage evidence under a workload where that family should be preferred.

## Matrix Dimensions

The certification matrix shall preserve:

- family identity
- workload identity
- metrics freshness class
- accelerator state where applicable
- resident-memory state where applicable
- expected winner family
- actual winner family
- correctness result

## Required Rows

For each implemented family, the matrix shall include at minimum:

- one row where the family is expected to win
- one row where the family is expected to lose for an explained reason
- one row where metrics staleness or degradation is present

## Failure Rule

The certification matrix fails when:

- a family lacks an expected-win row
- a family cannot appear as actual winner where expected
- the explanation trace cannot account for the result

## Non-Guarantees

This file does not require all families to be benchmarked under identical workloads. It requires sufficient workload coverage to prove primary-class planning parity.
