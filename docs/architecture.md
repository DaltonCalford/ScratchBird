# Architecture

ScratchBird separates concerns into:
- Engine library (embeddable): parser, optimizer, executor, storage, locks, intl, plugins.
- Server: listener + y-valve dispatch + auth; forwards to engine or legacy providers.
- Y-Valve dispatch chooses provider based on target and protocol
- Utilities: embed for local files; proxy for remote connections.
