# Topology Read Views - Issue #147

Still valid at investigation on `tcp` at `7dcc851`: mutable adjacency vectors
and Node pointers allowed callers to bypass topology invariants and revision updates.

## Required Contract and Plan

`getLinksFromNode()` exposes only a const vector reference, including on mutable
Topology instances. Link objects retain their existing validated, revision-aware
mutation APIs. `getNode()` returns only `const Node*`, preventing external node
deactivation, replacement or identity changes. Existing null/bounds behavior stays.

Callers change structure through add/remove node/link methods. Node labels may be
changed through `Topology::setNodeLabel(id, label)`, returning false for missing
or inactive nodes without mutation. Labels do not change routing revision.

Remove mutable getter overloads, add the label operation, and verify constness at
compile time. Runtime regressions must remove a node on a routed path and check
adjacency, link inventory, interfaces, active state, revision and updated routes.
Build, execute focused and full CTest, then headless validation. Commit each stage.

## Completion

Specification `259381c`, implementation `10bcade`. Compile-time assertions verify
read-only getter return types. Both new runtime cases and all 273 CTest cases pass.
The MinGW/Ninja build and mesh4 headless run succeeded in `build/validation-bin`.
These are intentional API restrictions: callers must use topology operations to
edit structure, and `setNodeLabel` to edit labels. Commits remain local; the
GitHub issue remains open pending publication.
