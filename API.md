# RWLEnvelope API Documentation

## Breaking Changes (v1.5.0)

**Version 1.5.0 introduces breaking changes to the API:**

### Callback Signature Changes

- **Old API**: Callbacks were wrapped in `std::function<R(const T&)>` or `std::function<R(T&)>`
- **New API**: Callbacks are now direct function pointers or lambdas with **mandatory `noexcept` specification**

### Key Changes:

1. **Callbacks must be `noexcept`**: All callbacks passed to `observe()` and `mutate()` must be marked with `noexcept`
2. **No `std::function` wrapper**: Callbacks are now passed directly, improving performance and enabling compile-time validation
3. **Concept-based validation**: The API uses C++20 concepts (`ObserveCallbackNoexcept` and `MutateCallbackNoexcept`) to enforce callback requirements at compile time
4. **Additional arguments support**: Callbacks can now accept additional arguments beyond the container reference, enabling better lambda capture avoidance
5. **Exception handling**: The `mutate()` method now includes exception handling for callbacks that may throw despite the `noexcept` requirement

### Migration Guide:

See the [Migration Examples](#migration-examples) section below for detailed before/after code examples.

---

## Overview

`RWLEnvelope` is a header-only C++ template class that provides a simple, convenient envelope-access model for thread-safe access to objects using reader-writer locks. It wraps a type `T` with `std::shared_mutex` to enable safe concurrent read and exclusive write access patterns.

## Requirements

- **C++ Standard**: C++23 or later
- **Required Headers**: `<shared_mutex>`, `<functional>`, `<tuple>`, `<utility>`, `<concepts>`
- **Compiler Support**: Must support `[[nodiscard]]` attribute and C++20 concepts
- **Optional**: `nlohmann/json` library for JSON serialization support

## Template Class

### `template <typename T> class RWLEnvelope`

A thread-safe envelope for type `T` using reader-writer lock semantics.

**Namespace**: `siddiqsoft`

**Header**: `siddiqsoft/RWLEnvelope.hpp`

**Template Constraints**: `T` must be `copy_constructible`

---

## Constructors

### Default Constructor

```cpp
explicit RWLEnvelope()
```

Creates an envelope with a default-constructed instance of type `T`. Use `reassign()` to initialize the underlying storage later if the type doesn't have a default constructor.

**Example**:
```cpp
siddiqsoft::RWLEnvelope<std::map<std::string, int>> myMap;
```

### Move Constructor (from T)

```cpp
explicit RWLEnvelope(T&& src)
```

Creates an envelope by moving the provided object into the envelope.

**Parameters**:
- `src`: An rvalue reference to an object of type `T`

**Example**:
```cpp
std::vector<int> data = {1, 2, 3};
siddiqsoft::RWLEnvelope<std::vector<int>> envelope(std::move(data));
// data is now empty
```

### Move Constructor (from RWLEnvelope)

```cpp
explicit RWLEnvelope(RWLEnvelope<T>&& src) noexcept
```

Moves the contents of another envelope into a new envelope. The source envelope's data is transferred, but each envelope maintains its own mutex.

**Parameters**:
- `src`: An rvalue reference to another `RWLEnvelope<T>`

**Note**: The underlying mutexes are **not** shared. The new object gets its own mutex while the source retains its mutex.

**Example**:
```cpp
siddiqsoft::RWLEnvelope<std::string> env1(std::string("hello"));
siddiqsoft::RWLEnvelope<std::string> env2(std::move(env1));
```

### Copy Constructor (from T)

```cpp
explicit RWLEnvelope(const T& arg)
```

Creates an envelope by copying the provided object into the envelope.

**Parameters**:
- `arg`: A const reference to an object of type `T`

**Example**:
```cpp
std::map<std::string, int> original = {{"key", 42}};
siddiqsoft::RWLEnvelope<std::map<std::string, int>> envelope(original);
// original is unchanged
```

### Deleted Constructors

```cpp
RWLEnvelope& operator=(RWLEnvelope const&) = delete;
```

Copy assignment is explicitly deleted to enforce move semantics.

---

## Member Functions

### `observe()`

```cpp
template <typename Callback, typename... Args>
    requires ObserveCallbackNoexcept<T, Callback, Args...>
auto observe(Callback cbf, Args&&... args) const
```

Performs a read-only operation on the enclosed object using a shared (reader) lock. Multiple threads can execute `observe()` concurrently.

**Template Parameters**:
- `Callback`: The callback type (must be a callable with `noexcept` specification)
- `Args`: Additional argument types to forward to the callback

**Parameters**:
- `cbf`: A callable that takes `const T&` as the first argument, followed by any additional arguments (must be `noexcept`)
- `args`: Additional arguments to forward to the callback

**Returns**: The return value from the callback

**Thread Safety**: Acquires a shared lock; multiple readers can access simultaneously

**Callback Requirements**:
- Must be marked `noexcept`
- Must accept `const T&` as the first parameter
- May accept additional arguments
- Callbacks that don't meet these requirements will fail to compile

**Example**:
```cpp
siddiqsoft::RWLEnvelope<std::map<std::string, int>> data;

// Read without copying
bool found = data.observe([](const auto& map) noexcept {
    return map.find("key") != map.end();
});

// Get a value
int value = data.observe([](const auto& map) noexcept {
    return map.at("key");
});

// With additional arguments
std::string prefix = "item_";
bool hasPrefix = data.observe(
    [](const auto& map, const std::string& p) noexcept {
        return map.find(p) != map.end();
    },
    prefix
);
```

### `mutate()`

```cpp
template <typename Callback, typename... Args>
    requires MutateCallbackNoexcept<T, Callback, Args...>
auto mutate(Callback cbf, Args&&... args)
```

Performs a read-write operation on the enclosed object using an exclusive (writer) lock. Only one thread can execute `mutate()` at a time, and no `observe()` calls can run concurrently.

**Template Parameters**:
- `Callback`: The callback type (must be a callable with `noexcept` specification)
- `Args`: Additional argument types to forward to the callback

**Parameters**:
- `cbf`: A callable that takes `T&` as the first argument, followed by any additional arguments (must be `noexcept`)
- `args`: Additional arguments to forward to the callback

**Returns**: The return value from the callback

**Thread Safety**: Acquires an exclusive lock; blocks all other readers and writers

**Callback Requirements**:
- Must be marked `noexcept`
- Must accept `T&` as the first parameter
- May accept additional arguments
- Callbacks that don't meet these requirements will fail to compile

**Performance Note**: Avoid I/O operations within the callback to minimize lock contention

**Exception Handling**: The method includes try-catch to handle exceptions from callbacks, though callbacks should be `noexcept`

**Example**:
```cpp
siddiqsoft::RWLEnvelope<std::map<std::string, int>> data;

// Modify the map
data.mutate([](auto& map) noexcept {
    map["key"] = 42;
});

// Modify and return a value
int newSize = data.mutate([](auto& map) noexcept {
    map["another"] = 100;
    return map.size();
});

// With additional arguments
std::atomic_int counter{0};
int result = data.mutate(
    [](auto& map, std::atomic_int& cnt) noexcept {
        map["count"] = cnt.load();
        cnt++;
        return cnt.load();
    },
    counter
);
```

### `snapshot()`

```cpp
[[nodiscard]] T snapshot() const
```

Returns a copy of the underlying object. Acquires a shared lock during the copy operation.

**Returns**: A copy of the enclosed object (requires copy constructor for type `T`)

**Thread Safety**: Acquires a shared lock

**Note**: The `[[nodiscard]]` attribute encourages proper use of the returned value

**Example**:
```cpp
siddiqsoft::RWLEnvelope<std::vector<int>> data;
std::vector<int> copy = data.snapshot();
// copy is now independent and can be used without locks
```

### `readLock()`

```cpp
[[nodiscard]] std::tuple<const T&, RLock> readLock()
```

Acquires a shared (reader) lock and returns both the lock and a const reference to the enclosed object. Intended for use with structured bindings in an if-statement or scoped block.

**Returns**: A tuple containing:
- `const T&`: A const reference to the enclosed object
- `RLock`: A `std::shared_lock<std::shared_mutex>` that remains locked within the scope

**Thread Safety**: Acquires a shared lock; multiple readers can access simultaneously

**Important**: The lock is automatically released when the scope exits. Use within a statement block.

**Example**:
```cpp
siddiqsoft::RWLEnvelope<std::map<std::string, int>> data;

if (auto const& [map, lock] = data.readLock(); lock) {
    // map is safely accessible here
    auto it = map.find("key");
    if (it != map.end()) {
        std::cout << it->second << std::endl;
    }
} // lock is released here
```

### `writeLock()`

```cpp
[[nodiscard]] std::tuple<T&, RWLock> writeLock()
```

Acquires an exclusive (writer) lock and returns both the lock and a mutable reference to the enclosed object. Intended for use with structured bindings in an if-statement or scoped block.

**Returns**: A tuple containing:
- `T&`: A mutable reference to the enclosed object
- `RWLock`: A `std::unique_lock<std::shared_mutex>` that remains locked within the scope

**Thread Safety**: Acquires an exclusive lock; blocks all other readers and writers

**Important**: The lock is automatically released when the scope exits. Use within a statement block. Avoid I/O operations within the lock.

**Example**:
```cpp
siddiqsoft::RWLEnvelope<std::map<std::string, int>> data;

if (auto [map, lock] = data.writeLock(); lock) {
    // map is safely modifiable here
    map["key"] = 42;
    map.erase("old_key");
} // lock is released here
```

### `reassign()`

```cpp
void reassign(T&& src)
```

Replaces the enclosed object with a new one by moving the provided object into the envelope. Acquires an exclusive lock during the operation.

**Parameters**:
- `src`: An rvalue reference to a new object of type `T`

**Thread Safety**: Acquires an exclusive lock

**Example**:
```cpp
siddiqsoft::RWLEnvelope<std::vector<int>> data;

std::vector<int> newData = {1, 2, 3, 4, 5};
data.reassign(std::move(newData));
// newData is now empty
```

---

## Conversion Operators

### JSON Conversion (Optional)

```cpp
operator nlohmann::json() const
```

If the `nlohmann/json` library is included in the project, this operator provides a JSON representation of the envelope.

**Returns**: A JSON object containing:
- `"storage"`: The enclosed object serialized to JSON
- `"_typver"`: Version string `"RWLEnvelope/1.0.0"`
- `"readWriteActions"`: Internal counter tracking the number of `mutate()` callbacks executed

**Thread Safety**: Acquires a shared lock

**Availability**: Only available if `INCLUDE_NLOHMANN_JSON_HPP_` is defined

**Example**:
```cpp
#include "nlohmann/json.hpp"
#include "siddiqsoft/RWLEnvelope.hpp"

siddiqsoft::RWLEnvelope<nlohmann::json> doc({{"key", "value"}});
nlohmann::json representation = doc; // Uses conversion operator
std::cout << representation.dump(2) << std::endl;
```

---

## Type Aliases

The following type aliases are used internally:

```cpp
using RWLock = std::unique_lock<std::shared_mutex>;  // Exclusive lock
using RLock  = std::shared_lock<std::shared_mutex>;  // Shared lock
```

---

## Concepts

The following C++20 concepts are used to validate callback signatures at compile time:

### `ObserveCallbackNoexcept`

```cpp
template <typename ContainerType, typename Callback, typename... Args>
concept ObserveCallbackNoexcept = requires(Callback f, const ContainerType& item, Args... args) {
    { f(item, std::forward<Args>(args)...) } noexcept;
};
```

Enforces that a callback for `observe()` is `noexcept` and accepts `const ContainerType&` as the first parameter.

### `MutateCallbackNoexcept`

```cpp
template <typename ContainerType, typename Callback, typename... Args>
concept MutateCallbackNoexcept = requires(Callback f, ContainerType& item, Args... args) {
    { f(item, std::forward<Args>(args)...) } noexcept;
};
```

Enforces that a callback for `mutate()` is `noexcept` and accepts `ContainerType&` as the first parameter.

---

## Usage Patterns

### Pattern 1: Observer/Mutator with Callbacks

Use `observe()` and `mutate()` for focused, callback-based access:

```cpp
siddiqsoft::RWLEnvelope<std::map<std::string, int>> cache;

// Read operation
bool exists = cache.observe([](const auto& m) noexcept {
    return m.count("key") > 0;
});

// Write operation
cache.mutate([](auto& m) noexcept {
    m["key"] = 42;
});
```

**Advantages**:
- Explicit about read vs. write intent
- Callback scope naturally limits lock duration
- Return value avoids unnecessary copies
- Compile-time validation of callback correctness
- No runtime overhead from `std::function` wrapper

### Pattern 2: Direct Lock Access with Structured Bindings

Use `readLock()` and `writeLock()` for more complex operations:

```cpp
siddiqsoft::RWLEnvelope<std::vector<int>> data;

// Read with direct access
if (auto const& [vec, lock] = data.readLock(); lock) {
    for (int val : vec) {
        std::cout << val << " ";
    }
}

// Write with direct access
if (auto [vec, lock] = data.writeLock(); lock) {
    vec.emplace_back(42);
    vec.erase(vec.begin());
}
```

**Advantages**:
- More flexible for complex operations
- Compiler automatically releases lock on scope exit
- Clear intent with structured bindings

### Pattern 3: Snapshot for External Processing

Use `snapshot()` when you need to work with data outside the lock:

```cpp
siddiqsoft::RWLEnvelope<std::vector<int>> data;

// Get a copy to process without holding the lock
std::vector<int> copy = data.snapshot();

// Expensive operation without lock
std::sort(copy.begin(), copy.end());
std::cout << "Sorted: ";
for (int val : copy) std::cout << val << " ";
```

**Advantages**:
- Minimizes lock duration
- Allows expensive operations without blocking other threads
- Simple and straightforward

### Pattern 4: Callbacks with Additional Arguments

Use additional arguments to avoid lambda captures:

```cpp
siddiqsoft::RWLEnvelope<std::map<std::string, int>> data;
std::string searchKey = "target";

// Read with additional argument
bool found = data.observe(
    [](const auto& m, const std::string& key) noexcept {
        return m.find(key) != m.end();
    },
    searchKey
);

// Write with additional argument
std::atomic_int counter{0};
data.mutate(
    [](auto& m, std::atomic_int& cnt) noexcept {
        m["count"] = cnt.load();
        cnt++;
    },
    counter
);
```

**Advantages**:
- Avoids lambda capture overhead
- Cleaner and more explicit parameter passing
- Better for complex operations with multiple external variables

---

## Thread Safety Guarantees

- **Multiple Readers**: Multiple threads can call `observe()` or `readLock()` concurrently
- **Exclusive Writer**: Only one thread can call `mutate()` or `writeLock()` at a time
- **Reader-Writer Exclusion**: No readers can access while a writer is active, and vice versa
- **Exception Safe**: Move constructor is `noexcept`; `mutate()` includes exception handling for callbacks

---

## Common Use Cases

### Thread-Safe Container

```cpp
using RWLMap = siddiqsoft::RWLEnvelope<std::map<std::string, std::string>>;

RWLMap config;
config.reassign(std::move(initialConfig));

// Multiple threads can read concurrently
config.observe([](const auto& m) noexcept {
    std::cout << m.at("setting") << std::endl;
});

// Single thread can write exclusively
config.mutate([](auto& m) noexcept {
    m["setting"] = "new_value";
});
```

### Thread-Safe JSON Document

```cpp
#include "nlohmann/json.hpp"
#include "siddiqsoft/RWLEnvelope.hpp"

siddiqsoft::RWLEnvelope<nlohmann::json> document;

// Initialize
document.reassign(nlohmann::json::parse(jsonString));

// Read fields
std::string name = document.observe([](const auto& doc) noexcept {
    return doc.value("name", "unknown");
});

// Update fields
document.mutate([](auto& doc) noexcept {
    doc["lastModified"] = nlohmann::json::object({
        {"timestamp", std::time(nullptr)},
        {"user", "admin"}
    });
});
```

---

## Performance Considerations

1. **Lock Duration**: Keep callbacks and lock scopes as short as possible
2. **Avoid I/O**: Do not perform I/O operations (file, network) within locks
3. **Snapshot vs. Direct Access**: Use `snapshot()` for expensive post-processing; use direct locks for quick operations
4. **Copy Overhead**: `snapshot()` requires a copy; use `observe()` or `readLock()` to avoid copying
5. **Contention**: High contention scenarios may benefit from more granular locking strategies
6. **No std::function Overhead**: Direct callbacks eliminate the runtime overhead of `std::function` wrappers

---

## Limitations

- **No Copy Construction**: Copy construction is not explicitly deleted, but copy assignment is
- **Default Constructor**: Default constructor requires `T` to be default-constructible (or use `reassign()` later)
- **Mutex Not Shared**: When moving envelopes, each gets its own mutex; they are not shared
- **No Recursive Locking**: Attempting to acquire a lock while already holding one will deadlock
- **Callbacks Must Be noexcept**: All callbacks must be marked `noexcept`; callbacks that throw will not compile

---

## Migration Examples

### Before (v1.4.x) - Using std::function

```cpp
siddiqsoft::RWLEnvelope<std::map<std::string, int>> data;

// Old API with std::function and template arguments
bool found = data.observe<bool>(
    std::function<bool(const std::map<std::string, int>&)>(
        [](const auto& m) { return m.find("key") != m.end(); }
    )
);

data.mutate<void>(
    std::function<void(std::map<std::string, int>&)>(
        [](auto& m) { m["key"] = 42; }
    )
);
```

### After (v1.5.0) - Using Direct Callbacks with noexcept

```cpp
siddiqsoft::RWLEnvelope<std::map<std::string, int>> data;

// New API with direct callbacks and noexcept
bool found = data.observe(
    [](const auto& m) noexcept { return m.find("key") != m.end(); }
);

data.mutate(
    [](auto& m) noexcept { m["key"] = 42; }
);
```

### Migration Steps:

1. **Remove `std::function` wrapper**: Pass lambdas/functions directly
2. **Add `noexcept` to callbacks**: All callbacks must be marked `noexcept`
3. **Remove template arguments**: No need for `<bool>` or `<void>` template arguments
4. **Update lambda captures**: Consider using additional arguments instead of captures for better performance

### Compilation Errors and Solutions:

**Error**: `error: no matching function for call to 'observe'`
- **Cause**: Callback is not marked `noexcept`
- **Solution**: Add `noexcept` to the lambda: `[](const auto& m) noexcept { ... }`

**Error**: `error: no matching function for call to 'mutate'`
- **Cause**: Callback is not marked `noexcept`
- **Solution**: Add `noexcept` to the lambda: `[](auto& m) noexcept { ... }`

**Error**: `error: no matching function for call to 'observe' with template arguments`
- **Cause**: Using old API with template arguments like `observe<bool>(...)`
- **Solution**: Remove template arguments and let the compiler deduce the return type

---

## Example: Complete Usage

```cpp
#include "siddiqsoft/RWLEnvelope.hpp"
#include <iostream>
#include <map>
#include <string>

int main() {
    // Create an envelope for a map
    siddiqsoft::RWLEnvelope<std::map<std::string, int>> scores;
    
    // Initialize with data
    std::map<std::string, int> initial = {{"Alice", 100}, {"Bob", 85}};
    scores.reassign(std::move(initial));
    
    // Read operation using callback
    int aliceScore = scores.observe([](const auto& m) noexcept {
        return m.at("Alice");
    });
    std::cout << "Alice's score: " << aliceScore << std::endl;
    
    // Write operation using callback
    scores.mutate([](auto& m) noexcept {
        m["Charlie"] = 92;
    });
    
    // Read operation using direct lock
    if (auto const& [m, lock] = scores.readLock(); lock) {
        std::cout << "Total players: " << m.size() << std::endl;
    }
    
    // Get a snapshot for external processing
    auto snapshot = scores.snapshot();
    std::cout << "Snapshot size: " << snapshot.size() << std::endl;
    
    return 0;
}
```

---

## License

BSD 3-Clause License

Copyright (c) 2021, Siddiq Software LLC. All rights reserved.

See LICENSE file for full details.
