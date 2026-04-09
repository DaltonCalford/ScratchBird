# Forensic Replay Session Profile

## Current status

Forensic replay session transport is not part of the shipped current section
`26` native or IPC transport contract.

## Required interpretation

- Do not implement current native or IPC transport from this document.
- Do not claim replay-session capability negotiation, bind semantics, or replay
  result metadata as shipped section `26` behavior.
- Any future replay transport promotion requires direct runtime code and
  maintained tests before this file can become active implementation authority.
