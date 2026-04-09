# Code Truth Audit Maintenance Rules

## Purpose

This Beta 1 planning package does not change product code, but it does govern
how the downstream implementation work-plans must preserve audit legibility.

## Rules

1. Every generated downstream work-plan must carry a current
   `SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv`.
2. Every audit anchor must use:
   - project-root-relative implementation path
   - one file-local `unique_search_key`
3. Line-number anchors are prohibited.
4. If a canonical spec already records an implementation anchor, the generated
   downstream plan must reuse or deliberately update that search-key contract;
   it may not silently drift.
5. If no stable search key exists for an implementation seam, the downstream
   plan must treat creation or identification of one as part of the work.
6. Every generated downstream plan must update the affected canonical specs so
   they disclose:
   - implementation status
   - implementation path
   - unique search key
7. Tickets may not close on prose-only evidence. The required audit matrix,
   gate evidence, and canonical spec maintenance must all agree.
8. Split-owner sections must name one primary downstream work-plan for each
   concrete implementation seam so auditors do not have to infer ownership.
