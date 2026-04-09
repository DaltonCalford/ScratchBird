# Control-Plane Log Schema Surface

Implemented catalog families backing control-plane configuration/runtime state:
- Clock policy/source/state/violation families.
- Cluster fabric link/session/txn/task/chunk/event/error families.

Key constraints exercised in tests:
- Uniqueness constraints on policy/source/link/service identities.
- State transition/version constraints (stale updates rejected).
- Required-field validation by state (invalid terminal/incomplete payloads rejected).
