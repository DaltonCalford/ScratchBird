Status: reconstructed_required

# Listener Parser Agent Pool and IP Handoff Model

## Purpose

This document defines the canonical listener role in the normal network stack and the parser-agent pool it manages.

## Canonical Rule

Listeners own inbound IP connection handling and parser-agent pool management. They do not own SQL parsing semantics themselves and they do not execute engine semantics directly.

## Listener Responsibilities

The listener owns:

- inbound connection attempts
- protocol-family admission
- session establishment at the transport boundary
- parser-agent pool sizing and assignment
- handoff of validated connection context to a parser agent

## Parser Agent Role

A parser agent is the normal stack form that:

- receives the listener handoff
- uses a parser library for dialect-local SQL handling
- uses the IPC library
- talks to the threaded IPC server backed by the engine library

## Pool Rule

The listener shall manage a pool of parser-agent workers or stacks for the admitted protocol family. Pool sizing, reload, and runtime control remain bounded by the listener-management seam and engine-controlled policy.

## Handoff Rule

The IP handoff shall preserve:

- connection identity
- authenticated transport context if any
- selected protocol or emulation family
- parser-agent assignment
- required routing or manager-binding context

## Non-Guarantees

This file does not require the listener itself to become a parser library host for all logic. The canonical model is listener handoff into parser agents.
