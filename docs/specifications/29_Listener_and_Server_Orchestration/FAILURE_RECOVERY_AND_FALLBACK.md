# Failure Recovery and Fallback

## Startup refusal classes

Listener startup must fail closed for:
- missing engine endpoint
- invalid listener mode token
- managed mode with non-loopback bind
- direct mode with proxy-binding requirement
- invalid `pool_min` or `pool_max`
- failure to create front-door socket
- failure to create control socket
- failure to create management socket
- failure to reach the warm pool minimum before startup admission

## Runtime degradation classes

Current runtime degradation classes are:
- drain mode
- queue saturation
- worker fault
- management reload failure
- managed-mode validation failure

## Drain behavior

Graceful stop behavior:
- set drain flag
- stop admitting new work
- allow active sessions to complete
- exit when active session count reaches zero

Force stop behavior:
- set force-shutdown flag
- set global shutdown flag
- terminate promptly without waiting for sessions to drain

## Reload behavior

Reload is config-file backed.

Reload may fail for:
- no config path
- config parse failure
- invalid runtime pool bounds

On reload failure, the existing live runtime remains authoritative.

## Worker fault behavior

If a worker faults:
- the pool records an error or recycle event
- the faulted worker stops receiving work
- replenishment may spawn replacement capacity toward `pool_min`

## Managed-mode validation failure

Invalid `DBBT` or `LPREFACE` handling must:
- reject the binding attempt
- emit a deterministic failure reason
- keep the listener bound to its current owner-database policy
- avoid any partial bind or cross-database ambiguity

## Platform boundary

Windows current limitation:
- local listener management IPC is not implemented

That is a platform limitation, not a fallback path.

## Hard boundaries

- No network-partition policy matrix is current authority here.
- No live migration or cross-host fallback policy is current authority here.
- No localhost failover or replica reroute runtime exists in the shipped
  section `29` path.
