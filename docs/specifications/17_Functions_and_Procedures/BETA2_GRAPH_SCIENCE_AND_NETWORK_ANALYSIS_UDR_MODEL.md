# Beta 2 Graph Science And Network Analysis UDR Model

## Purpose

This document defines the graph-oriented UDR family for network analysis,
routing, centrality, connectivity, and educational graph workloads.

This group is the ScratchBird-native replacement target for the highest-value
operational portions of `NetworkX`.

## Owning package

- `sb_pkg_graph_udr`

## Dependencies

This package depends on:

- `sb_pkg_num_array_udr`
- `sb_pkg_sci_udr`
- `sb_pkg_opt_udr`

## Mandatory surfaces

The package shall provide:

- graph construction from edge/vertex rowsets
- directed and undirected graph support
- weighted graph support
- connectivity and component analysis
- shortest path families
- centrality metrics
- spanning tree helpers
- topological ordering
- flow and cut helpers for admitted bounded graph sizes

## Required routine families

At minimum the following families shall exist:

- `sb_graph.from_edges(...)`
- `sb_graph.components_*`
- `sb_graph.shortest_path_*`
- `sb_graph.centrality_*`
- `sb_graph.spanning_tree_*`
- `sb_graph.toposort(...)`
- `sb_graph.flow_*`

## Example contract

```sql
select *
from sb_graph.shortest_path_dijkstra(
    source_query => 'select src, dst, weight from logistics.routes',
    source_node => 'A',
    target_node => 'Z'
);
```

## Representation rules

1. Graphs may be transient execution artifacts or stored graph artifacts.
2. Stored graph artifacts shall record directedness, weight schema, and vertex
   identity mapping.
3. Large graph operations shall publish node count, edge count, frontier size,
   and memory metrics.

## Explicit exclusions

- unrestricted distributed graph engines
- graph rendering/visualization
- open-ended graph-mining plugins outside admission control
