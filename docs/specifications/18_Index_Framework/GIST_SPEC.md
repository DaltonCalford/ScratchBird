# GiST Specification

Status: current_authority

## Purpose

This document defines the generalized search-tree family for extensible predicate routing with mandatory recheck where the opclass requires it.

## ScratchBird shipped type coverage

Current runtime exposes `GIST` as a distinct generalized search-tree family.

## MGA-first contract

- GiST nodes route to candidate regions or candidate row references only
- opclass-consistent routing may over-admit candidates but must not under-admit visible truth
- final acceptance requires heap fetch, MGA visibility recheck, and opclass exact recheck where required
- cleanup of stale entries waits for heap reclaim proof

## Search contract

- search uses strategy-number or family-equivalent predicate routing
- overlap and penalty rules are structural routing aids only
- recheck-required results must record both structural candidate count and exact recheck count

## Required optimizer metrics

The GiST-family metrics packet shall include at minimum:

- tree depth
- page count by level
- overlap rate
- split rate
- candidate count per strategy class
- exact recheck rate
- dead-entry debt
- MGA visibility reject rate
- metrics freshness and confidence
