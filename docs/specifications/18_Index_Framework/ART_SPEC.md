# Adaptive Radix Tree (ART) Specification

Status: current_authority

## Purpose

This document defines the current ScratchBird ART surface as implemented today.

## Current implementation boundary

The current runtime does not admit a distinct ART on-disk node family as primary implementation authority. The `ART` index type is currently routed through the ordered-family runtime defined by `BTREE_SPEC.md`.

That means:

- prefix and equality semantics are exposed through the parser surface
- ordered-page publication, cleanup, concurrency, and MGA visibility follow the B-tree family contract
- true ART node families (`Node4`, `Node16`, `Node48`, `Node256`) remain target-state only until a distinct storage/runtime path is promoted

## MGA contract

- heap/version truth remains authoritative
- ART-surface entries are candidate locators only
- writes publish through the ordered-family runtime after heap version materialization
- cleanup of obsolete entries waits for heap reclaim proof

## Required optimizer metrics

The ART surface shall currently publish the ordered-family metrics packet plus:

- prefix probe candidate count
- prefix selectivity estimate
- longest common prefix compression ratio
- equality-vs-prefix probe mix

Until a distinct ART runtime exists, these metrics are reported on top of the ordered-family implementation.
