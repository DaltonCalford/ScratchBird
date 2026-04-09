Status: reconstructed_required

# Library Composition Executable Variants and Packaging Model

## Purpose

This document defines how the canonical library layers compose into executables or deployable packages.

## Canonical Rule

Executable or package variants are compositions of the canonical libraries. Packaging choice must not redefine subsystem ownership or move parser semantics into the engine.

## Canonical Composition Units

The composition units are:

- engine library
- parser libraries
- IPC library
- threaded IPC server executable or package
- listener executable or package
- parser-agent executable or package
- manager executable or package

## Packaging Variants

Valid packaging variants include:

- application-linked embedded library form
- local shared-server form
- networked listener stack form
- manager-fronted stack form

## Composition Rule

Any packaged form shall still preserve the canonical dependency direction:

- parser depends on engine or IPC, not vice versa
- listener depends on parser-agent or handoff stack, not vice versa
- manager depends on listener routing contracts, not on parser semantics

## Non-Guarantees

This file does not require every layer to be a distinct process in every deployment. It requires composition to preserve the library-first ownership model.
