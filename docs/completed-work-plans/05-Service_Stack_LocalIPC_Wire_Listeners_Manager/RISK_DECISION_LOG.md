# Risk Decision Log

## Fixed Decisions

- B1-05-001 must close specification sufficiency before any implementation
  ticket begins
- the local reference tree under docs/reference is the primary donor and
  authority intake surface for this lane
- section `24` listener-topology and remote-management persistence files are
  required consumed dependencies for this package; they are not optional
  background reading
- section `30` fixes the operator-facing remote-management command names and
  result families while package `05` remains responsible for the underlying
  manager control and service-stack substrate
- Windows listener-management IPC remains an explicit fail-closed current
  platform limitation; later tickets must not invent an unsupported fallback
- the stronger manager heartbeat publication remote-drift and queued
  instruction model remains active Beta 1 package scope, but this package
  closes it at the bounded manager-status, listener-control, parser-pool, and
  persisted deployment substrate it directly owns
- lane A owns local IPC session identity threaded-server and deployment-boundary
  closure first; lane B owns wire handshake listener manager and topology work
  after that foundation is frozen
- the cluster-fabric and remote-connector catalog families already present in
  `CatalogManager` are the cluster-side persistence substrate consumed by this
  package; follow-on tooling or cluster transport work must reuse them instead
  of inventing a second deployment record model

## Active Risk

Risk: no active implementation risk remains inside this package. Follow-on work
must preserve the library-first layering and the manager-owned control seam
rather than collapsing listener, engine, and operator-facing control into one
monolith.

## Final Closeout Note

All bounded tickets for package `05` are complete. Lane A and lane B proof is
preserved, the front-door benchmark artifact is recorded for the touched
section `31` surface, and the directory is ready to archive under
`docs/completed-work-plans/05-Service_Stack_LocalIPC_Wire_Listeners_Manager/`.
