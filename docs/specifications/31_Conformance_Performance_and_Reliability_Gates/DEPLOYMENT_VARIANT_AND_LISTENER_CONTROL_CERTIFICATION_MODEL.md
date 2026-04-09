Status: reconstructed_required

# Deployment Variant and Listener Control Certification Model

## Purpose

This document defines the certification evidence required for the recovered deployment-variant and database-controlled listener design.

## Required Certification Classes

Certification shall cover:

- direct embedded engine deployment
- embedded parser-plus-engine deployment
- local IPC shared-server deployment
- listener plus parser-agent deployment
- manager-fronted listener deployment
- database-controlled listener configuration propagation

## Required Evidence

Each certification case shall preserve:

- deployment variant identity
- active layer set
- listener topology generation if applicable
- parser-pool policy if applicable
- control operation attempted
- apply or refusal result

## Failure Criteria

Certification fails when:

- a deployment variant reports layers that are not actually present
- listener topology changes cannot be traced back to database-controlled configuration
- parser-pool changes bypass the canonical control chain
- a manager-fronted deployment cannot explain its inner-listener target

## Non-Guarantees

This file does not require every deployment variant to be fully implemented today. It defines the certification targets for the recovered architecture.
