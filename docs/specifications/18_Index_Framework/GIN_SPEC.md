# GIN Specification

Status: current_authority

## Purpose

This document defines the generalized inverted index family for multi-key extraction workloads.

## ScratchBird shipped type coverage

Current runtime exposes `GIN` as a distinct generalized inverted family.

## MGA-first contract

- extracted keys map to candidate postings only
- the extractor may yield zero, one, or many keys per row version
- heap/version truth is authoritative for visibility and conflict handling
- obsolete postings remain retained until heap reclaim proof permits cleanup

## Search contract

- GIN supports extractor-driven candidate discovery, not ordered range scans
- query operators map to extracted keys and posting intersections or unions
- final row acceptance requires MGA visibility recheck and any operator-specific exact recheck

## Cleanup and maintenance rules

- posting cleanup is downstream of heap reclaim proof
- entry-page splits and posting-tree maintenance are structural only
- pending-list or deferred-merge behavior, if present, must preserve visibility correctness and metrics fidelity

## Required optimizer metrics

The GIN-family metrics packet shall include at minimum:

- extracted keys per row average
- posting-list depth or posting-tree depth
- pending or deferred-merge debt
- candidate count per probe
- exact recheck rate
- dead-posting debt
- MGA visibility reject rate
- metrics freshness and confidence
