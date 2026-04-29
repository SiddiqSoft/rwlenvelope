# RWLEnvelope API Documentation

## Overview

`RWLEnvelope` is a header-only C++ template class that provides a simple, convenient envelope-access model for thread-safe access to objects using reader-writer locks. It wraps a type `T` with `std::shared_mutex` to enable safe concurrent read and exclusive write access patterns.

## Requirements

- **C++ Standard**: C++17 or later
- **Required Headers**: `<shared_mutex>`, `<functional>`, `<tuple>`, `<utility>`
- **Compiler Support**: Must support `[[nodiscard]]` attribute
- **Optional**: `nlohmann/json` library for JSON serialization support

## Template Class

### `template <class T> class RWLEnvelope`

A thread-safe envelope for type `T` using reader-writer lock semantics.

**Namespace**: `siddiqsoft`

**Header**: `siddiqsoft/RWLEnvelope.hpp`

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

### Deleted Constructors

```cpp
explicit RWLEnvelope(const T&) = delete;
RWLEnvelope& operator=(RWLEnvelope const&) = delete;
```

Copy construction and copy assignment are explicitly deleted to prevent unintended copies and enforce move semantics.

---

## Member Functions

### `observe()`

```cpp
template <typename R = void> R observe(std::function<R(const T&)> callback) const
```

Performs a read-only operation on the enclosed object using a shared (reader) lock. Multiple threads can execute `observe()` concurrently.

**Template Parameters**:
- `R`: Return type of the callback (defaults to `void`)

**Parameters**:
- `callback`: A callable that takes a `const T&` and returns `R`

**Returns**: The return value from the callback

**Thread Safety**: Acquires a shared lock; multiple readers can access simultaneously

**Example**:
```cpp
siddiqsoft::RWLEnvelope<std::map<std::string, int>> data;

// Read without copying
bool found = data.observe<bool>([](const auto& map) {
    return map.find("key") != map.end();
});

// Get a value
int value = data.observe<int>([](const auto& map) {
    return map.at("key");
});
```

### `mutate()`

```cpp
template <typename R = void> R mutate(std::function<R(T&)> callback)
```

Performs a read-write operation on the enclosed object using an exclusive (writer) lock. Only one thread can execute `mutate()` at a time, and no `observe()` calls can run concurrently.

**Template Parameters**:
- `R`: Return type of the callback (defaults to `void`)

**Parameters**:
- `callback`: A callable that takes a `T&` and returns `R`

**Returns**: The return value from the callback

**Thread Safety**: Acquires an exclusive lock; blocks all other readers and writers

**Performance Note**: Avoid I/O operations within the callback to minimize lock contention

**Example**:
```cpp
siddiqsoft::RWLEnvelope<std::map<std::string, int>> data;

// Modify the map
data.mutate<void>([](auto& map) {
    map["key"] = 42;
});

// Modify and return a value
int newSize = data.mutate<int>([](auto& map) {
    map["another"] = 100;
    return map.size();
});
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

## Usage Patterns

### Pattern 1: Observer/Mutator with Callbacks

Use `observe()` and `mutate()` for focused, callback-based access:

```cpp
siddiqsoft::RWLEnvelope<std::map<std::string, int>> cache;

// Read operation
bool exists = cache.observe<bool>([](const auto& m) {
    return m.count("key") > 0;
});

// Write operation
cache.mutate<void>([](auto& m) {
    m["key"] = 42;
});
```

**Advantages**:
- Explicit about read vs. write intent
- Callback scope naturally limits lock duration
- Return value avoids unnecessary copies

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
    vec.push_back(42);
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

---

## Thread Safety Guarantees

- **Multiple Readers**: Multiple threads can call `observe()` or `readLock()` concurrently
- **Exclusive Writer**: Only one thread can call `mutate()` or `writeLock()` at a time
- **Reader-Writer Exclusion**: No readers can access while a writer is active, and vice versa
- **Exception Safe**: Move constructor is `noexcept`; other operations may throw if callbacks throw

---

## Common Use Cases

### Thread-Safe Container

```cpp
using RWLMap = siddiqsoft::RWLEnvelope<std::map<std::string, std::string>>;

RWLMap config;
config.reassign(std::move(initialConfig));

// Multiple threads can read concurrently
config.observe<void>([](const auto& m) {
    std::cout << m.at("setting") << std::endl;
});

// Single thread can write exclusively
config.mutate<void>([](auto& m) {
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
std::string name = document.observe<std::string>([](const auto& doc) {
    return doc.value("name", "unknown");
});

// Update fields
document.mutate<void>([](auto& doc) {
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

---

## Limitations

- **No Copy Construction**: Type `T` must be movable; copy construction is explicitly deleted
- **Default Constructor**: Default constructor requires `T` to be default-constructible (or use `reassign()` later)
- **Mutex Not Shared**: When moving envelopes, each gets its own mutex; they are not shared
- **No Recursive Locking**: Attempting to acquire a lock while already holding one will deadlock

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
    int aliceScore = scores.observe<int>([](const auto& m) {
        return m.at("Alice");
    });
    std::cout << "Alice's score: " << aliceScore << std::endl;
    
    // Write operation using callback
    scores.mutate<void>([](auto& m) {
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
