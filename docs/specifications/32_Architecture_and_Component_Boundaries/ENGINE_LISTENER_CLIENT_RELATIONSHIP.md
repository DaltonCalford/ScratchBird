# Engine Listener Client Relationship

ScratchBird architecture is bounded to three distinct top-level roles:
- engine/runtime truth
- listener or server orchestration truth
- client/tooling truth

Current guarantees:
- engine truth is not the same thing as parser or client truth
- listener/session surfaces are not a license to widen engine semantics
- client tooling surfaces are bounded APIs and operational wrappers, not authority over core engine behavior

This section exists so audit and implementation work can answer where each layer starts and stops without recomputing that boundary from several other sections.
