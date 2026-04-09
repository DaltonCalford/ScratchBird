# Spatial Index Specification

Status: current_authority

## Purpose

This document defines the spatial bounding-region family used for geometric and geo candidate routing.

## ScratchBird shipped type coverage

Current runtime routes the following index types through the spatial family:

- `RTREE`
- `MONGODB_2D`
- `MONGODB_2DSPHERE`
- `MONGODB_2DSPHERE_BUCKET`
- `REDIS_GEO`

## MGA-first contract

- spatial nodes store candidate bounding regions, not visibility truth
- exact geometry acceptance and MGA visibility are both mandatory before a row is returned
- updates materialize new heap/version truth before spatial entry publication
- obsolete spatial entries are retained until heap reclaim proof authorizes cleanup

## Search contract

- region or nearest-neighbor routing may over-admit candidates
- exact geometry recheck remains mandatory for any family that stores approximations or bounding boxes
- visibility reject rate must be tracked separately from geometry recheck reject rate

## Maintenance rules

- split, merge, and reinsertion are structural maintenance only
- dead spatial entries may be removed only after heap reclaim proof
- damaged routing state blocks destructive cleanup on the affected segment

## Required optimizer metrics

The spatial-family metrics packet shall include at minimum:

- tree depth
- node overlap rate
- bounding-region enlargement rate
- candidate expansion count
- exact geometry recheck rate
- dead-entry debt
- MGA visibility reject rate
- split and reinsertion rate
- metrics freshness and confidence
