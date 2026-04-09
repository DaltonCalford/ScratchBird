# Code Truth Audit Maintenance Rules

1. Keep SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv current throughout execution.
2. Use only project-root-relative implementation paths plus one file-local
   unique_search_key per row.
3. Never use line numbers.
4. If no stable search key exists, treat creation or identification of one as
   part of the current ticket.
5. When a ticket changes an implementation seam, update the canonical spec so
   auditors can find the code by search key after the change.
