# Normative Server Cluster UDR Fabric Checklist

## Current status

Parserless server cluster or UDR fabric runtime is not part of the shipped
section `29` listener/server path.

## Required interpretation

- No current listener or server implementation may claim this fabric as active.
- Task routing, link lifecycle, multiplex session handling, and cluster
  scheduling described here are not section `29` runtime authority.
- This file remains an explicit unsupported boundary until dedicated runtime
  code and cross-section contracts exist.
