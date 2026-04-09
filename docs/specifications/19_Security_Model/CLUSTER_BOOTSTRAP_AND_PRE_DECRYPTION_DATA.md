# Cluster Bootstrap and Pre-Decryption Data

Status: current_authority

## Pre-decryption disclosure boundary

Before protected data is opened, only minimal bootstrap metadata may be visible, such as:

- format and compatibility markers
- encryption profile identifiers
- key-slot or key-descriptor identifiers
- page-size or structural bootstrap markers required to locate protected material
- node or channel identity descriptors needed to start the security bootstrap path

The following must not appear as pre-decryption plaintext bootstrap data:

- user data rows
- committed catalog object definitions beyond minimal bootstrap descriptors
- password material or reusable secrets
- authentication proofs

## Cluster boundary

Current authority is limited to configured local or explicitly provisioned channel bootstrap material. Automated distributed cluster trust establishment beyond those configured channels is unsupported and must be rejected rather than approximated.
