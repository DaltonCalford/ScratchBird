Status: reconstructed_required

# Deployment Variant Selection and Local to Layered Scale Up Operator Model

## Purpose

This document defines the operator-facing model for choosing a deployment variant and scaling from local-only to layered deployments.

## Canonical Rule

Operator surfaces shall present deployment choice as an explicit variant selection, not as a hidden side effect of packaging defaults.

## Canonical Variant Choices

The operator-facing variants are:

- direct embedded engine
- embedded parser plus engine
- local shared IPC server
- listener plus parser-agent pool
- manager-fronted routed stack

## Scale-Up Rule

The operator model shall make clear that a small local-only deployment may scale outward by adding layers while preserving:

- engine-library truth
- parser independence
- MGA semantics
- database-controlled listener and manager policy where those layers appear

## Surface Rule

Each operator surface shall reveal:

- active variant
- active layer set
- available control operations for that variant
- unavailable controls because a layer is absent

## Non-Guarantees

This file does not require one installer or one management UI. It requires explicit variant selection and honest surface scoping.
