RWLEnvelope : A simple read-writer lock envelope
-------------------------------------------

[![CodeQL](https://github.com/SiddiqSoft/RWLEnvelope/actions/workflows/codeql-analysis.yml/badge.svg)](https://github.com/SiddiqSoft/RWLEnvelope/actions/workflows/codeql-analysis.yml)
[![Build Status](https://dev.azure.com/siddiqsoft/siddiqsoft/_apis/build/status/SiddiqSoft.rwlenvelope?branchName=main)](https://dev.azure.com/siddiqsoft/siddiqsoft/_build/latest?definitionId=7&branchName=main)
![](https://img.shields.io/nuget/v/SiddiqSoft.RWLEnvelope)
![](https://img.shields.io/github/v/tag/SiddiqSoft/RWLEnvelope)
![](https://img.shields.io/azure-devops/tests/siddiqsoft/siddiqsoft/7)
![](https://img.shields.io/azure-devops/coverage/siddiqsoft/siddiqsoft/7)

## Objective

**RWLEnvelope** is a header-only C++ template library that provides a simple, convenient envelope-access model for thread-safe access to objects using reader-writer locks. Our goal is to:

- **Avoid re-implementing the rw-lock**: The standard C++ library (since C++14) provides excellent reader-writer lock implementations via [`std::shared_mutex`](https://en.cppreference.com/w/cpp/thread/shared_mutex), [`std::unique_lock`](https://en.cppreference.com/w/cpp/thread/unique_lock), and [`std::shared_lock`](https://en.cppreference.com/w/cpp/thread/shared_lock).

- **Simplify thread-safe access patterns**: We provide a convenient layer that makes it easy to work with shared data in multi-threaded applications without exposing the complexity of manual lock management.

- **Enable safe concurrent access**: Support multiple concurrent readers while ensuring exclusive access for writers, with automatic lock management and RAII semantics.

- **Minimize boilerplate code**: Reduce the amount of locking code needed to safely access shared objects through intuitive APIs.

<p align="right" width="50%">
<b>WE DO NOT IMPLEMENT</b> a read-writer lock; the standard C++ library has one.<br/>We provide a header-only package simplifying the locking code around thread-safe access to your underlying type.
<br/>
<i>NOT a wrapper; an envelope.</i>
</p>

# API Documentation

For comprehensive API documentation, including detailed descriptions of all methods, usage patterns, and examples, see [**API.md**](API.md).

# Requirements
- You must be able to use [`<shared_mutex>`](https://en.cppreference.com/w/cpp/thread/shared_mutex) and [`<mutex>`](https://en.cppreference.com/w/cpp/thread/mutex).
- Minimal target is `C++17`.
- The build and tests are for Visual Studio 2019 under x64.
- We use [`nlohmann::json`](https://github.com/nlohmann/json) only in our tests and the library is aware to provide a conversion operator if library is detected.

# Usage

- Use the nuget [SiddiqSoft.RWLEnvelope](https://www.nuget.org/packages/SiddiqSoft.RWLEnvelope/)
- Copy paste..whatever works.
- The idea is to not "wrap" the underlying type forcing you to either inherit or re-implement the types but to take advantage of the underlying type's interface whilst ensuring that we have the necessary locks.
- Two methods:
  - Observer/mutator model with callback and custom return to limit access and to focus the where and how to access the underlying type.
  - Take advantage of [init-statement in if-statement](https://en.cppreference.com/w/cpp/language/if) to get the contained object within a lock and have the compiler auto-release once we leave scope.
- A sample implementation (say you want a std::map with reader-writer lock)
  - `using RWLMap = siddiqsoft::RWLEnvelope<std::map>;`


```cpp
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "siddiqsoft/RWLEnvelope.hpp"


TEST(examples, AssignWithCallbacks)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> docl; // we will assign later
	nlohmann::json                          doc2 {{"baa", 0x0baa}, {"fee", 0x0fee}, {"bee", 0x0bee}};

	// Move assign here post init
	docl.reassign(std::move(doc2));
	// Must be empty since we moved it into the envelope
	EXPECT_TRUE(doc2.empty());

	// Check we have pre-change value.. Note that here we return a boolean to avoid data copy
	EXPECT_TRUE(docl.observe<bool>([](const auto& doc) -> bool {
		return (doc.value("fee", 0xfa17) == 0x0fee) && (doc.value("baa", 0xfa17) == 0x0baa) && (doc.value("bee", 0xfa17) == 0x0bee);
	}));

	EXPECT_EQ(3, docl.observe<size_t>([](const auto& doc) { return doc.size(); }));
}


TEST(examples, AssignWithDirectLocks)
{
	siddiqsoft::RWLEnvelope<nlohmann::json> docl({{"foo", "bar"}, {"few", "lar"}});
	nlohmann::json                          doc2 {{"baa", 0x0baa}, {"fee", 0x0fee}, {"bee", 0x0bee}};

	// Previous document has two items..
	if (auto const& [doc, rl] = docl.readLock(); rl) { EXPECT_EQ(2, doc.size()); }

	// Modify the item (replace the initial with new)
	if (auto [doc, wl] = docl.writeLock(); wl) { doc = std::move(doc2); };

	//doc2 -> Must be empty since we moved it into the envelope
	EXPECT_TRUE(doc2.empty());

	// Check we have post-change value..
	if (const auto& [doc, rl] = docl.readLock(); rl) { EXPECT_EQ(3, doc.size()); }
}
```

Additional [examples](tests/examples.cpp).

# Test Coverage

The library includes comprehensive test coverage across multiple categories:

## Basic Functionality Tests

- **Simple Operations**: Basic envelope creation and mutation
- **Callback-Based Access**: Testing `observe()` and `mutate()` methods with various return types
- **Direct Lock Access**: Testing `readLock()` and `writeLock()` with structured bindings
- **Reassignment**: Testing `reassign()` method for replacing envelope contents
- **Snapshot Operations**: Testing `snapshot()` for independent copies
- **Move Semantics**: Testing move constructors and move assignment

## Edge Case Tests

- **Default Construction**: Envelopes with default-constructed objects
- **Return Value Forwarding**: Callbacks returning various types (void, int, string, bool, size_t)
- **Non-JSON Types**: Testing with `std::vector<int>`, `std::string`, and other types
- **Move Constructor Behavior**: Verifying source envelope state after move operations
- **RWA Counter Tracking**: Validating the read-write-action counter accuracy
- **Exception Safety**: Testing behavior when callbacks throw exceptions
- **Independent Snapshots**: Verifying snapshots are truly independent copies
- **Multiple Reassignments**: Testing repeated reassignment operations

## Concurrency & Stress Tests

### Reader-Writer Contention
- **Two-Thread Tests**: Concurrent readers and writers with callbacks and direct locks
- **Monotonic Counter Integrity**: Verifying counter never goes backwards under concurrent access
- **Snapshot Consistency**: Ensuring snapshots return internally consistent state
- **Concurrent Reassign**: Testing reassign racing with observe and snapshot operations

### High-Contention Scenarios
- **Zero-Sleep Maximum Contention**: All threads hammer the lock without delays
- **Mixed API Concurrency**: All 5 API methods used concurrently on the same envelope
- **Shared Read Lock Concurrency**: Multiple readers accessing simultaneously without blocking
- **Concurrent Observe with Return**: Readers returning values under write contention

### Data Integrity Verification
- **RWA Counter Accuracy**: Verifying mutation counter matches exact mutate() count
- **Paired Field Consistency**: Ensuring related fields remain synchronized
- **Version-Data Pairing**: Validating version and data fields stay in sync during reassignment

## Test Statistics

- **Total Test Cases**: 20+ comprehensive test cases
- **Concurrency Levels**: Tests with up to 16 concurrent reader threads and 8 writer threads
- **Iteration Counts**: Stress tests with 5,000-10,000 iterations per thread
- **Coverage Areas**:
  - ✅ All public API methods
  - ✅ Thread safety guarantees
  - ✅ Lock semantics (shared vs. exclusive)
  - ✅ Exception safety
  - ✅ Move semantics
  - ✅ Data consistency under contention
  - ✅ Return value forwarding
  - ✅ JSON serialization (when available)

## Running Tests

Tests are built using Google Test (gtest) and can be run via the CMake build system:

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
```

Coverage reports are generated and tracked via Azure Pipelines CI/CD.

<small align="right">

&copy; 2021 Siddiq Software LLC. All rights reserved.

</small>
