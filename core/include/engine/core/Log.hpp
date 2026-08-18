#pragma once

// Debug logging for the simulation core.
// Compiled out unless KNS_VERBOSE_LOGGING is defined, so normal builds
// and headless runs do not spam stdout for every event.
#ifdef KNS_VERBOSE_LOGGING
    #include <iostream>
    #define KNS_DEBUG_LOG(expr) do { std::cout << expr; } while (false)
#else
    #define KNS_DEBUG_LOG(expr) do { } while (false)
#endif
