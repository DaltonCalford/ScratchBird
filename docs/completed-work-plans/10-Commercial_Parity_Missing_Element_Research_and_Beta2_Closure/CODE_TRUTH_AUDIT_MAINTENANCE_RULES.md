# Code Truth Audit Maintenance Rules

1. Local ScratchBird code and canonical spec boundaries are read before any web
   research claim is accepted.
2. Donor behavior claims should prefer:
   - local donor clone source
   - official vendor documentation
   - standards documentation
   - official open-source driver or engine source
   - whitepapers from original implementers or authors
3. Research packets must distinguish:
   - proven current ScratchBird behavior
   - planned canon
   - donor behavior
   - inferred implementation option
4. No final Beta 2 spec may rely on line-number-only pointers for durable
   internal anchors; use stable `path + unique_search_key` references where the
   package needs durable audit anchors.
5. No research artifact may cite a downloaded source without storing that source
   under the canonical reference tree and indexing it in the relevant manifest.
6. MGA invariants outrank donor familiarity:
   - donor WAL, redo, log-ship, or journal designs may inspire extensions, but
     they must not replace MGA core truth
   - archived or derivative lanes must remain subordinate unless a new canon
     explicitly promotes them with proof
7. When a current canonical file overclaims reality, the resulting Beta 2 work
   must either:
   - narrow the old file
   - or supersede it with a clearer owning file
   - but never leave contradiction unrecorded
