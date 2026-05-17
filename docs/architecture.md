# Design Decisions Explanation

KNS is a deterministic networks simulator, inspired by ns-3 and Cisco Packet Tracer, oriented to the execution of events. It was initially made to be search project to learn more about C++ and networks,  but ended up being a more complex project with high-level architecture.

## Priority Queue over simple list

Using a priority queue for events instead of a simple list ensures that events are executed in the correct chronological order based on their execution time. In cases where two events have the same scheduled time, they can be disambiguated using their ID, guaranteeing a consistent and predictable execution order.
The priority list is implemented on `core/include/engine/core/EventQueue.hpp`, and is called every time a event is executed or added to the list so the program can know which event is being called.

## core/ and app/

Separating the project into `core/` and `app/` modules improves code organization and reusability. The `core/` module contains components that are generic and can be reused by other developers or even in different projects, while `app/` focuses on the application-specific logic.
`core/` is also divided in many smaller directories, such as `include/ ` (which declares the header files) and `src/` (that contains the implementation files) and their subdirectories, each on with its specific responsability and imortance to the project.

## Dijkstra

Dijkstra's algorithm is calculated only at the beginning rather than for every packet because recalculating it repeatedly would significantly increase execution time. Not only would it need 
to run for each packet, but also for every node the packet traverses, making the system inefficient and slow.
Dijkstra's algorithm is implemented in `src/network/Routing.cpp`, and, as explained in the file, it has an execution time of O((V + E) log V), where E is the number os edges and V is the number of nodes received by the algorithm.

## Unique_ptr used to store events

Using `std::unique_ptr` for events avoids the need for manual memory management. Without it, memory allocation and deallocation would have to be handled explicitly, increasing the risk of memory leaks and making the code more error-prone.
As it ensures safety, it also ensures the code is more readable, since it won't have keywords such as `new` and `delete`.