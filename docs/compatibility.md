# KNS compatibility

KNS imports topology schema 1.0 from local files. KNS Discovery emits a compatible
snapshot and can also write a versioned change set for watch mode. The simulator
continues to accept unversioned legacy local topologies.

Headless runs emit CSV schema 1. Batch runs also record configuration and results
as versioned JSON. Optional desktop clients accept compatible endpoints configured
by the operator. Their request and response schemas are listed in
[the contract reference](../contracts/README.md).

KNS and KNS Discovery have independent versions. Changes to the meaning or
requiredness of existing wire fields require a new contract version. Record the
exact executable, topology and seed when sharing reproducible results.
