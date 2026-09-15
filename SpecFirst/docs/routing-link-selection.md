# Routing Link Selection - Issue #130

Still valid at investigation on `e8e24a7`: Dijkstra stored only predecessor nodes;
forwarding chose the first UP link to that node, potentially using a different edge.

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

## Completion

Specification `a17e7cf`, implementation `fbd518b`. Two new regression cases
exercise all metrics, opposite insertion orders, multi-hop reconstruction, DOWN
fallback and SIMPLEX direction, checking in-transit edge identity. All 275 CTest
cases passed after the full MinGW/Ninja build. The mesh4 headless run succeeded
using `build/validation-bin`. Commits remain local; the issue remains open on GitHub.
