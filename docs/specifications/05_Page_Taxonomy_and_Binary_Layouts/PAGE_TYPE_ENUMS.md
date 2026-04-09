# Page Type Enums

Status: current_authority

## Current authority

The durable `PageType` enum in `ondisk.h` is the numeric authority for page-family identity.

## Current taxonomy classes

Current page-type space includes classes for:
- bootstrap and control pages
- heap and heap-adjacent storage pages
- index and index-adjacent storage pages
- temporary or work pages where explicitly flagged
- reserved emulation-facing families including document, wide-column, graph, vector, and Redis-related families

## Rules

- page type is a durable page-family discriminator
- page-type legality is validated against the shared page contract
- a reserved page type may exist before full family-local implementation exists
- family-local readers and writers must fail closed on unsupported or mismatched page types

## Non-claims

This file does not claim full producer, consumer, repair, or maintenance coverage for every reserved page type.
