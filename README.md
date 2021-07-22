RWLEnvelope : A simple read-writer lock envelope
-------------------------------------------

[![CodeQL](https://github.com/SiddiqSoft/RWLEnvelope/actions/workflows/codeql-analysis.yml/badge.svg)](https://github.com/SiddiqSoft/RWLEnvelope/actions/workflows/codeql-analysis.yml)
[![Build Status](https://dev.azure.com/siddiqsoft/siddiqsoft/_apis/build/status/SiddiqSoft.rwlenvelope?branchName=main)](https://dev.azure.com/siddiqsoft/siddiqsoft/_build/latest?definitionId=7&branchName=main)
![](https://img.shields.io/nuget/v/SiddiqSoft.RWLEnvelope)
![](https://img.shields.io/github/v/tag/SiddiqSoft/RWLEnvelope)
![](https://img.shields.io/azure-devops/tests/siddiqsoft/siddiqsoft/7)
![](https://img.shields.io/azure-devops/coverage/siddiqsoft/siddiqsoft/7)

# Objective
- Avoid re-implementing the rw-lock; standard C++ (since C++14) has a good reader-writer lock implementation.
- Provide a simple, convenience layer atop the underlying [`std::unique_lock`](https://en.cppreference.com/w/cpp/thread/unique_lock) and [`std::shared_lock`](https://en.cppreference.com/w/cpp/thread/shared_lock) access to some type.

<p align="right" width="50%">
<b>WE DO NOT IMPLEMENT</b> a read-writer lock; the standard C++ library has one.<br/>We provide a header-only package simplifying the locking code around thread-safe access to your underlying type.
<br/>
<i>NOT a wrapper; an envelope.</i>
</p>

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


<small align="right">

&copy; 2021 Siddiq Software LLC. All rights reserved.

</small>
