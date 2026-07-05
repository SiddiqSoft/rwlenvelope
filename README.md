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

using CustomType = std::map<std::string,int>;
// Declare a map as container and initialize it with some values
// Using initializer list constructor for convenient initialization.
// Note the brace-count as we use brace initializer to construct the RWLEnvelope
// while constructing its internal storage with the std::map initializer!
siddiqsoft::RWLEnvelope<CustomType> data({{"key", 1010},
                                          {"key2", 2020},
                                          {"key3", 3030}});

// Read-only access
auto val = data.observe([](const auto& m) noexcept -> auto {
    return m.value("key", 0);
});

// Read-write access
data.mutate([](auto& m) noexcept {
    m["key"] = 42;
});

std::println(std::cerr, "Contents: {}", data.snapshot());
```


## Why RWLEnvelope?

- **Automatic Lock Management**: No manual lock/unlock needed
- **Clear Intent**: `observe()` for reads, `mutate()` for writes
- **Exception Safe**: Locks released properly even if callbacks throw
- **Zero Overhead**: Header-only, no runtime cost beyond std::shared_mutex
- **Works with Any Type**: Not limited to specific containers
- **Convenient Initialization**: Supports initializer lists for easy construction

## Documentation

**📖 [Complete API Documentation](https://siddiqsoft.github.io/rwlenvelope/)**

The full documentation includes:
- [Detailed API reference](https://siddiqsoft.github.io/rwlenvelope/doxygen/html/index.html)
- [RWLEnvelope class documentation](https://siddiqsoft.github.io/rwlenvelope/doxygen/html/classsiddiqsoft_1_1RWLEnvelope.html)
- [Usage patterns and examples](https://siddiqsoft.github.io/rwlenvelope/doxygen/html/classsiddiqsoft_1_1RWLEnvelope.html)
- [Thread safety guarantees](https://siddiqsoft.github.io/rwlenvelope/doxygen/html/classsiddiqsoft_1_1RWLEnvelope.html)
- [Requirements and limitations](https://siddiqsoft.github.io/rwlenvelope/doxygen/html/classsiddiqsoft_1_1RWLEnvelope.html)

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

See [`observe()` documentation](https://siddiqsoft.github.io/rwlenvelope/doxygen/html/classsiddiqsoft_1_1RWLEnvelope.html#a8c8c8c8c8c8c8c8) for details.

### Exclusive Writer
```cpp
// Only one thread can write at a time
data.mutate([](auto& m) noexcept {
    m["key"] = 42;
});
```

See [`mutate()` documentation](https://siddiqsoft.github.io/rwlenvelope/doxygen/html/classsiddiqsoft_1_1RWLEnvelope.html#a9d9d9d9d9d9d9d9) for details.

### Direct Lock Access
```cpp
// For complex operations
if (auto [map, lock] = data.writeLock(); lock) {
    map["key"] = 42;
    map.erase("old_key");
}
```

See [`readLock()`](https://siddiqsoft.github.io/rwlenvelope/doxygen/html/classsiddiqsoft_1_1RWLEnvelope.html#a7e7e7e7e7e7e7e7) and [`writeLock()`](https://siddiqsoft.github.io/rwlenvelope/doxygen/html/classsiddiqsoft_1_1RWLEnvelope.html#a6f6f6f6f6f6f6f6) documentation.

### Snapshot for External Processing
```cpp
// Get a copy to process outside the lock
std::vector<int> copy = data.snapshot();
std::sort(copy.begin(), copy.end());
```

See [`snapshot()` documentation](https://siddiqsoft.github.io/rwlenvelope/doxygen/html/classsiddiqsoft_1_1RWLEnvelope.html#a5e5e5e5e5e5e5e5) for details.

### Initializer List Construction
```cpp
// Construct with initializer list - works with any container supporting std::initializer_list
siddiqsoft::RWLEnvelope<std::vector<int>> vec({1, 2, 3, 4, 5});

// Maps and other associative containers
siddiqsoft::RWLEnvelope<std::map<std::string, int>> map({
    {"key1", 1},
    {"key2", 2},
    {"key3", 3}
});

// JSON objects
siddiqsoft::RWLEnvelope<nlohmann::json> doc({
    {"name", "John"},
    {"age", 30},
    {"active", true}
});
```

See [constructor documentation](https://siddiqsoft.github.io/rwlenvelope/doxygen/html/classsiddiqsoft_1_1RWLEnvelope.html) for all available constructors.

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

### Initializing with Data
```cpp
// Initialize a cache with pre-populated data
siddiqsoft::RWLEnvelope<std::map<std::string, std::string>> cache({
    {"user:1", "Alice"},
    {"user:2", "Bob"},
    {"user:3", "Charlie"}
});

// Initialize a JSON document with structured data
siddiqsoft::RWLEnvelope<nlohmann::json> config({
    {"database", {{"host", "localhost"}, {"port", 5432}}},
    {"cache", {{"ttl", 3600}, {"enabled", true}}}
});
```

## Testing

The library includes comprehensive test coverage with 38+ tests covering:
- Basic functionality (observe, mutate, readLock, writeLock)
- Initializer list construction
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

- **[Full API Documentation](https://siddiqsoft.github.io/rwlenvelope/doxygen/html/index.html)** - Complete reference and examples
- **[RWLEnvelope Class](https://siddiqsoft.github.io/rwlenvelope/doxygen/html/classsiddiqsoft_1_1RWLEnvelope.html)** - Main class documentation
- **[GitHub Repository](https://github.com/SiddiqSoft/RWLEnvelope)** - Source code and issues
- **[C++ std::shared_mutex](https://en.cppreference.com/w/cpp/thread/shared_mutex)** - Standard library reference

<small align="right">

&copy; 2021 Abdulkareem Siddiq. All rights reserved.

</small>
