# Normative Listener Passthrough and Live Migration Checklist

## Current status

Passthrough and live migration runtime is not part of the shipped section `29`
listener implementation.

## Required interpretation

- Do not treat passthrough routing or live migration as current listener
  capability.
- Retry schedules, migration phases, and passthrough route guarantees described
  here are not shipped runtime authority.
- Current listener admission, handoff, and drain rules remain the only section
  `29` implementation authority.
