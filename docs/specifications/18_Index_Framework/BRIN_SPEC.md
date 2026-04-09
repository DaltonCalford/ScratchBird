# BRIN Specification

Status: current_authority

## Purpose

This document defines the summary/range-pruning family used to skip page or block ranges rather than to locate final row truth directly.

## ScratchBird shipped type coverage

Current runtime routes the following index types through the BRIN-family runtime:

- `BRIN`
- `ZONEMAP`
- `BLOOM` current runtime summary path

## MGA-first contract

- summaries describe candidate page or block ranges only
- summaries never override heap/version visibility
- inserts and updates refresh or invalidate summaries after heap/version materialization
- deletes never tighten visibility truth in-place; they only contribute to invalidation or resummarization debt
- stale summaries are tolerated if they over-admit candidates, not if they under-admit visible truth

## Search contract

- planner or executor uses summaries to identify candidate ranges
- candidate ranges are scanned against heap truth
- every returned row must still pass MGA visibility
- false-positive ranges are expected and must be measured

## Summary maintenance rules

- range invalidation is permitted when exact tightening cannot be proven cheaply
- autosummarize or background summarization is maintenance only
- summary refresh must not outrun durable heap truth
- cleanup of stale summary state is downstream of heap reclaim and maintenance policy

## Required optimizer metrics

The summary-family metrics packet shall include at minimum:

- pages or blocks per range
- summarized range count
- invalid range count
- unsummarized range count
- range skip ratio
- range false-positive ratio
- refresh lag / summarization debt
- deleted-row pressure per range where tracked
- metrics freshness and confidence
