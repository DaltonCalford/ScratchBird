Status: reconstructed_required

# GPU Accelerator and Resident Index Cost Model

## Purpose

This document defines how the optimizer compares optional GPU acceleration with CPU execution and with resident-memory index advantages.

## Canonical Rule

GPU use is never free and never presumed. The optimizer shall compare accelerator and non-accelerator paths through an explicit cost frame that includes transfer, admission, and fallback costs.

## Required Cost Components

The accelerator comparison frame shall include:

- GPU admission state
- host-to-device transfer cost
- device execution startup cost
- device execution continuation cost
- device-to-host materialization cost
- CPU fallback cost if the device path demotes
- resident index warm state
- device-memory pressure penalty

## Resident Index Credit

If an index family is already resident in host memory, the optimizer shall credit that state explicitly. GPU planning may still win, but it must overcome the host-resident startup advantage and transfer costs.

## Accelerator Refusal Rule

If the device is not admitted, is degraded beyond policy, or lacks compatible operator support, the optimizer shall remove the accelerator variant from the candidate set without removing the underlying CPU family candidate.

## Candidate Parity Rule

GPU-enabled families remain the same primary family candidates as their CPU equivalents. Accelerator support adds candidate variants; it does not create a separate secondary class.

## Metrics Requirements

The optimizer shall consume:

- accelerator admission state
- device-memory pressure class
- transfer metrics
- resident-host-memory state
- family-native index metrics

## Diagnostics Requirements

For an accelerator-considered plan, diagnostics shall be able to explain:

- why the GPU variant won
- why the GPU variant lost
- why the GPU variant was refused

## Non-Guarantees

This file does not require the current engine to support every family on GPU. It defines the cost model required whenever optional GPU execution exists.
