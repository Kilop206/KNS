# Routing Link Selection - Issue #130

Still valid at `e8e24a7`: Dijkstra stores only predecessor nodes; forwarding
chooses the first UP link to that node, potentially using a different edge.

## Required Contract and Plan

Dijkstra retains each predecessor edge ID. Routing entries expose the first
edge's optional `link_id`, absent for self/unreachable destinations. Forwarding
uses that exact edge, subject to its UP state and transmission direction.
Lazy routing revision refresh remains in effect. Existing strict cost comparison
and first-discovered tie policy remain unchanged, including hop-count ties.

Test both insertion orders for parallel links under all four metrics. Include a
multi-hop route to verify first-edge reconstruction, DOWN changes, reverse SIMPLEX
edges, and absent link identities for self/unreachable routes. Assert the actual
in-transit link ID as well as route metadata. Build, run focused/full CTest and
headless validation; commit specification, implementation and evidence separately.
