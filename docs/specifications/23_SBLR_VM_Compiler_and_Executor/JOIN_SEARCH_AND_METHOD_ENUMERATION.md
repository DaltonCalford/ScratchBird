# Join Search and Method Enumeration

Status: current_authority

## Current authority

Authoritative source anchors:
- src/optimizer/join_ordering.cpp
- include/scratchbird/optimizer/join_ordering.h
- include/scratchbird/optimizer/join_legality.h
- runtime-plan join summary fields in plan_payload.h

## Current guarantees

- real bounded join-order search
- legality-aware method selection
- runtime-plan search summaries and rejection counts for the code paths that populate them
- preservation of explicit barriers such as outer, semi, anti, natural, using, and lateral constraints where the implementation models them

## Non-guarantees

- no donor-style exhaustive global search claim is made for every join workload
- no distributed or cluster-aware join orchestration is claimed here
