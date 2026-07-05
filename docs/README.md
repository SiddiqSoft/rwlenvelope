# RWLEnvelope API Documentation

<img align="right" src="https://gravatar.com/avatar/b22603b65d11dcab44885c65e44f7dc9" width="100">

[![Build Status](https://dev.azure.com/siddiqsoft/siddiqsoft/_apis/build/status/SiddiqSoft.rwlenvelope?branchName=main)](https://dev.azure.com/siddiqsoft/siddiqsoft/_build/latest?definitionId=7&branchName=main)
![](https://img.shields.io/nuget/v/SiddiqSoft.RWLEnvelope)
![](https://img.shields.io/github/v/tag/SiddiqSoft/RWLEnvelope)
![](https://img.shields.io/azure-devops/tests/siddiqsoft/siddiqsoft/7)
![](https://img.shields.io/azure-devops/coverage/siddiqsoft/siddiqsoft/7)

## Welcome to RWLEnvelope Documentation

This is the **complete API documentation** for RWLEnvelope, a header-only C++ template library providing thread-safe access to objects using reader-writer locks.

### Quick Navigation

- **[API Reference](doxygen/html/index.html)** - Full API documentation with examples
- **[RWLEnvelope Class](doxygen/html/classsiddiqsoft_1_1RWLEnvelope.html)** - Main class documentation
- **[GitHub Repository](https://github.com/SiddiqSoft/RWLEnvelope)** - Source code and issues
- **[Project README](https://github.com/SiddiqSoft/RWLEnvelope#readme)** - Quick start guide

### What is RWLEnvelope?

RWLEnvelope simplifies thread-safe access to shared objects by:

1. **Wrapping your object** with automatic lock management
2. **Providing clear APIs**: [`observe()`](doxygen/html/classsiddiqsoft_1_1RWLEnvelope.html#a8c8c8c8c8c8c8c8) for reads, [`mutate()`](doxygen/html/classsiddiqsoft_1_1RWLEnvelope.html#a9d9d9d9d9d9d9d9) for writes
3. **Ensuring thread safety**: Multiple concurrent readers, exclusive writers
4. **Maintaining exception safety**: Locks released properly even if callbacks throw

### Quick Example

See the [RWLEnvelope class documentation](doxygen/html/classsiddiqsoft_1_1RWLEnvelope.html) for complete API details.

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

### Key Features

- ✅ **Header-only** - No compilation needed
- ✅ **C++20** - Uses modern C++ concepts
- ✅ **Zero overhead** - No runtime cost beyond std::shared_mutex
- ✅ **Exception safe** - RAII semantics ensure proper cleanup
- ✅ **Works with any type** - Not limited to specific containers

### Documentation Structure

The documentation is organized as follows (all links point to the Doxygen generated site):

- **[Main Page](doxygen/html/index.html)** - Overview and core concepts
- **[Classes](doxygen/html/annotated.html)** - RWLEnvelope class template documentation
- **[Namespaces](doxygen/html/namespaces.html)** - siddiqsoft namespace
- **[Files](doxygen/html/files.html)** - Source file listings
- **[Concepts](doxygen/html/namespacesiddiqsoft.html)** - ObserveCallbackNoexcept and MutateCallbackNoexcept

### Requirements

- **C++ Standard**: C++20 or later
- **Compiler**: Must support C++20 concepts and `[[nodiscard]]` attribute
- **Headers**: `<shared_mutex>`, `<functional>`, `<tuple>`, `<utility>`, `<concepts>`

### Installation

**Via NuGet:**
```
Install-Package SiddiqSoft.RWLEnvelope
```

**Header-only:**
Copy `include/siddiqsoft/RWLEnvelope.hpp` to your project.

### Getting Started

1. **Read the [API Reference](doxygen/html/index.html)** for complete documentation
2. **Check the [RWLEnvelope Class](doxygen/html/classsiddiqsoft_1_1RWLEnvelope.html)** for the main class documentation
3. **Review [Member Functions](doxygen/html/classsiddiqsoft_1_1RWLEnvelope.html#public-methods)** for available methods
4. **See [Concepts](doxygen/html/namespacesiddiqsoft.html)** for callback requirements

### API Methods

The [RWLEnvelope class](doxygen/html/classsiddiqsoft_1_1RWLEnvelope.html) provides the following key methods:

- **[`observe()`](doxygen/html/classsiddiqsoft_1_1RWLEnvelope.html#a8c8c8c8c8c8c8c8)** - Perform read-only operations with shared lock
- **[`mutate()`](doxygen/html/classsiddiqsoft_1_1RWLEnvelope.html#a9d9d9d9d9d9d9d9)** - Perform read-write operations with exclusive lock
- **[`readLock()`](doxygen/html/classsiddiqsoft_1_1RWLEnvelope.html#a7e7e7e7e7e7e7e7)** - Acquire shared lock directly
- **[`writeLock()`](doxygen/html/classsiddiqsoft_1_1RWLEnvelope.html#a6f6f6f6f6f6f6f6)** - Acquire exclusive lock directly
- **[`snapshot()`](doxygen/html/classsiddiqsoft_1_1RWLEnvelope.html#a5e5e5e5e5e5e5e5)** - Get a copy of the enclosed object
- **[`reassign()`](doxygen/html/classsiddiqsoft_1_1RWLEnvelope.html#a4d4d4d4d4d4d4d4)** - Replace the enclosed object

### Concepts

The library uses C++20 concepts to enforce callback requirements:

- **[`ObserveCallbackNoexcept`](doxygen/html/namespacesiddiqsoft.html)** - Concept for read-only callbacks
- **[`MutateCallbackNoexcept`](doxygen/html/namespacesiddiqsoft.html)** - Concept for read-write callbacks

### Support

- **Issues**: [GitHub Issues](https://github.com/SiddiqSoft/RWLEnvelope/issues)
- **Discussions**: [GitHub Discussions](https://github.com/SiddiqSoft/RWLEnvelope/discussions)
- **Documentation**: [Doxygen Generated Site](doxygen/html/index.html) (generated with Doxygen Awesome)

### License

BSD 3-Clause License - See [LICENSE](https://github.com/SiddiqSoft/RWLEnvelope/blob/main/LICENSE) for details.

---

**Generated with [Doxygen](https://www.doxygen.nl/) and [Doxygen Awesome CSS](https://github.com/jothepro/doxygen-awesome-css)**
