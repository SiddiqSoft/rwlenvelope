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

- **[API Reference](index.html)** - Full API documentation with examples
- **[GitHub Repository](https://github.com/SiddiqSoft/RWLEnvelope)** - Source code and issues
- **[Project README](https://github.com/SiddiqSoft/RWLEnvelope#readme)** - Quick start guide

### What is RWLEnvelope?

RWLEnvelope simplifies thread-safe access to shared objects by:

1. **Wrapping your object** with automatic lock management
2. **Providing clear APIs**: `observe()` for reads, `mutate()` for writes
3. **Ensuring thread safety**: Multiple concurrent readers, exclusive writers
4. **Maintaining exception safety**: Locks released properly even if callbacks throw

### Quick Example

```cpp
#include "siddiqsoft/RWLEnvelope.hpp"

siddiqsoft::RWLEnvelope<std::map<std::string, int>> data;

// Read-only access (multiple threads can do this concurrently)
data.observe([](const auto& m) noexcept {
    return m.find("key") != m.end();
});

// Read-write access (exclusive access)
data.mutate([](auto& m) noexcept {
    m["key"] = 42;
});
```

### Key Features

- ✅ **Header-only** - No compilation needed
- ✅ **C++20** - Uses modern C++ concepts
- ✅ **Zero overhead** - No runtime cost beyond std::shared_mutex
- ✅ **Exception safe** - RAII semantics ensure proper cleanup
- ✅ **Works with any type** - Not limited to specific containers

### Documentation Structure

The documentation is organized as follows:

- **Main Page** - Overview and core concepts
- **Classes** - RWLEnvelope class template documentation
- **Namespaces** - siddiqsoft namespace
- **Files** - Source file listings
- **Examples** - Code examples and usage patterns

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

1. **Read the [API Reference](index.html)** for complete documentation
2. **Check the [Examples](index.html)** for common usage patterns
3. **Review [Performance Considerations](index.html)** for best practices
4. **See [Limitations](index.html)** for important constraints

### Support

- **Issues**: [GitHub Issues](https://github.com/SiddiqSoft/RWLEnvelope/issues)
- **Discussions**: [GitHub Discussions](https://github.com/SiddiqSoft/RWLEnvelope/discussions)
- **Documentation**: This site (generated with Doxygen Awesome)

### License

BSD 3-Clause License - See [LICENSE](https://github.com/SiddiqSoft/RWLEnvelope/blob/main/LICENSE) for details.

---

**Generated with [Doxygen](https://www.doxygen.nl/) and [Doxygen Awesome CSS](https://github.com/jothepro/doxygen-awesome-css)**
