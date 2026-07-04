RWLEnvelope : A simple read-writer lock envelope
-------------------------------------------

[![CodeQL](https://github.com/SiddiqSoft/RWLEnvelope/actions/workflows/codeql-analysis.yml/badge.svg)](https://github.com/SiddiqSoft/RWLEnvelope/actions/workflows/codeql-analysis.yml)
[![Build Status](https://dev.azure.com/siddiqsoft/siddiqsoft/_apis/build/status/SiddiqSoft.rwlenvelope?branchName=main)](https://dev.azure.com/siddiqsoft/siddiqsoft/_build/latest?definitionId=7&branchName=main)
![](https://img.shields.io/nuget/v/SiddiqSoft.RWLEnvelope)
![](https://img.shields.io/github/v/tag/SiddiqSoft/RWLEnvelope)
![](https://img.shields.io/azure-devops/tests/siddiqsoft/siddiqsoft/7)
![](https://img.shields.io/azure-devops/coverage/siddiqsoft/siddiqsoft/7)

## Quick Start

**RWLEnvelope** is a header-only C++ template library that provides a simple, convenient envelope-access model for thread-safe access to objects using reader-writer locks.

```cpp
#include "siddiqsoft/RWLEnvelope.hpp"

siddiqsoft::RWLEnvelope<std::map<std::string, int>> data;

// Read-only access
data.observe([](const auto& m) noexcept {
    return m.find("key") != m.end();
});

// Read-write access
data.mutate([](auto& m) noexcept {
    m["key"] = 42;
});
```

## Why RWLEnvelope?

- **Automatic Lock Management**: No manual lock/unlock needed
- **Clear Intent**: `observe()` for reads, `mutate()` for writes
- **Exception Safe**: Locks released properly even if callbacks throw
- **Zero Overhead**: Header-only, no runtime cost beyond std::shared_mutex
- **Works with Any Type**: Not limited to specific containers

## Installation

### NuGet Package
```
Install-Package SiddiqSoft.RWLEnvelope
```

### Header-Only
Copy `include/siddiqsoft/RWLEnvelope.hpp` to your project.

## Requirements

- **C++ Standard**: C++20 or later (requires C++20 concepts support)
- **Compiler**: Must support `[[nodiscard]]` attribute and C++20 concepts
- **Headers**: `<shared_mutex>`, `<functional>`, `<tuple>`, `<utility>`, `<concepts>`
- **Optional**: nlohmann/json library for JSON serialization support

## Key Features

### Multiple Concurrent Readers
```cpp
// Multiple threads can read simultaneously
data.observe([](const auto& m) noexcept {
    return m.at("key");
});
```

### Exclusive Writer
```cpp
// Only one thread can write at a time
data.mutate([](auto& m) noexcept {
    m["key"] = 42;
});
```

### Direct Lock Access
```cpp
// For complex operations
if (auto [map, lock] = data.writeLock(); lock) {
    map["key"] = 42;
    map.erase("old_key");
}
```

### Snapshot for External Processing
```cpp
// Get a copy to process outside the lock
std::vector<int> copy = data.snapshot();
std::sort(copy.begin(), copy.end());
```

## Real-World Examples

### Configuration Management
```cpp
siddiqsoft::RWLEnvelope<AppConfig> config;

// Multiple threads reading config
auto url = config.observe([](const auto& cfg) noexcept {
    return cfg.databaseUrl;
});

// Single thread updating config
config.mutate([](auto& cfg) noexcept {
    cfg.databaseUrl = "new_url";
});
```

### Cache Implementation
```cpp
siddiqsoft::RWLEnvelope<std::unordered_map<std::string, CacheEntry>> cache;

// Fast concurrent reads
auto val = cache.observe([](const auto& c) noexcept {
    return c.at("key").value;
});

// Exclusive writes
cache.mutate([](auto& c) noexcept {
    c["key"] = {"computed_value", now()};
});
```

## Testing

The library includes comprehensive test coverage with 38+ tests covering:
- Basic functionality (observe, mutate, readLock, writeLock)
- Edge cases and exception safety
- Concurrent access patterns
- Data integrity under contention

Run tests:
```bash
cmake --preset Apple-Debug
cmake --build --preset Apple-Debug
ctest --preset Apple-Debug
```

## License

BSD 3-Clause License - See LICENSE file for details

## Resources

- **[GitHub Repository](https://github.com/SiddiqSoft/RWLEnvelope)** - Source code and issues
- **[C++ std::shared_mutex](https://en.cppreference.com/w/cpp/thread/shared_mutex)** - Standard library reference

<small align="right">

&copy; 2021 Abdulkareem Siddiq. All rights reserved.

</small>
