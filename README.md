# RWLEnvelope API Documentation

<img align="right" src="https://gravatar.com/avatar/b22603b65d11dcab44885c65e44f7dc9">

[![Build Status](https://dev.azure.com/siddiqsoft/siddiqsoft/_apis/build/status/SiddiqSoft.rwlenvelope?branchName=main)](https://dev.azure.com/siddiqsoft/siddiqsoft/_build/latest?definitionId=7&branchName=main)
![](https://img.shields.io/nuget/v/SiddiqSoft.RWLEnvelope)
![](https://img.shields.io/github/v/tag/SiddiqSoft/RWLEnvelope)
![](https://img.shields.io/azure-devops/tests/siddiqsoft/siddiqsoft/7)
![](https://img.shields.io/azure-devops/coverage/siddiqsoft/siddiqsoft/7)

## Overview

**RWLEnvelope** is a header-only C++ template library that provides a simple, convenient envelope-access model for thread-safe access to objects using reader-writer locks.

## Core Concepts

### What is an Envelope?

An envelope wraps your object with automatic lock management. Instead of manually managing `std::shared_mutex`, `std::unique_lock`, and `std::shared_lock`, you provide callbacks that operate on your data while the envelope handles the locking.

### Thread Safety Model

- **Multiple Readers**: Multiple threads can call `observe()` or `readLock()` concurrently
- **Exclusive Writer**: Only one thread can call `mutate()` or `writeLock()` at a time
- **Reader-Writer Exclusion**: No readers can access while a writer is active, and vice versa
- **Exception Safe**: Locks are released properly even if callbacks throw

## API Reference

### Class Template: RWLEnvelope

```cpp
template <typename ContainerType>
    requires std::copy_constructible<ContainerType>
class RWLEnvelope;
```

#### Template Parameter

- **ContainerType**: The type of object to be enveloped. Must be copy-constructible.

### Constructors

#### Default Constructor
```cpp
explicit RWLEnvelope();
```
Creates an envelope with a default-constructed instance of `ContainerType`.

**Requirements**: `ContainerType` must be default-constructible.

**Example**:
```cpp
siddiqsoft::RWLEnvelope<std::map<std::string, int>> myMap;
```

#### Move Constructor (from RWLEnvelope)
```cpp
explicit RWLEnvelope(RWLEnvelope<ContainerType>&& src) noexcept;
```
Moves the contents of another envelope into a new envelope. Each envelope maintains its own mutex.

**Note**: The underlying mutexes are **not** shared.

**Example**:
```cpp
siddiqsoft::RWLEnvelope<std::string> env1(std::string("hello"));
siddiqsoft::RWLEnvelope<std::string> env2(std::move(env1));
```

#### Move Constructor (from ContainerType)
```cpp
explicit RWLEnvelope(ContainerType&& src);
```
Creates an envelope by moving the provided object into the envelope.

**Example**:
```cpp
std::vector<int> data = {1, 2, 3};
siddiqsoft::RWLEnvelope<std::vector<int>> envelope(std::move(data));
// data is now empty
```

#### Copy Constructor (from ContainerType)
```cpp
explicit RWLEnvelope(const ContainerType& arg);
```
Creates an envelope by copying the provided object into the envelope.

**Requirements**: `ContainerType` must be copy-constructible.

**Example**:
```cpp
std::map<std::string, int> original = {{"key", 42}};
siddiqsoft::RWLEnvelope<std::map<std::string, int>> envelope(original);
// original is unchanged
```

### Methods

#### observe() - Read-Only Access

```cpp
template <typename Callback, typename... Args>
    requires ObserveCallbackNoexcept<ContainerType, Callback, Args...>
auto observe(Callback cbf, Args&&... args) const;
```

Performs a read-only operation on the enclosed object using a shared (reader) lock.

**Parameters**:
- `cbf`: A callback function that accepts `const ContainerType&` as the first parameter (must be `noexcept`)
- `args`: Additional arguments to pass to the callback

**Returns**: The return value from the callback

**Thread Safety**: Multiple threads can execute `observe()` concurrently.

**Requirements**: The callback must be marked `noexcept`.

**Example**:
```cpp
siddiqsoft::RWLEnvelope<std::map<std::string, int>> data;

// Simple read
bool found = data.observe([](const auto& map) noexcept {
    return map.find("key") != map.end();
});

// Read with additional arguments
std::string searchKey = "target";
bool exists = data.observe(
    [](const auto& map, const std::string& key) noexcept {
        return map.count(key) > 0;
    },
    searchKey
);

// Read with return value
int value = data.observe([](const auto& map) noexcept {
    return map.at("key");
});
```

#### mutate() - Read-Write Access

```cpp
template <typename Callback, typename... Args>
    requires MutateCallbackNoexcept<ContainerType, Callback, Args...>
auto mutate(Callback cbf, Args&&... args);
```

Performs a read-write operation on the enclosed object using an exclusive (writer) lock.

**Parameters**:
- `cbf`: A callback function that accepts `ContainerType&` as the first parameter (must be `noexcept`)
- `args`: Additional arguments to pass to the callback

**Returns**: The return value from the callback

**Thread Safety**: Only one thread can execute `mutate()` at a time. No `observe()` calls can run concurrently.

**Requirements**: The callback must be marked `noexcept`.

**Example**:
```cpp
siddiqsoft::RWLEnvelope<std::map<std::string, int>> data;

// Simple write
data.mutate([](auto& map) noexcept {
    map["key"] = 42;
});

// Write with additional arguments
std::string newKey = "new";
int newValue = 100;
data.mutate(
    [](auto& map, const std::string& k, int v) noexcept {
        map[k] = v;
    },
    newKey,
    newValue
);

// Write with return value
int newSize = data.mutate([](auto& map) noexcept {
    map["another"] = 100;
    return map.size();
});
```

#### snapshot() - Copy for External Processing

```cpp
[[nodiscard]] ContainerType snapshot() const;
```

Returns a copy of the underlying object. Acquires a shared lock during the copy operation.

**Returns**: A copy of the enclosed object

**Thread Safety**: Acquires a shared lock

**Use Case**: When you need to perform expensive operations without holding the lock.

**Example**:
```cpp
siddiqsoft::RWLEnvelope<std::vector<int>> data;
std::vector<int> copy = data.snapshot();
std::sort(copy.begin(), copy.end());
// Process the sorted copy without holding the lock
```

#### readLock() - Direct Shared Lock Access

```cpp
[[nodiscard]] std::tuple<const ContainerType&, RLock> readLock();
```

Acquires a shared (reader) lock and returns it with a const reference to the object.

**Returns**: A tuple containing:
- `const ContainerType&`: A const reference to the enclosed object
- `RLock`: A `std::shared_lock<std::shared_mutex>` that remains locked within the scope

**Thread Safety**: Multiple threads can execute `readLock()` concurrently.

**Usage**: Intended for use with structured bindings in an if-statement or scoped block.

**Example**:
```cpp
siddiqsoft::RWLEnvelope<std::map<std::string, int>> data;

if (auto const& [map, lock] = data.readLock(); lock) {
    auto it = map.find("key");
    if (it != map.end()) {
        std::cout << it->second << std::endl;
    }
} // lock is released here
```

#### writeLock() - Direct Exclusive Lock Access

```cpp
[[nodiscard]] std::tuple<ContainerType&, RWLock> writeLock();
```

Acquires an exclusive (writer) lock and returns it with a mutable reference to the object.

**Returns**: A tuple containing:
- `ContainerType&`: A mutable reference to the enclosed object
- `RWLock`: A `std::unique_lock<std::shared_mutex>` that remains locked within the scope

**Thread Safety**: Only one thread can execute `writeLock()` at a time. No readers can access concurrently.

**Usage**: Intended for use with structured bindings in an if-statement or scoped block.

**Example**:
```cpp
siddiqsoft::RWLEnvelope<std::map<std::string, int>> data;

if (auto [map, lock] = data.writeLock(); lock) {
    map["key"] = 42;
    map.erase("old_key");
} // lock is released here
```

#### reassign() - Replace Enclosed Object

```cpp
void reassign(ContainerType&& src);
```

Replaces the enclosed object with a new one by moving the provided object into the envelope.

**Parameters**:
- `src`: The new object to move into the envelope

**Thread Safety**: Acquires an exclusive (writer) lock

**Example**:
```cpp
siddiqsoft::RWLEnvelope<std::vector<int>> data;
std::vector<int> newData = {1, 2, 3, 4, 5};
data.reassign(std::move(newData));
// newData is now empty
```

### JSON Conversion (Optional)

```cpp
operator nlohmann::json() const;
```

If the nlohmann/json library is included in the project, this operator provides a JSON representation of the envelope.

**Returns**: A JSON object containing:
- `"storage"`: The enclosed object serialized to JSON
- `"_typver"`: Version string "RWLEnvelope/1.5.1"
- `"readWriteActions"`: Internal counter tracking the number of `mutate()` callbacks executed

**Requirements**: Must include `<nlohmann/json.hpp>` before including RWLEnvelope.hpp

**Example**:
```cpp
#include "nlohmann/json.hpp"
#include "siddiqsoft/RWLEnvelope.hpp"

siddiqsoft::RWLEnvelope<nlohmann::json> doc({{"key", "value"}});
nlohmann::json representation = doc;
std::cout << representation.dump(2) << std::endl;
```

## Usage Patterns

### Pattern 1: Callback-Based Access (Recommended)

Use callbacks for simple, focused operations:

```cpp
siddiqsoft::RWLEnvelope<std::map<std::string, int>> cache;

// Read
bool exists = cache.observe([](const auto& m) noexcept {
    return m.count("key") > 0;
});

// Write
cache.mutate([](auto& m) noexcept {
    m["key"] = 42;
});
```

**Advantages**:
- Automatic lock management
- Clear intent (observe vs mutate)
- Minimal boilerplate
- Exception safe

### Pattern 2: Direct Lock Access

Use direct locks for complex operations:

```cpp
siddiqsoft::RWLEnvelope<std::map<std::string, int>> data;

// Read with complex logic
if (auto const& [map, lock] = data.readLock(); lock) {
    auto it = map.find("key");
    if (it != map.end()) {
        std::cout << it->second << std::endl;
    }
}

// Write with complex logic
if (auto [map, lock] = data.writeLock(); lock) {
    map["key"] = 42;
    map.erase("old_key");
}
```

**Advantages**:
- Full control over the locked region
- Can perform multiple operations
- Lock scope is explicit

### Pattern 3: Snapshot for External Processing

Use snapshots when you need to process data outside the lock:

```cpp
siddiqsoft::RWLEnvelope<std::vector<int>> data;

// Get a copy
std::vector<int> copy = data.snapshot();

// Process without holding the lock
std::sort(copy.begin(), copy.end());
std::cout << "Sorted: ";
for (int v : copy) std::cout << v << " ";
```

**Advantages**:
- Minimizes lock duration
- Allows expensive operations without blocking other threads
- Safe for I/O operations

### Pattern 4: Callbacks with Additional Arguments

Pass data into callbacks to avoid lambda captures:

```cpp
siddiqsoft::RWLEnvelope<std::map<std::string, int>> data;

std::string searchKey = "target";
bool found = data.observe(
    [](const auto& m, const std::string& key) noexcept {
        return m.find(key) != m.end();
    },
    searchKey
);

std::string newKey = "new";
int newValue = 100;
data.mutate(
    [](auto& m, const std::string& k, int v) noexcept {
        m[k] = v;
    },
    newKey,
    newValue
);
```

**Advantages**:
- Avoids lambda captures
- Clearer parameter passing
- Better for complex operations

## Performance Considerations

1. **Keep callbacks short**: Minimize the time locks are held
2. **Avoid I/O in locks**: Don't perform file or network operations within callbacks
3. **Use snapshot() for expensive operations**: Process data outside the lock when possible
4. **Use observe() for read-only access**: Allows concurrent readers
5. **Batch operations**: Combine multiple operations into a single `mutate()` call when possible

## Requirements

- **C++ Standard**: C++20 or later (requires C++20 concepts support)
- **Required Headers**: `<shared_mutex>`, `<functional>`, `<tuple>`, `<utility>`, `<concepts>`
- **Compiler Support**: Must support `[[nodiscard]]` attribute and C++20 concepts
- **Optional**: nlohmann/json library for JSON serialization support

## Limitations

- Copy assignment is deleted; use move semantics
- Default constructor requires `ContainerType` to be default-constructible
- Mutex is not shared when moving envelopes
- No recursive locking (will deadlock)
- All callbacks must be marked `noexcept`

## Examples

### Configuration Management

```cpp
struct AppConfig {
    std::string databaseUrl;
    int maxConnections;
};

siddiqsoft::RWLEnvelope<AppConfig> config;

// Multiple threads reading config
auto myDatabaseUrl = config.observe([](const auto& cfg) noexcept {
    return cfg.databaseUrl;
});

// Single thread updating config
config.mutate([](auto& cfg) noexcept {
    cfg.databaseUrl = "new_url";
    cfg.maxConnections = 50;
});
```

### Cache Implementation

```cpp
struct CacheEntry {
    std::string value;
    std::chrono::system_clock::time_point timestamp;
};

siddiqsoft::RWLEnvelope<std::unordered_map<std::string, CacheEntry>> cache;

// Fast concurrent reads
auto val = cache.observe([](const auto& c) noexcept {
    auto it = c.find("key");
    return it != c.end() ? it->second.value : "";
});

// Exclusive writes
cache.mutate([](auto& c) noexcept {
    c["key"] = {"computed_value", std::chrono::system_clock::now()};
});
```

### Shared State in Services

```cpp
struct ServiceState {
    bool healthy;
    std::map<std::string, std::string> metrics;
};

siddiqsoft::RWLEnvelope<ServiceState> state;

// Multiple reader threads
auto healthCheck = state.observe([](const auto& s) noexcept {
    return s.healthy;
});

// Single writer thread
state.mutate([](auto& s) noexcept {
    s.metrics["cpu_usage"] = "45%";
    s.metrics["memory_usage"] = "60%";
});
```

## See Also

- [GitHub Repository](https://github.com/SiddiqSoft/RWLEnvelope)
- [C++ std::shared_mutex Documentation](https://en.cppreference.com/w/cpp/thread/shared_mutex)
- [C++ std::unique_lock Documentation](https://en.cppreference.com/w/cpp/thread/unique_lock)
- [C++ std::shared_lock Documentation](https://en.cppreference.com/w/cpp/thread/shared_lock)
