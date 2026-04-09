# Emulation Storage Page Types

Status: current_authority

## Current authority

Current code-backed authority includes:
- reserved emulation-facing page-type assignments in the durable page-type enum
- durable emulation-profile catalog state
- durable emulation-type and emulation-server catalog state
- session or connection emulation-mode state

## Current guarantees

- reserved emulation page families are part of the durable enum space
- durable emulation profile and session emulation mode are distinct concerns
- enum reservation does not imply that each family has a complete storage-engine implementation

## Non-claims

This file does not claim:
- exact Alpha default mapping for every donor engine unless separately proven
- that every reserved emulation family has allocator, writer, reader, integrity, repair, and maintenance ownership
- that disabled-emulation references already map to one universal runtime error contract in every path
